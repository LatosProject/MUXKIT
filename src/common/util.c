/**
 * util.c - muxkit 工具函数模块实现
 *
 * 本模块实现通用工具函数：
 * - 获取用户默认 shell (优先级：$SHELL > passwd > /bin/sh)
 * - 检查 shell 可执行性
 * - 检查嵌套运行 (MUXKIT/TMUX 环境变量)
 * - 文件描述符传递 (SCM_RIGHTS)
 *
 * 文件描述符传递：
 *   使用 sendmsg/recvmsg 和 SCM_RIGHTS 辅助消息
 *   在进程间传递打开的文件描述符
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

#include "util.h"
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/file.h>
#include <unistd.h>
struct passwd *pw;

int checkshell(const char *shell) {
  if (shell == NULL || *shell != '/') {
    return 0;
  }
  if (access(shell, X_OK) != 0) {
    return 0;
  }
  return 1;
}

const char *getshell() {
  const char *shell;
#ifndef MUXKIT_DEFAULT_SHELL
  shell = getenv("SHELL");
  if (checkshell(shell)) {
    return shell;
  }

  pw = getpwuid(getuid());
  if (pw != NULL && checkshell(shell)) {
    return pw->pw_shell;
  }
#endif
  return (_PATH_BSHELL);
}

struct environ_entry *environ_find(const char *name,
                                   struct environ_entry *out) {

  out->name = (char *)name;
  out->value = getenv(name);
  return out;
}

int client_check_nested() {
  struct environ_entry out1, out2;
  struct environ_entry *envent = environ_find("MUXKIT", &out1);
  struct environ_entry *tmux = environ_find("TMUX", &out2);

  if ((tmux == NULL || tmux->value == NULL || *tmux->value == '\0') &&
      (envent == NULL || envent->value == NULL || *envent->value == '\0'))
    return (0);
  return (1);
}

// fdpass.c
int send_fd(int sock, int fd) {
  struct msghdr msg = {0};
  struct iovec iov[1];
  char buf[1] = {0};

  // 辅助数据缓冲区，存放要传递的 fd
  char cmsgbuf[CMSG_SPACE(sizeof(int))];

  iov[0].iov_base = buf;
  iov[0].iov_len = 1;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS; // 传递文件描述符
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  *(int *)CMSG_DATA(cmsg) = fd; // 把 fd 放进去

  return sendmsg(sock, &msg, 0) >= 0 ? 0 : -1; // fd内核检查
}

int recv_fd(int sock) {
  struct msghdr msg = {0};
  struct iovec iov[1];
  char buf[1];
  char cmsgbuf[CMSG_SPACE(sizeof(int))];

  iov[0].iov_base = buf;
  iov[0].iov_len = 1;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  if (recvmsg(sock, &msg, 0) < 0) // fd内核验证
    return -1;

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  if (cmsg && cmsg->cmsg_type == SCM_RIGHTS) {
    return *(int *)CMSG_DATA(cmsg); // 提取接收到的 fd
  }
  return -1;
}

// Unicode codepoint 转 UTF-8
int unicode_to_utf8(uint32_t cp, char *buf) {
  if (cp < 0x80) {
    buf[0] = (char)cp;
    buf[1] = 0;
    return 1;
  } else if (cp < 0x800) {
    buf[0] = (char)(0xC0 | (cp >> 6));
    buf[1] = (char)(0x80 | (cp & 0x3F));
    buf[2] = 0;
    return 2;
  } else if (cp < 0x10000) {
    buf[0] = (char)(0xE0 | (cp >> 12));
    buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (cp & 0x3F));
    buf[3] = 0;
    return 3;
  } else if (cp < 0x110000) {
    buf[0] = (char)(0xF0 | (cp >> 18));
    buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (cp & 0x3F));
    buf[4] = 0;
    return 4;
  }
  buf[0] = 0;
  return 0;
}