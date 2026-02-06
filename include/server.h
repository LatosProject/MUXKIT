/**
 * server.h - muxkit 服务端头文件
 *
 * 定义服务端相关的数据结构和接口，包括：
 * - session 结构体：管理每个终端会话的状态
 * - server_start：启动服务端守护进程
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

#ifndef SERVER_H
#define SERVER_H

#define MAX_CLIENTS 64 // 最大客户端连接数
#define MAX_PANES 64
#include "list.h"
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
/**
 * 启动 muxkit 服务端
 *
 * 如果服务端尚未运行，则创建守护进程并启动服务端循环。
 * 使用 Unix 域套接字监听客户端连接。
 *
 * @return 成功时返回连接到服务端的 fd，失败返回 -1
 */
int server_start(void);

/**
 * session - 终端会话结构体
 *
 * 每个 session 代表一个独立的伪终端会话，包含：
 * - PTY 主从设备的文件描述符
 * - 关联的 shell 子进程
 * - 终端属性和窗口大小
 * - 会话的分离/附加状态
 */
struct session {
  int id;                      // 会话唯一标识符
  int client_fd;               // 关联的客户端连接 fd（-1 表示无客户端）
  int master_fds[MAX_PANES];   // PTY 主设备 fd 数组（每个 pane 一个）
  int pane_count;              // 当前 pane 数量
  pid_t pane_pids[MAX_PANES];  // 每个 pane 的 shell 进程 PID
  int slave_fd;                // PTY 从设备 fd（临时使用）
  int detached;                // 分离标志：1=已分离，0=已附加
  pid_t slave_pid;             // shell 子进程 PID
  struct winsize ws;           // 终端窗口大小（行数、列数）
  struct termios orig_termios; // 原始终端属性（用于恢复）
  int child_exited;            // 子进程退出标志
  struct termios raw;          // raw 模式终端属性
  char *slave_name;            // PTY 从设备路径（如 /dev/pts/X）
  struct environ *environ;     // 环境变量（未使用）

  struct list_head link; // 链表节点，用于连接到全局会话列表
  struct window *active_window;

  void *grid_data[MAX_PANES];
  ssize_t grid_data_len[MAX_PANES];
};

#endif /* SERVER_H */