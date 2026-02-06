/**
 * util.h - muxkit 工具函数模块
 *
 * 提供通用工具函数：
 * - getshell: 获取用户默认 shell
 * - checkshell: 检查 shell 可执行性
 * - client_check_nested: 检查是否在 muxkit/tmux 中嵌套运行
 * - send_fd/recv_fd: Unix 域套接字传递文件描述符
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

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <sys/socket.h>

/**
 * 获取用户默认 shell
 * 优先级：$SHELL 环境变量 > passwd 文件 > /bin/sh
 * @return shell 路径
 */
const char *getshell(void);

/**
 * 检查 shell 是否可执行
 * @param shell shell 路径
 * @return 1 可执行，0 不可执行
 */
int checkshell(const char *shell);

/**
 * 环境变量条目结构
 */
struct environ_entry {
  char *name;
  char *value;
  int flags;
};

/**
 * 检查是否嵌套运行
 * 通过检查 MUXKIT 和 TMUX 环境变量判断
 * @return 1 嵌套运行，0 非嵌套
 */
int client_check_nested(void);

/**
 * 通过 Unix 域套接字发送文件描述符
 * 使用 SCM_RIGHTS 辅助消息
 * @param sock 套接字
 * @param fd   要发送的文件描述符
 * @return 0 成功，-1 失败
 */
int send_fd(int sock, int fd);

/**
 * 通过 Unix 域套接字接收文件描述符
 * @param sock 套接字
 * @return 接收到的文件描述符，失败返回 -1
 */
int recv_fd(int sock);

/**
 * @brief Unicode codepoint 转 UTF-8 编码
 *
 * 将单个 Unicode codepoint 转换为 UTF-8 编码序列。
 * 支持完整的 Unicode 范围 (U+0000 ~ U+10FFFF)。
 *
 * @param cp  Unicode codepoint 值
 * @param buf 输出缓冲区（至少需要 5 字节：4字节UTF-8 + 1字节NULL）
 * @return 写入的字节数（不含结尾 NULL），失败返回 0
 *
 * @note 缓冲区大小建议：
 *   - 1字节: U+0000 ~ U+007F
 *   - 2字节: U+0080 ~ U+07FF
 *   - 3字节: U+0800 ~ U+FFFF
 *   - 4字节: U+10000 ~ U+10FFFF
 *
 * @example
 * char buf[5];
 * int len = unicode_to_utf8(0x4E2D, buf); // 中文"中"
 * // len = 3, buf = "\xE4\xB8\xAD"
 */
int unicode_to_utf8(uint32_t cp, char *buf);

#endif /* UTIL_H */
