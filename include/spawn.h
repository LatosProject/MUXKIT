/**
 * spawn.h - muxkit 子进程创建模块
 *
 * 定义 shell 子进程的创建接口：
 * - spawn_child: 在 PTY 从设备上 fork 并执行 shell
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

#ifndef SPAWN_H
#define SPAWN_H

#include <sys/types.h>
#include <sys/ioctl.h>

struct session;

/**
 * 创建 shell 子进程
 *
 * 在 PTY 从设备上 fork 子进程并执行用户的 shell。
 * 子进程会：
 * - 创建新会话 (setsid)
 * - 打开并配置 PTY 从设备
 * - 设置控制终端
 * - 重定向标准输入/输出/错误
 * - 执行 shell 程序
 *
 * @param s 会话结构体指针
 * @return 子进程 PID (父进程)，不返回 (子进程)
 */
pid_t spawn_child(struct session *s);

#endif /* SPAWN_H */
