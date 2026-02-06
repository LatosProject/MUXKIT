/**
 * log.c - muxkit 日志模块实现
 *
 * 本模块实现简单的日志功能：
 * - 输出到 stderr 和文件
 * - 日志格式：[时间] [级别] [文件:行号] 消息
 * - 行缓冲模式确保实时写入
 *
 * 日志文件位置：
 *   /tmp/muxkit-<uid>/client.log
 *   /tmp/muxkit-<uid>/server.log
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

#define _GNU_SOURCE
#include "log.h"
#include "main.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

extern char *socket_path;

static FILE *log_fp = NULL;
static log_level_t min_level = LOG_DEBUG;
static const char *log_name = "unknown";

static const char *level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};

void log_init(const char *name) {
  log_name = name;

  // 从 socket_path 提取目录，创建日志文件
  if (socket_path) {
    char log_path[MUXKIT_BUF_MEDIUM];
    char *last_slash = strrchr(socket_path, '/');
    if (last_slash) {
      size_t dir_len = last_slash - socket_path;
      snprintf(log_path, sizeof(log_path), "%.*s/%s.log", (int)dir_len,
               socket_path, name);
    } else {
      snprintf(log_path, sizeof(log_path), "%s.log", name);
    }
    log_fp = fopen(log_path, "a");
    if (log_fp) {
      setvbuf(log_fp, NULL, _IOLBF, 0); // 行缓冲
    }
  }
}

void log_close(void) {
  if (log_fp) {
    fclose(log_fp);
    log_fp = NULL;
  }
}

void log_set_level(log_level_t level) { min_level = level; }

void log_write(log_level_t level, const char *file, int line, const char *fmt,
               ...) {
  if (level < min_level)
    return;

  // 获取时间戳
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  char time_buf[32];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

  // 提取文件名
  const char *base = strrchr(file, '/');
  base = base ? base + 1 : file;

  // 格式化用户消息
  char msg[MUXKIT_BUF_LARGE];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  // 完整日志行
  char log_line[MUXKIT_BUF_LARGE + MUXKIT_BUF_MEDIUM];
  snprintf(log_line, sizeof(log_line), "[%s] [%s] [%s:%d] %s\n", time_buf,
           level_names[level], base, line, msg);

  // 输出到 stderr
  fputs(log_line, stderr);

  // 输出到文件
  if (log_fp) {
    fputs(log_line, log_fp);
  }
}
