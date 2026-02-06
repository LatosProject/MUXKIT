/**
 * log.h - muxkit 日志模块
 *
 * 提供简单的日志功能：
 * - 4 个日志级别：DEBUG, INFO, WARN, ERROR
 * - 日志输出到 stderr 和文件
 * - 可通过 ENABLE_LOG 宏开关
 * - 自动添加时间戳、文件名、行号
 *
 * 使用方法：
 *   log_init("client");
 *   log_info("connected to server, fd %d", fd);
 *   log_close();
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

#ifndef LOG_H
#define LOG_H

/**
 * 日志级别枚举
 */
typedef enum {
  LOG_DEBUG,  /* 调试信息 */
  LOG_INFO,   /* 一般信息 */
  LOG_WARN,   /* 警告 */
  LOG_ERROR,  /* 错误 */
} log_level_t;

/**
 * 初始化日志系统
 * 日志文件创建在 socket_path 同目录下
 * @param name 日志名称 (server/client)
 */
void log_init(const char *name);

/**
 * 关闭日志
 */
void log_close(void);

/**
 * 设置最低日志级别
 * @param level 最低级别
 */
void log_set_level(log_level_t level);

/* 日志开关 */
/* #define ENABLE_LOG */
#ifdef ENABLE_LOG
/* 日志宏 - 自动添加文件名和行号 */
#define log_debug(...) log_write(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) log_write(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) log_write(LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_write(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#else
/* 日志禁用时的空宏 */
#define log_debug(...)                                                         \
  do {                                                                         \
  } while (0)
#define log_info(...)                                                          \
  do {                                                                         \
  } while (0)
#define log_warn(...)                                                          \
  do {                                                                         \
  } while (0)
#define log_error(...)                                                         \
  do {                                                                         \
  } while (0)
#endif

/**
 * 写入日志 (内部函数，通过宏调用)
 * @param level 日志级别
 * @param file  源文件名
 * @param line  行号
 * @param fmt   格式字符串
 */
void log_write(log_level_t level, const char *file, int line, const char *fmt,
               ...);

#endif
