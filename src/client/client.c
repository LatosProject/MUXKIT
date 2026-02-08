/**
 * client.c - muxkit 客户端实现
 *
 * 本模块实现了 muxkit 客户端的核心功能：
 * - 有限状态机 (FSM) 驱动的事件处理
 * - Unix 域套接字连接管理
 * - 终端原始模式切换
 * - PTY 数据读写和渲染
 * - 信号处理 (SIGWINCH, SIGCHLD)
 * - 会话分离/附加
 * - 多窗格支持
 *
 * 状态机状态：
 *   ST_BOOT -> ST_RUNNING -> ST_EXITING
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

#include "list.h"
#include "muxkit-protocol.h"
#include "render.h"
#include "window.h"
#define _GNU_SOURCE
#include "client.h"
#include "i18n.h"
#include "input.h"
#include "keyboard.h"
#include "log.h"
#include "main.h"
#include "server.h"
#include "util.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
int server_fd;
extern char *socket_path;
volatile sig_atomic_t sigwinch_pending,
    sigchld_pending = 0; // C 语言唯一保证信号读写安全的类型
static const state_transition table[] = {
    {ST_BOOT, EV_ENABLE_RAW_MODE, ST_RUNNING, act_enable_raw_mode},
    {ST_RUNNING, EV_WINCH, ST_RUNNING, act_resize},
    {ST_RUNNING, EV_CHLD_EXIT, ST_EXITING, act_child_exit},
    {ST_RUNNING, EV_PTY_READ, ST_RUNNING, act_pty_read},
    {ST_RUNNING, EV_STDIN_READ, ST_RUNNING, act_stdin_read},
    {ST_EXITING, EV_STDIN_READ, ST_EXITING, NULL},
    {ST_EXITING, EV_PTY_READ, ST_EXITING, NULL},
    {ST_RUNNING, EV_EOF_PTY, ST_EXITING, act_child_exit},
    {ST_RUNNING, EV_EOF_STDIN, ST_EXITING, NULL},
    {ST_RUNNING, EV_INTERRUPT, ST_EXITING, NULL},
    {ST_RUNNING, EV_DETACHED, ST_EXITING, act_detach},
    {ST_RUNNING, EV_PANE_SPLIT, ST_RUNNING, act_pane_split}};

#define NTRANS (sizeof(table) / sizeof(table[0]))

void dispatch_event(struct client *c, client_event ev) {
  for (size_t i = 0; i < NTRANS; i++) {
    if (table[i].state == c->state && table[i].event == ev) {
      if (table[i].action) {
        table[i].action(c, ev);
      }

      c->state = table[i].next;
      return;
    }
  }
  log_warn("FSM unhandled event %d in state %d", ev, c->state);
}
static int client_get_lock(char *lockfile) {
  int lockfd;
  log_debug("lock file is %s", lockfile);

  if ((lockfd = open(lockfile, O_RDWR | O_CREAT, 0600)) == -1) {
    log_error("open lock file failed: %s", strerror(errno));
    return -1;
  }

  if (flock(lockfd, LOCK_EX | LOCK_NB) == -1) {
    log_debug("flock failed: %s", strerror(errno));
    if (errno != EAGAIN)
      return lockfd;
    // 信号阻塞等待
    while (flock(lockfd, LOCK_EX) == -1 && errno == EINTR)
      ;
    close(lockfd);
    return -2;
  }
  log_debug("flock succeeded");
  return lockfd;
}

static int client_connect(const char *path) {
  struct sockaddr_un sa;
  int fd, lockfd = -1;
  int locked = 0;
  char buf[MUXKIT_BUF_SMALL] = {0};
  char *lockfile = NULL;

  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  strlcpy(sa.sun_path, path, sizeof(sa.sun_path));

  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    log_error("socket failed: %s", strerror(errno));
    return -1;
  }
  log_debug("socket path is %s", path);
  log_debug("trying connect");
  if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
    log_debug("connect failed: %s", strerror(errno));
    close(fd);
    if (!locked) {
      snprintf(buf, sizeof(buf), "%s.lock", path);
      lockfile = buf;
      if ((lockfd = client_get_lock(lockfile)) < 0) {
        log_debug("didn't get lock %d", lockfd);
      }
    }
    // lock标识符存在，无法删除目录项，且失败原因不是文件夹不存在
    if (lockfd >= 0 && unlink(path) != 0 && errno != ENOENT) {
      close(lockfd);
      return -1;
    }
    log_debug("got lock %d", lockfd);
    fd = server_start();
  } else {
    log_debug("connected sucessfully");
  }
  if (locked && lockfd >= 0) {
    close(lockfd);
  }
  return fd;
}

int send_server(enum msgtype type, int fd, const void *buf, size_t len) {
  struct msg_header hdr = {type, len};
  ssize_t n;
  const char *ph = (const char *)&hdr;
  // 发送消息头
  size_t sent = 0;
  while (sent < sizeof(hdr)) {
    ssize_t n = write(fd, ph + sent, sizeof(hdr) - sent);
    if (n == -1) {
      if (errno == EINTR)
        continue; // 被信号打断，重试
      return -1;
    }
    sent += n;
  }

  // 发送数据
  const char *p = buf;
  sent = 0;
  while (sent < len) {
    ssize_t n = write(fd, p + sent, len - sent);
    if (n == -1) {
      if (errno == EINTR)
        continue; // 被信号打断，重试
      return -1;
    }
    sent += n;
  }
  return 0;
}

void act_resize(struct client *c, client_event ev) {
  // 设置终端尺寸
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &(c->ws)) == -1) {
    return;
  }
  struct winsize ws_pane = c->ws;
  ws_pane.ws_row -= 1;

  unsigned int new_height = c->ws.ws_row - 1; // 留一行给状态栏
  unsigned int new_width = c->ws.ws_col;

  // 遍历所有 pane，计算新尺寸
  struct window_pane *p;
  int pane_count = 0;
  list_for_each_entry(p, &c->pane->window->panes, link) { pane_count++; }
  unsigned int pane_width = (new_width - pane_count + 1) / pane_count;
  unsigned int x_offset = 0;

  // 调整 pane 结构大小，并通知每个 PTY 正确的尺寸
  list_for_each_entry(p, &c->pane->window->panes, link) {
    pane_resize(p, pane_width, new_height);
    p->xoff = x_offset;
    x_offset += pane_width + 1;

    // 通知 PTY 这个 pane 的实际尺寸
    struct winsize ws = {.ws_row = new_height, .ws_col = pane_width};
    ioctl(p->master_fd, TIOCSWINSZ, &ws);
  }

  // 清屏并移动光标到左上角
  write(STDOUT_FILENO, "\033[2J\033[H", 7);

  // 重新渲染所有 pane 和边框
  list_for_each_entry(p, &c->pane->window->panes, link) {
    render_pane(p);
    if (p->link.next != &c->pane->window->panes) {
      render_pane_borders(p);
    }
  }
  render_status_bar(c);

  // 通知 server 保存整体尺寸（但 server 不再给 PTY 发 TIOCSWINSZ）
  send_server(MSG_RESIZE, c->server_fd, &ws_pane, sizeof(ws_pane));
  return;
}

void act_child_exit(struct client *c, client_event ev) {
  c->child_exited = 1;
  // 切换回主屏幕缓冲区
  write(STDOUT_FILENO, "\033[?1049l", 8);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &(c->orig_termios));
}

void act_enable_raw_mode(struct client *c, client_event ev) {
  // 原始终端切换至 raw 模式
  tcgetattr(STDIN_FILENO, &(c->raw));
  c->raw.c_lflag &= ~(ECHO | ICANON | ISIG); // 关掉回显、立即读取、禁用SIGINT
  c->raw.c_iflag &=
      ~ICRNL; // 禁用 CR->NL 转换，否则 Enter(\r) 会变成 \n (Ctrl+J)
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &(c->raw));
}

void act_pty_read(struct client *c, client_event ev) {
  char buff[MUXKIT_BUF_XLARGE];
  ssize_t n = read(c->master_fd, buff, sizeof(buff));
  if (n <= 0) {
    dispatch_event(c, EV_EOF_PTY);
    return;
  }
  pane_input(c->pane, buff, n);
  render_status_bar(c);
  render_pane(c->pane);
}

void act_stdin_read(struct client *c, client_event ev) {
  char buff[MUXKIT_BUF_XLARGE];
  ssize_t n = read(STDIN_FILENO, buff, sizeof(buff));
  if (n <= 0) {
    dispatch_event(c, EV_EOF_STDIN);
    return;
  }

  static int ctrl_b_pressed = 0;

  for (ssize_t i = 0; i < n; i++) {
    if (buff[i] == 0x02) { // ctrl+b
      if (ctrl_b_pressed) {
        // Ctrl+B + Ctrl+B = 发送一个真正的 Ctrl+B 到 PTY
        write(c->pane->master_fd, &buff[i], 1);
      }
      ctrl_b_pressed = 1;
      continue;
    }
    if (ctrl_b_pressed) {
      enum key_table table = KEY_PREFIX;
      handle_key(c, table, buff[i]);
      ctrl_b_pressed = 0;
    } else {
      // 如果正在查看历史，非 Ctrl+B 按键退出历史模式
      if (c->pane->grid->scroll_offset > 0) {
        c->pane->grid->scroll_offset = 0;
        render_pane(c->pane);
        // 如果是 Esc 或 q，不发送到 shell
        if (buff[i] == 0x1b || buff[i] == 'q') {
          continue;
        }
      }
      write(c->pane->master_fd, &buff[i], 1);
    }
  }
}

void act_detach(struct client *c, client_event ev) {
  struct window_pane *p;
  list_for_each_entry(p, &c->pane->window->panes, link) {
    void *buf;
    size_t n = grid_serialize(p->grid, p->id, p->cx, p->cy, &buf);
    if (n > 0) {
      send_server(MSG_GRID_SAVE, server_fd, buf, n);
      free(buf);
    }
  }
  send_server(MSG_DETACH, server_fd, NULL, 0);
  c->child_exited = 1;
  // 切换回主屏幕缓冲区
  write(STDOUT_FILENO, "\033[?1049l", 8);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &(c->orig_termios));
}

/*
  服务端信号处理器
  注意: 信号可能会打断运行的状态，导致竞态关系，请使用原子操作
*/
void signal_handler(int sig) {
  extern struct client client;
  int status;
  pid_t ret;
  switch (sig) {
  case SIGWINCH:
    sigwinch_pending = 1;
    break;
  // 回收子进程
  case SIGCHLD:
    ret = waitpid(client.slave_pid, &status, WNOHANG);
    if (ret > 0) {
      sigchld_pending = 1;
    }
    break;
  }
}

void act_pane_split(struct client *c, client_event ev) {
  struct window_pane *p;

  // 统计现有 pane 数量
  int pane_count = 0;
  list_for_each_entry(p, &c->pane->window->panes, link) { pane_count++; }

  // 计算分割后每个 pane 的宽度
  unsigned int total_width = c->ws.ws_col;
  unsigned int total_height = c->pane->sy;
  int new_pane_count = pane_count + 1;
  // 总宽度减去边框数量(new_pane_count - 1)，再平分
  unsigned int pane_width =
      (total_width - (new_pane_count - 1)) / new_pane_count;

  // 先发送新 pane 的尺寸给 server
  struct winsize new_ws = {.ws_row = total_height, .ws_col = pane_width};
  send_server(MSG_RESIZE, server_fd, &new_ws, sizeof(new_ws));

  char buf[MUXKIT_BUF_SMALL] = "pane-split";
  send_server(MSG_COMMAND, server_fd, buf, strlen(buf) + 1);
  int new_fd = recv_fd(server_fd);
  if (new_fd == -1) {
    log_error("recv_fd failed");
    return;
  }

  // 调整所有现有 pane 的尺寸和位置
  unsigned int x_offset = 0;
  list_for_each_entry(p, &c->pane->window->panes, link) {
    pane_resize(p, pane_width, total_height);
    p->xoff = x_offset;
    x_offset += pane_width + 1; // +1 是边框

    // 通知 PTY 新尺寸
    struct winsize ws = {.ws_row = total_height, .ws_col = pane_width};
    ioctl(p->master_fd, TIOCSWINSZ, &ws);
  }

  // 创建新 pane
  struct window_pane *new_pane = pane_create(
      c->pane->window, pane_width, total_height, x_offset, c->pane->yoff);
  pane_set_master_fd(new_pane, new_fd);

  struct winsize ws = {.ws_row = new_pane->sy, .ws_col = new_pane->sx};
  ioctl(new_fd, TIOCSWINSZ, &ws);

  // 清屏并渲染所有 pane
  write(STDOUT_FILENO, "\033[2J", 4);
  render_status_bar(c);
  list_for_each_entry(p, &c->pane->window->panes, link) {
    render_pane(p);
    if (p->link.next != &c->pane->window->panes) {
      render_pane_borders(p);
    }
  }
}

/*
  客户端初始化
*/
void client_init(struct client *c) {
  c->state = ST_BOOT;
  c->server_fd = -1;
  c->master_fd = -1; // 子进程的父级
  c->slave_fd = -1;  // 子进程
  c->slave_pid = -1;
  c->child_exited = 0;

  tcgetattr(STDIN_FILENO, &(c->orig_termios));
  ioctl(STDIN_FILENO, TIOCGWINSZ, &(c->ws));
}

/*
  客户端循环处理
*/
void client_loop(struct client *c) {
  while (1) {
    if (c->child_exited)
      break;
    fd_set rfds;
    // 输入和输出
    int maxfd;
    FD_ZERO(&rfds);

    FD_SET(STDIN_FILENO, &rfds);
    FD_SET(c->server_fd, &rfds); // 监听 server 连接

    maxfd = c->master_fd > STDIN_FILENO ? c->master_fd : STDIN_FILENO;
    struct window_pane *p;
    list_for_each_entry(p, &c->pane->window->panes, link) {
      if (p->master_fd > 0) {
        FD_SET(p->master_fd, &rfds);
        if (p->master_fd > maxfd) {
          maxfd = p->master_fd;
        }
      }
    }
    if (c->server_fd > maxfd)
      maxfd = c->server_fd;

    int select_ok = 1;
    if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
      // 防止收到信号后中断 fd
      if (errno != EINTR) {
        dispatch_event(c, EV_INTERRUPT);
        log_error("select failed: %s", strerror(errno));
        break;
      }
      // fd 检查完毕
      select_ok = 0;
    }

    if (sigwinch_pending) {
      sigwinch_pending = 0;
      dispatch_event(c, EV_WINCH);
    }

    if (sigchld_pending) {
      sigchld_pending = 0;
      dispatch_event(c, EV_CHLD_EXIT);
    }

    // 只有 select 成功时才检查 fd
    if (select_ok) {
      // server 关闭连接，说明 session 结束
      if (FD_ISSET(c->server_fd, &rfds)) {
        char buf[1];
        if (read(c->server_fd, buf, 1) <= 0) {
          dispatch_event(c, EV_EOF_PTY);
        }
      }

      // 使用 safe 版本，因为可能在循环中删除 pane
      struct window_pane *tmp;
      int pane_removed = 0;
      list_for_each_entry_safe(p, tmp, &c->pane->window->panes, link) {
        if (p->master_fd >= 0 && FD_ISSET(p->master_fd, &rfds)) {
          char buff[MUXKIT_BUF_XLARGE];
          ssize_t n = read(p->master_fd, buff, sizeof(buff));
          if (n > 0) {
            pane_input(p, buff, n);
            render_pane(p);
            if (p->link.next != &c->pane->window->panes) {
              render_pane_borders(p);
            }
          } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) {
            // pane 的 shell 退出了
            close(p->master_fd);
            p->master_fd = -1;

            // 如果是当前活动 pane，切换到另一个
            if (c->pane == p) {
              struct window_pane *next =
                  list_entry(p->link.next, struct window_pane, link);
              if (&next->link == &c->pane->window->panes) {
                // 到达链表头，尝试前一个
                next = list_entry(p->link.prev, struct window_pane, link);
              }
              if (&next->link != &c->pane->window->panes) {
                c->pane = next;
              }
            }

            // 从链表移除并销毁
            list_del(&p->link);
            pane_destroy(p);
            pane_removed = 1;

            // 检查是否还有 pane
            if (list_empty(&c->pane->window->panes)) {
              c->child_exited = 1;
              break;
            }
          }
        }
      }

      // 如果有 pane 被移除，重新调整剩余 pane 的尺寸
      if (pane_removed && !c->child_exited) {
        unsigned int new_height = c->ws.ws_row - 1;
        unsigned int new_width = c->ws.ws_col;
        int pane_count = 0;
        list_for_each_entry(p, &c->pane->window->panes, link) { pane_count++; }
        unsigned int pane_width = (new_width - (pane_count - 1)) / pane_count;
        unsigned int x_offset = 0;

        list_for_each_entry(p, &c->pane->window->panes, link) {
          pane_resize(p, pane_width, new_height);
          p->xoff = x_offset;
          x_offset += pane_width + 1;
          struct winsize ws = {.ws_row = new_height, .ws_col = pane_width};
          ioctl(p->master_fd, TIOCSWINSZ, &ws);
        }

        // 清屏并重新渲染
        write(STDOUT_FILENO, "\033[2J", 4);
        render_status_bar(c);
        list_for_each_entry(p, &c->pane->window->panes, link) {
          render_pane(p);
          if (p->link.next != &c->pane->window->panes) {
            render_pane_borders(p);
          }
        }
      }

      render_status_bar(c);

      // 重新定位光标到当前活动 pane
      char cursor_buf[32];
      int clen = snprintf(cursor_buf, sizeof(cursor_buf), "\033[%u;%uH",
                          c->pane->yoff + c->pane->cy + 1,
                          c->pane->xoff + c->pane->cx + 1);
      write(STDOUT_FILENO, cursor_buf, clen);

      if (FD_ISSET(STDIN_FILENO, &rfds)) {
        dispatch_event(c, EV_STDIN_READ);
      }
    }
  }
}

int client_main(struct client *c) {
  log_init("client");
  log_info("client starting");
  keybind_init();
  server_fd = client_connect(socket_path);
  if (server_fd == -1) {
    log_error("client connect failed");
    return -1;
  }
  log_info("connected to server, fd %d", server_fd);
  // 保存 server 连接 fd
  c->server_fd = server_fd;

  extern int detached_session_id;
  extern int list_sessions;
  extern int kill_session_id;
  struct window *w = NULL;
  int client_version = PROTOCOL_VERSION;
  int server_version = 0;
  // 协议版本校验
  send_server(MSG_VERSION, server_fd, &client_version, sizeof(client_version));
  if (read(server_fd, &server_version, sizeof(server_version)) <= 0 ||
      server_version != PROTOCOL_VERSION) {
    write(STDOUT_FILENO, TR(MSG_ERR_PROTOCOL_VERSION),
          strlen(TR(MSG_ERR_PROTOCOL_VERSION)));
    close(server_fd);
    log_close();
    return 0;
  }

  // 列出所有 session
  if (list_sessions) {
    send_server(MSG_LIST_SESSIONS, server_fd, NULL, 0);
    // 读取响应
    size_t len;
    if (read(server_fd, &len, sizeof(len)) > 0 && len > 0) {
      char *response = malloc(len);
      if (read(server_fd, response, len) > 0) {
        printf("%s", response);
      }
      free(response);
    }
    close(server_fd);
    log_close();
    return 0;
  }

  // 杀死指定 session
  if (kill_session_id != -1) {
    send_server(MSG_DETACHKILL, server_fd, &kill_session_id,
                sizeof(kill_session_id));
    // 读取响应
    size_t len;
    if (read(server_fd, &len, sizeof(len)) > 0 && len > 0) {
      char *response = malloc(len);
      if (read(server_fd, response, len) > 0) {
        printf("%s", response);
      }
      free(response);
    }
    close(server_fd);
    log_close();
    return 0;
  }

  // attach 指定 session
  if (detached_session_id != -1) {
    send_server(MSG_DETACH, server_fd, &detached_session_id,
                sizeof(detached_session_id));
    // attach: 先读取 pane 数量
    int pane_count = 0;
    if (read(server_fd, &pane_count, sizeof(int)) <= 0 || pane_count <= 0) {
      char buff[MUXKIT_BUF_SMALL] = {0};
      snprintf(buff, sizeof(buff), TR(MSG_ATTACH_FAILED), detached_session_id);
      write(STDOUT_FILENO, buff, strlen(buff));
      log_warn("attach failed: session %d not found or not detached",
               detached_session_id);
      return 0;
    }

    log_info("attaching to session with %d panes", pane_count);

    // 创建 window
    w = window_create(TR(MSG_WINDOW_ATTACHED));
    c->ws.ws_row -= 1;

    // 计算每个 pane 的宽度
    unsigned int pane_width = (c->ws.ws_col - (pane_count - 1)) / pane_count;
    unsigned int x_offset = 0;

    // 接收所有 pane 的 fd 并创建 pane
    for (int i = 0; i < pane_count; i++) {
      int fd = recv_fd(server_fd);
      if (fd == -1) {
        log_error("recv_fd failed for pane %d", i);
        continue;
      }
      struct window_pane *p =
          pane_create(w, pane_width, c->ws.ws_row, x_offset, 0);
      pane_set_master_fd(p, fd);

      // 通知 PTY 新尺寸
      struct winsize ws = {.ws_row = c->ws.ws_row, .ws_col = pane_width};
      ioctl(fd, TIOCSWINSZ, &ws);

      if (i == 0) {
        c->pane = p;
        c->master_fd = fd;
      }
      x_offset += pane_width + 1;
    }
    // 读取 grid 数量
    int grid_count = 0;
    read(server_fd, &grid_count, sizeof(grid_count));
    log_info("client attach: received grid_count=%d", grid_count);

    for (int i = 0; i < grid_count; i++) {
      struct msg_header gh;
      ssize_t hdr_read = read(server_fd, &gh, sizeof(gh));
      log_info("client attach: read header, got %zd bytes, type=%d, len=%zu",
               hdr_read, gh.type, gh.len);
      if (hdr_read == sizeof(gh) && gh.type == MSG_GRID_SAVE) {
        void *data = malloc(gh.len);
        // 循环读取，确保读取完整数据
        size_t total_read = 0;
        while (total_read < gh.len) {
          ssize_t n =
              read(server_fd, (char *)data + total_read, gh.len - total_read);
          if (n <= 0) {
            log_error("client attach: read failed at %zu/%zu bytes", total_read,
                      gh.len);
            break;
          }
          total_read += n;
        }
        log_info("client attach: read data, got %zu bytes total", total_read);
        if (total_read == gh.len) {
          unsigned int pane_id;
          memcpy(&pane_id, data, sizeof(pane_id));
          log_info("client attach: grid pane_id=%u, len=%zu", pane_id, gh.len);

          struct window_pane *wp;
          int found = 0;
          list_for_each_entry(wp, &w->panes, link) {
            log_info("client attach: checking wp->id=%u vs pane_id=%u", wp->id,
                     pane_id);
            if (wp->id == pane_id) {
              unsigned int cx, cy;
              int ret =
                  grid_deserialize(wp->grid, &pane_id, &cx, &cy, data, gh.len);
              if (ret == 0) {
                wp->cx = cx;
                wp->cy = cy;
                sync_vterm_from_grid(wp);
              }
              log_info("client attach: grid_deserialize returned %d", ret);
              found = 1;
              free(data);
              break;
            }
          }
          if (!found) {
            log_warn("client attach: no pane found for pane_id=%u", pane_id);
            free(data);
          }
        }
      }
    }
  } else {
    // 不允许嵌套运行
    if (client_check_nested()) {
      const char *msg = TR(MSG_NESTED_WARNING);
      write(STDOUT_FILENO, msg, strlen(msg));
      _exit(-1);
    }
    // 创建新session
    char buf[MUXKIT_BUF_SMALL] = "new-session";
    struct winsize ws_pty = c->ws;
    ws_pty.ws_row -= 1;
    send_server(MSG_RESIZE, server_fd, &ws_pty, sizeof(ws_pty));
    send_server(MSG_COMMAND, server_fd, buf, strlen(buf) + 1);

    // 获取 server 主进程fd
    c->master_fd = recv_fd(server_fd);
    if (c->master_fd == -1) {
      log_error("recv_fd failed");
      return -1;
    }
    struct window *w = window_create(TR(MSG_WINDOW_NEW));
    c->ws.ws_row -= 1;
    c->pane = pane_create(w, c->ws.ws_col, c->ws.ws_row, 0, 0);
    pane_set_master_fd(c->pane, c->master_fd);
  }
  // 终端窗口尺寸更新
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = SA_RESTART; // restart 收到信号打断后，重新执行被打断的函数
  sigemptyset(&sa.sa_mask);
  sigaction(SIGWINCH, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);

  dispatch_event(c, EV_ENABLE_RAW_MODE);
  // 切换到备用屏幕缓冲区（防止滚动看到之前的历史）
  write(STDOUT_FILENO, "\033[?1049h", 8);
  // 清屏
  write(STDOUT_FILENO, "\033[2J\033[H", 7);

  // 初始渲染所有 pane 和状态栏
  render_status_bar(c);
  struct window_pane *p;
  list_for_each_entry(p, &c->pane->window->panes, link) {

    render_pane(p);
    if (p->link.next != &c->pane->window->panes) {
      render_pane_borders(p);
    }
  }
  // 定位光标
  char cursor_buf[32];
  int clen = snprintf(cursor_buf, sizeof(cursor_buf), "\033[%u;%uH",
                      c->pane->yoff + c->pane->cy + 1,
                      c->pane->xoff + c->pane->cx + 1);
  write(STDOUT_FILENO, cursor_buf, clen);

  log_info("entering client loop");
  client_loop(c);

  char buf[MUXKIT_BUF_SMALL];
  memset(buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf), "%d", c->slave_pid);
  send_server(MSG_EXITED, server_fd, buf, strlen(buf) + 1);
  log_info("client exiting");
  log_close();
  window_destroy(w);
  pane_destroy(c->pane);
  return 0;
}
