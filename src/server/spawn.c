/**
 * spawn.c - muxkit 子进程创建实现
 *
 * 本模块负责在 PTY 从设备上创建 shell 子进程：
 * - fork 子进程
 * - 创建新会话，脱离父进程的控制终端
 * - 打开 PTY 从设备并设置为控制终端
 * - 配置终端属性 (OPOST, ONLCR, ICRNL)
 * - 设置环境变量 (TERM, MUXKIT)
 * - 重定向标准 I/O 到 PTY
 * - 执行用户 shell
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

#include "i18n.h"
#include "main.h"
#include "server.h"
#include "util.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
extern char **environ;

/*
  生成子进程
*/
pid_t spawn_child(struct session *s) {

  pid_t pid = fork();
  if (pid < 0) {
    perror(TR(MSG_ERR_FORK));
    exit(1);
  } else if (pid == 0) {
    const char *shell = getshell();
    char *args[] = {(char *)shell, NULL};

    // 创建会话
    setsid();

    s->slave_fd = open(*&s->slave_name, O_RDWR);
    if (s->slave_fd < 0) {
      perror(TR(MSG_ERR_OPEN_PTY));
      _exit(1);
    }

    // 配置 PTY 终端属性
    struct termios tio;
    tcgetattr(s->slave_fd, &tio);
    tio.c_oflag |= OPOST | ONLCR; // 输出处理：NL -> CR+NL
    tio.c_iflag |= ICRNL;         // 输入处理：CR -> NL
    tcsetattr(s->slave_fd, TCSANOW, &tio);

    // 把 slave 设为子进程的控制终端
    ioctl(s->slave_fd, TIOCSCTTY, 0);

    setenv("TERM", "xterm-256color", 1);

    char buf[MUXKIT_BUF_SMALL];
    snprintf(buf, sizeof(buf), "%d", s->slave_pid);
    setenv("MUXKIT", buf, 1);

    tcsetpgrp(
        s->slave_fd,
        getpid()); // 设置前台进程组。这样，终端设备驱动程序就能了解将终端输入和终端产生的信号送到何处。

    dup2(s->slave_fd, STDIN_FILENO);
    dup2(s->slave_fd, STDOUT_FILENO);
    dup2(s->slave_fd, STDERR_FILENO);

    // 关闭所有继承的 fd（除了 0, 1, 2）
    // 这样 server 的 client socket 不会被子进程持有
    for (int fd = 3; fd < 1024; fd++) {
      close(fd);
    }

    execve(args[0], args, environ);
    perror(TR(MSG_ERR_EXEC));
    _exit(1); // Use _exit to avoid flushing stdio buffers again
  }
  return pid;
}
