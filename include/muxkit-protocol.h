/**
 * muxkit-protocol.h - muxkit 客户端-服务端通信协议
 *
 * 定义客户端和服务端之间的消息协议：
 * - 消息类型枚举 (msgtype)
 * - 消息头结构 (msg_header)
 *
 * 消息格式：
 *   [msg_header][payload]
 *
 * 主要消息类型：
 *   MSG_COMMAND      - 执行命令
 *   MSG_RESIZE       - 调整终端尺寸
 *   MSG_DETACH       - 分离/附加会话
 *   MSG_LIST_SESSIONS - 列出会话
 *   MSG_GRID_SAVE    - 保存屏幕状态
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

#pragma once
#include <stddef.h>

#define PROTOCOL_VERSION 2

/**
 * 消息类型枚举
 */
enum msgtype {
  MSG_VERSION = 12,

  /* 客户端标识消息 (100-199) */
  MSG_IDENTIFY_FLAGS = 100,
  MSG_IDENTIFY_TERM,
  MSG_IDENTIFY_TTYNAME,
  MSG_IDENTIFY_OLDCWD, /* unused */
  MSG_IDENTIFY_STDIN,
  MSG_IDENTIFY_ENVIRON,
  MSG_IDENTIFY_DONE,
  MSG_IDENTIFY_CLIENTPID,
  MSG_IDENTIFY_CWD,
  MSG_IDENTIFY_FEATURES,
  MSG_IDENTIFY_STDOUT,
  MSG_IDENTIFY_LONGFLAGS,
  MSG_IDENTIFY_TERMINFO,

  /* 命令消息 (200-299) */
  MSG_COMMAND = 200,
  MSG_DETACH,
  MSG_LIST_SESSIONS,
  MSG_DETACHKILL,
  MSG_EXIT,
  MSG_EXITED,
  MSG_EXITING,
  MSG_LOCK,
  MSG_READY,
  MSG_RESIZE,
  MSG_SHELL,
  MSG_SHUTDOWN,
  MSG_OLDSTDERR, /* unused */
  MSG_OLDSTDIN,  /* unused */
  MSG_OLDSTDOUT, /* unused */
  MSG_SUSPEND,
  MSG_UNLOCK,
  MSG_WAKEUP,
  MSG_EXEC,
  MSG_FLAGS,

  /* I/O 消息 (300-399) */
  MSG_READ_OPEN = 300,
  MSG_READ,
  MSG_READ_DONE,
  MSG_WRITE_OPEN,
  MSG_WRITE,
  MSG_WRITE_READY,
  MSG_WRITE_CLOSE,
  MSG_READ_CANCEL,

  MSG_GRID_SAVE,
};

/**
 * 消息头结构
 */
struct msg_header {
  enum msgtype type; /* 消息类型 */
  size_t len;        /* 负载长度 */
};