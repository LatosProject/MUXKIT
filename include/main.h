/**
 * main.h - muxkit 全局配置和常量定义
 *
 * 本头文件定义了 muxkit 的全局常量：
 * - MUXKIT_SOCK: Unix 域套接字目录路径
 * - MUXKIT_BUF_*: 各种缓冲区大小常量
 * - MUXKIT_LISTEN_BACKLOG: 服务端监听队列长度
 *
 * MIT License
 * Copyright (c) 2024 LatosProject
 */

#ifndef MAIN_H
#define MAIN_H

#include <paths.h>
#include <stdarg.h>

/* 默认 shell 配置（取消注释以强制使用特定 shell） */
// #define MUXKIT_DEFAULT_SHELL

/* Unix 域套接字存放目录，默认为 /tmp */
#ifndef MUXKIT_SOCK
#define MUXKIT_SOCK _PATH_TMP
#endif

/*
 * 缓冲区大小常量
 * 根据用途选择合适的缓冲区大小，避免栈溢出和内存浪费
 */
#define MUXKIT_BUF_SMALL   100   /* 小缓冲：路径、短消息 */
#define MUXKIT_BUF_MEDIUM  256   /* 中缓冲：路径、渲染 */
#define MUXKIT_BUF_LARGE   1024  /* 大缓冲：日志消息 */
#define MUXKIT_BUF_XLARGE  4096  /* 超大缓冲：数据传输 */
#define MUXKIT_BUF_PATH    512   /* 路径缓冲 */
#define MUXKIT_INPUT_SEQ   64    /* 输入序列缓冲 */
#define MUXKIT_LISTEN_BACKLOG 5  /* listen() 队列长度 */

#endif /* MAIN_H */
