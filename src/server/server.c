/**
 * server.c - muxkit 服务端实现
 *
 * 本模块实现了 muxkit 服务端守护进程的核心功能：
 * - 守护进程创建 (double-fork 模式)
 * - Unix 域套接字监听和客户端连接管理
 * - 会话 (session) 生命周期管理
 * - PTY 创建和 shell 进程管理
 * - 多窗格 (pane) 支持
 * - 会话分离/附加功能
 * - SIGCHLD 信号处理和子进程回收
 *
 * 消息协议：
 *   MSG_COMMAND      - 执行命令 (new-session, pane-split)
 *   MSG_RESIZE       - 调整终端尺寸
 *   MSG_DETACH       - 分离/附加会话
 *   MSG_LIST_SESSIONS - 列出所有会话
 *   MSG_DETACHKILL   - 终止指定会话
 *   MSG_EXITED       - 客户端退出通知
 *   MSG_GRID_SAVE    - 保存屏幕网格数据
 *
 * MIT License
 * Copyright (c) 2024 LatosProject
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _XOPEN_SOURCE 700
#include <string.h>
#include "i18n.h"
#include "list.h"
#include "log.h"
#include "main.h"
#include "muxkit-protocol.h"
#include "server.h"
#include "spawn.h"
#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
extern char *socket_path;
struct list_head session_list;
static volatile sig_atomic_t sigchld_pending = 0;
ssize_t read_n(int fd, void *buf, size_t n) {
  size_t recvd = 0;
  char *p = buf;
  while (recvd < n) {
    ssize_t r = read(fd, p + recvd, n - recvd);
    if (r == 0)
      return 0; // EOF
    if (r == -1) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    recvd += r;
  }
  return recvd;
}

/*
  服务端信号处理器
*/
void server_signal_handler(int sig) {
  switch (sig) {
  case SIGCHLD:
    sigchld_pending = 1; // 主循环处理
    break;
  }
}

/*
  会话初始化
*/
void session_init(struct session *s) {
  s->id = -1;
  s->client_fd = -1;
  s->pane_count = 0;
  for (int i = 0; i < MAX_PANES; i++) {
    s->master_fds[i] = -1;
    s->pane_pids[i] = -1;
  }
  s->slave_fd = -1;
  s->slave_pid = -1;
  s->child_exited = 0;
  s->detached = 0;
  for (int i = 0; i < MAX_PANES; i++) {
    s->master_fds[i] = -1;
    s->pane_pids[i] = -1;
    s->grid_data[i] = NULL;
    s->grid_data_len[i] = 0;
  }
  list_init(&s->link);
  tcgetattr(STDIN_FILENO, &(s->orig_termios));
  ioctl(STDIN_FILENO, TIOCGWINSZ, &(s->ws));
}

/*
  根据 client_fd 查找 session
*/
static struct session *find_session_by_client_fd(int fd) {
  struct session *sess;
  list_for_each_entry(sess, &session_list, link) {
    if (sess->client_fd == fd) {
      return sess;
    }
  }
  return NULL;
}

/*
  根据 session id 查找 session
*/
static struct session *find_session_by_id(int id) {
  struct session *sess;
  list_for_each_entry(sess, &session_list, link) {
    if (sess->id == id) {
      return sess;
    }
  }
  return NULL;
}

/*
  处理来自客户端的消息
*/
int server_receive(int fd) {
  // 读取消息类型
  struct msg_header hdr;
  if (read_n(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    log_error("read header failed: %s", strerror(errno));
    return -1;
  }

  // 读取消息体
  char *buf = NULL;
  if (hdr.len > 0) {
    buf = malloc(hdr.len);
    if (read_n(fd, buf, hdr.len) != hdr.len) {
      log_error("read payload failed: %s", strerror(errno));
      free(buf);
      return -1;
    }
  }

  if (hdr.type == MSG_VERSION) {
    int server_version = PROTOCOL_VERSION;
    int *client_version = (int *)buf;
    if (server_version != *client_version) {
      log_error("protocol version mismatch: client=%d, server=%d",
                server_version, *client_version);
      free(buf);
      close(fd);
      return -1;
    }
    write(fd, &server_version, sizeof(server_version));
    free(buf);
    return 1;
  }
  // 列出会话列表
  if (hdr.type == MSG_LIST_SESSIONS) {
    char response[MUXKIT_BUF_XLARGE] = {0};
    int offset = 0;
    struct session *s;
    int count = 0;

    list_for_each_entry(s, &session_list, link) {
      // 列出有效会话
      if (s->slave_pid > 0) {
        count++;
        const char *status = s->detached ? "detached" : "attached";
        offset += snprintf(response + offset, sizeof(response) - offset,
                           "%d: %s (pid %d)\n", s->id, status, s->slave_pid);
      }
    }

    if (count == 0) {
      snprintf(response, sizeof(response), "%s", TR(MSG_NO_SESSIONS));
    }

    size_t len = strlen(response) + 1;
    write(fd, &len, sizeof(len));
    write(fd, response, len);

    log_info("listed %d sessions", count);
    free(buf);
    return -1; // 关闭连接
  }

  // 杀死指定会话
  if (hdr.type == MSG_DETACHKILL) {
    char response[MUXKIT_BUF_MEDIUM] = {0};
    int session_id;
    memcpy(&session_id, buf, sizeof(session_id));

    struct session *target = find_session_by_id(session_id);
    if (target && target->pane_count > 0) {
      log_info("killing session id=%d", target->id);
      // 杀死会话窗格
      for (int i = 0; i < target->pane_count; i++) {
        if (target->pane_pids[i] > 0) {
          kill(target->pane_pids[i], SIGKILL);
        }
        if (target->master_fds[i] >= 0) {
          close(target->master_fds[i]);
        }
      }
      if (target->slave_fd >= 0)
        close(target->slave_fd);
      if (target->client_fd >= 0)
        close(target->client_fd);
      list_del(&target->link);
      free(target);
      snprintf(response, sizeof(response), TR(MSG_SESSION_KILLED), session_id);
    } else {
      log_warn("kill-session failed: session %d not found", session_id);
      snprintf(response, sizeof(response), TR(MSG_SESSION_NOT_FOUND),
               session_id);
    }

    size_t len = strlen(response) + 1;
    write(fd, &len, sizeof(len));
    write(fd, response, len);

    free(buf);
    return -1; // 关闭连接
  }

  // 消息类型需要关联 session
  struct session *cur = find_session_by_client_fd(fd);

  // 如果没找到，说明是新连接，创建新 session
  if (cur == NULL) {
    cur = malloc(sizeof(struct session));
    session_init(cur);
    cur->client_fd = fd;

    // 设置 session id
    if (list_empty(&session_list)) {
      cur->id = 0;
    } else {
      struct session *last =
          list_last_entry(&session_list, struct session, link);
      cur->id = last->id + 1;
    }
    list_add_tail(&cur->link, &session_list);
    log_debug("created new session id=%d for fd=%d", cur->id, fd);
  }

  // 判断消息类型
  switch (hdr.type) {
  // 处理命令
  case MSG_COMMAND:
    if (strcmp(buf, "new-session") == 0 || strcmp(buf, "pane-split") == 0) {
      // 检查 pane 数量限制
      if (cur->pane_count >= MAX_PANES) {
        log_error("max panes reached");
        free(buf);
        return 1;
      }

      // 创建伪终端
      int new_master_fd = posix_openpt(O_RDWR);
      if (new_master_fd == -1) {
        log_error("posix_openpt failed: %s", strerror(errno));
        _exit(-1);
      }
      // 解锁 slave 设备
      grantpt(new_master_fd);
      unlockpt(new_master_fd);

      // 传回 client
      send_fd(fd, new_master_fd);
      cur->slave_name = ptsname(new_master_fd);
      cur->slave_fd = open(cur->slave_name, O_RDWR);
      ioctl(cur->slave_fd, TIOCSWINSZ, &cur->ws);

      log_info("create pane %d for session id:%d", cur->pane_count, cur->id);

      cur->slave_pid = spawn_child(cur);

      /* 父进程关闭 slave_fd，否则 shell 退出后 master 不会收到 EOF */
      close(cur->slave_fd);
      cur->slave_fd = -1;

      if (cur->slave_pid < 0) {
        log_error("spawn_child failed");
        close(new_master_fd);
        _exit(-1);
      }

      // 保存到数组
      cur->master_fds[cur->pane_count] = new_master_fd;
      cur->pane_pids[cur->pane_count] = cur->slave_pid;
      cur->pane_count++;

      log_info("spawned child process with pid %d, total panes: %d",
               cur->slave_pid, cur->pane_count);
    }
    free(buf);
    return 1;
  case MSG_RESIZE:
    log_debug("resize session");
    if (cur == NULL) {
      log_warn("MSG_RESIZE: session not found for fd %d", fd);
      free(buf);
      return 1;
    }
    // 只保存整体尺寸，不给 PTY 发 TIOCSWINSZ
    // （client 负责给每个 pane 发送正确的尺寸）
    memcpy(&cur->ws, buf, sizeof(cur->ws));
    free(buf);
    return 1;
  case MSG_EXITED:
    log_info("exit a session, pid:%s", buf);
    struct session *sess;
    list_for_each_entry(sess, &session_list, link) {
      log_info("session id=%d, pid=%d", sess->id, sess->slave_pid);
    }
    free(buf);
    return -1;
    break;
  case MSG_DETACH:
    if (hdr.len == 0) {
      log_info("detach a session");
      sess = NULL;
      sess = find_session_by_client_fd(fd);
      if (sess) {
        sess->detached = 1;
        log_debug("session id=%d marked as detached", sess->id);
      }
    } else {
      // attach: 客户端发送的是二进制 int
      int session_id;
      memcpy(&session_id, buf, sizeof(session_id));
      struct session *target = find_session_by_id(session_id);
      if (target && target->detached) {
        log_debug("attaching to detached session id=%d, pane_count=%d",
                  target->id, target->pane_count);
        // 先发送 pane 数量
        write(fd, &target->pane_count, sizeof(int));
        // 再发送所有 pane 的 fd
        for (int i = 0; i < target->pane_count; i++) {
          send_fd(fd, target->master_fds[i]);
        }
        // 统计并发送 grid 数量
        int grid_count = 0;
        for (int i = 0; i < target->pane_count; i++) {
          if (target->grid_data[i] && target->grid_data_len[i] > 0)
            grid_count++;
        }
        log_info("attach: pane_count=%d, grid_count=%d", target->pane_count,
                 grid_count);
        for (int i = 0; i < target->pane_count; i++) {
          log_info("attach: grid_data[%d]=%p, len=%zd", i, target->grid_data[i],
                   target->grid_data_len[i]);
        }
        write(fd, &grid_count, sizeof(grid_count));
        for (int i = 0; i < target->pane_count; i++) {
          if (target->grid_data[i] && target->grid_data_len[i] > 0) {
            struct msg_header gh = {MSG_GRID_SAVE, target->grid_data_len[i]};
            log_info("attach: sending grid header type=%d, len=%zu", gh.type,
                     gh.len);
            ssize_t hdr_written = write(fd, &gh, sizeof(gh));
            log_info("attach: header write returned %zd", hdr_written);
            ssize_t data_written =
                write(fd, target->grid_data[i], target->grid_data_len[i]);
            log_info("attach: data write returned %zd (expected %zd)",
                     data_written, target->grid_data_len[i]);
            free(target->grid_data[i]);
            target->grid_data[i] = NULL;
            target->grid_data_len[i] = 0;
          }
        }
        target->client_fd = fd;
        target->detached = 0;
      } else {
        log_warn("attach failed: session %d not found or not detached",
                 session_id);
        // 发送失败标记：pane_count = 0
        int zero = 0;
        write(fd, &zero, sizeof(int));
      }
    }
    free(buf);
    return 1; // 返回 1，让 detach 处理代码来关闭 fd
  case MSG_GRID_SAVE:
    sess = find_session_by_client_fd(fd);
    log_info("MSG_GRID_SAVE: sess=%p, fd=%d", (void *)sess, fd);
    if (sess) {
      unsigned int pane_id;
      memcpy(&pane_id, buf, sizeof(pane_id));
      log_info("MSG_GRID_SAVE: pane_id=%u, len=%zu", pane_id, hdr.len);
      if (pane_id < MAX_PANES) {
        free(sess->grid_data[pane_id]);
        sess->grid_data[pane_id] = buf;
        sess->grid_data_len[pane_id] = hdr.len;
        buf = NULL;
        log_info("MSG_GRID_SAVE: stored at grid_data[%u]", pane_id);
      }
    }
    free(buf);
    return 1;
  default:
    log_warn("unknown msgtype %d", hdr.type);
  }

  free(buf);
  return 1;
}

/*
  服务器主循环，监听客户端连接请求
*/
void server_loop(int listen_fd) {
  log_info("server loop started, listening on fd %d", listen_fd);

  // 在循环开始前设置信号处理器
  struct sigaction sa;
  sa.sa_handler = server_signal_handler;
  sa.sa_flags = 0; // 不用 SA_RESTART，让 select 被信号打断
  sigemptyset(&sa.sa_mask);
  sigaction(SIGCHLD, &sa, NULL);

  fd_set read_fds;
  int max_fd;
  int client_fds[MAX_CLIENTS] = {-1};
  // 初始化客户端 fd 数组
  for (int i = 0; i < MAX_CLIENTS; i++) {
    client_fds[i] = -1;
  }
  while (1) {
    FD_ZERO(&read_fds);
    FD_SET(listen_fd, &read_fds); // 添加监听 fd
    max_fd = listen_fd;

    // 当client_fds不为空时，把client_fds加入监听集合
    // 添加所有已连接的客户端 fd
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (client_fds[i] >= 0) {
        FD_SET(client_fds[i], &read_fds);
        if (client_fds[i] > max_fd) {
          max_fd = client_fds[i];
        }
      }
    }

    // 阻塞，等待 fd 可读
    int select_ok = 1;
    if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
      if (errno == EINTR) {
        select_ok = 0; // 不 continue，让后续代码检查 sigchld_pending
      } else {
        log_error("select failed: %s", strerror(errno));
        break;
      }
    }

    // 只有 select 成功时才处理 fd
    if (select_ok) {
      // 检查监听 fd是否可读，有新客户端连接
      if (FD_ISSET(listen_fd, &read_fds)) {
        int new_fd = accept(listen_fd, NULL, NULL);
        if (new_fd >= 0) {
          for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] == -1) {
              client_fds[i] = new_fd;
              break;
            }
          }
        }
      }

      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] >= 0 &&
            FD_ISSET(client_fds[i],
                     &read_fds)) { // 只处理内核提供的可读的 fd
          // 客户端断开连接则关闭 fd
          if (server_receive(client_fds[i]) < 0) {
            close(client_fds[i]);
            client_fds[i] = -1;
          }
        }
      }
    }

    // 处理 detach 的 session
    struct session *sess;
    list_for_each_entry(sess, &session_list, link) {
      if (sess->detached == 1) {
        // 先从 client_fds 数组中移除(此时 sess->client_fd 还保存着旧值)
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (client_fds[i] == sess->client_fd) {
            client_fds[i] = -1; // 清空槽位,防止 fd 复用时冲突
            break;
          }
        }

        // 关闭客户端连接(但保持 PTY 和 shell 继续运行)
        close(sess->client_fd);
        sess->client_fd = -1; // 标记 session 已没有客户端连接

        log_info("session %d detached, shell continues running", sess->id);
      }
    }

    // 无论 select 是否成功，都检查 sigchld_pending
    if (sigchld_pending) {
      sigchld_pending = 0;
      // 回收所有退出的子进程
      int status;
      pid_t pid;
      while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        struct session *sess;
        list_for_each_entry(sess, &session_list, link) {
          // 检查是否是这个 session 的某个 pane
          for (int i = 0; i < sess->pane_count; i++) {
            if (sess->pane_pids[i] == pid) {
              log_info("pane %d (pid %d) exited in session %d", i, pid,
                       sess->id);
              // 关闭这个 pane 的 master_fd
              if (sess->master_fds[i] >= 0) {
                close(sess->master_fds[i]);
                sess->master_fds[i] = -1;
              }
              sess->pane_pids[i] = -1;

              // 检查是否所有 pane 都退出了
              int all_exited = 1;
              for (int j = 0; j < sess->pane_count; j++) {
                if (sess->pane_pids[j] > 0) {
                  all_exited = 0;
                  break;
                }
              }
              if (all_exited) {
                sess->child_exited = 1;
                // 关闭 client 连接，通知 client 退出
                if (sess->client_fd >= 0) {
                  // 同步清理 client_fds 数组
                  for (int k = 0; k < MAX_CLIENTS; k++) {
                    if (client_fds[k] == sess->client_fd) {
                      client_fds[k] = -1;
                      break;
                    }
                  }
                  close(sess->client_fd);
                  sess->client_fd = -1;
                }
              }
              break;
            }
          }
        }
      }
      // 安全删除已退出的 session
      struct session *sess, *tmp;
      list_for_each_entry_safe(sess, tmp, &session_list, link) {
        if (sess->child_exited) {
          log_info("cleaning up session id=%d", sess->id);
          list_del(&sess->link);
          free(sess);
        }
      }
    }
  }
}

/*
  服务器启动函数，返回连接到服务器的客户端socket fd
*/
int server_start() {
  // 初始化 session 链表
  list_init(&session_list);
  sigset_t set, oldset;
  log_info("server is starting");

  // 创建 unix 套接字，用于客户端连接
  int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listen_fd == -1) {
    log_error("socket failed: %s", strerror(errno));
    return -1;
  }
  struct sockaddr_un sa;
  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  strncpy(sa.sun_path, socket_path, sizeof(sa.sun_path) - 1);

  // 绑定 socket
  if (bind(listen_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
    log_error("bind failed: %s", strerror(errno));
    close(listen_fd);
    return -1;
  }
  log_debug("bound to %s", socket_path);

  // 监听客户端连接
  if (listen(listen_fd, MUXKIT_LISTEN_BACKLOG) == -1) {
    log_error("listen failed: %s", strerror(errno));
    close(listen_fd);
    return -1;
  }

  // 阻塞所有信号，防止 fork 出错
  sigfillset(&set);
  sigprocmask(SIG_BLOCK, &set, &oldset);

  // fork 出守护进程
  pid_t pid = fork();
  if (pid < 0) {
    log_error("fork failed: %s", strerror(errno));
    sigprocmask(SIG_SETMASK, &oldset, NULL);
    close(listen_fd);
    return -1;
  }
  if (pid == 0) {
    // 服务端守护进程
    // 创建新会话，脱离控制终端
    if (setsid() == -1) {
      log_error("setsid failed: %s", strerror(errno));
      _exit(1);
    }

    // 二次fork，防止进程重新获得控制终端
    pid_t pid2 = fork();
    if (pid2 < 0) {
      _exit(1);
    }

    if (pid2 > 0) {
      // 第一个子进程退出，让子进程成为真正的守护进程
      _exit(0);
    }

    // 设置文件权限掩码，确保读写权限
    umask(0);

    // 关闭标准输入输出，重定向到 /dev/null
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY); // stdin  -> fd 0
    open("/dev/null", O_WRONLY); // stdout -> fd 1
    open("/dev/null", O_WRONLY); // stderr -> fd 2

    // 恢复信号掩码
    sigprocmask(SIG_SETMASK, &oldset, NULL);

    // 服务器启动完毕
    log_init("server");
    log_info("server daemon started, pid %d", getpid());

    // 进入服务器主循环
    server_loop(listen_fd);
    close(listen_fd);
    log_close();
    _exit(0);
  } else {

    // 等待第一个子进程退出（它会立即退出，子进程继续运行）
    waitpid(pid, NULL, 0);

    // 恢复parent进程信号掩码
    sigprocmask(SIG_SETMASK, &oldset, NULL);
    close(listen_fd);

    // 连接到刚创建的 server
    // 只用于获取 child2 的 fd
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd == -1) {
      log_error("client socket failed: %s", strerror(errno));
      return -1;
    }
    if (connect(client_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
      log_error("client connect to new server failed: %s", strerror(errno));
      close(client_fd);
      return -1;
    }
    log_debug("connected to server, fd %d", client_fd);
    return client_fd; // 获取 child2 的fd，返回到 client 进程
  }
}
