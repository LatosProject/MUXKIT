/**
 * client.h - muxkit 客户端模块
 *
 * 本头文件定义了 muxkit 客户端的核心数据结构和接口：
 * - client_state: 客户端状态机状态枚举
 * - client_event: 客户端事件类型枚举
 * - struct client: 客户端上下文结构体
 * - state_transition: 状态转换表项结构
 *
 * 客户端采用有限状态机 (FSM) 模式处理：
 * - 终端输入/输出
 * - PTY 数据读写
 * - 窗口尺寸变化 (SIGWINCH)
 * - 子进程退出 (SIGCHLD)
 * - 会话分离/附加
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

#ifndef CLIENT_H
#define CLIENT_H

#include "render.h"
#include "window.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>

/**
 * 客户端状态枚举
 * 用于有限状态机 (FSM) 控制客户端行为
 */
typedef enum {
  ST_BOOT,     /* 启动状态 */
  ST_RUNNING,  /* 正常运行状态 */
  ST_RESIZING, /* 调整尺寸中 */
  ST_EXITING,  /* 退出中 */
} client_state;

/**
 * 客户端事件枚举
 * 定义所有可能触发状态转换的事件
 */
typedef enum {
  EV_STDIN_READ,      /* 标准输入有数据可读 */
  EV_PTY_READ,        /* PTY 有数据可读 */
  EV_WINCH,           /* 收到 SIGWINCH 信号 */
  EV_CHLD_EXIT,       /* 子进程退出 */
  EV_INTERRUPT,       /* 中断信号 */
  EV_EOF_STDIN,       /* 标准输入 EOF */
  EV_EOF_PTY,         /* PTY EOF */
  EV_ENABLE_RAW_MODE, /* 启用原始模式 */
  EV_DETACHED,        /* 会话分离 */
  EV_PANE_SPLIT,      /* 分割窗格 */
  EV_SYNC_INPUT,      /* 同步输入 */
} client_event;

/**
 * 客户端上下文结构体
 * 存储客户端运行时的所有状态信息
 */
struct client {
  client_state state;          /* 当前状态 */
  int server_fd;               /* 与 server 的连接 fd */
  int master_fd;               /* PTY 主端 fd */
  int slave_fd;                /* PTY 从端 fd */
  pid_t slave_pid;             /* 子进程 PID */
  struct winsize ws;           /* 终端窗口尺寸 */
  struct termios orig_termios; /* 原始终端属性 (用于恢复) */
  int child_exited;            /* 子进程退出标志 */
  struct termios raw;          /* 原始模式终端属性 */
  char *slave_name;            /* PTY 从端设备名 */
  struct environ *environ;     /* 环境变量 */
  struct window_pane *pane;    /* 当前活动窗格 */
  int sync_input_mode;
};

/**
 * 状态转换动作函数指针类型
 */
typedef void (*action_fn)(struct client *c, client_event ev);

/**
 * 状态转换表项
 * 定义 (当前状态, 事件) -> (下一状态, 动作) 的映射
 */
typedef struct {
  client_state state; /* 当前状态 */
  client_event event; /* 触发事件 */
  client_state next;  /* 下一状态 */
  action_fn action;   /* 执行的动作函数 */
} state_transition;

/* ============ 客户端核心函数 ============ */

/**
 * 初始化客户端结构体
 * @param c 客户端上下文指针
 */
void client_init(struct client *c);

/**
 * 客户端主入口
 * 负责连接服务器、创建会话、进入主循环
 * @param c 客户端上下文指针
 * @return 0 成功，-1 失败
 */
int client_main(struct client *c);

/**
 * 分发事件到状态机
 * @param c  客户端上下文指针
 * @param ev 事件类型
 */
void dispatch_event(struct client *c, client_event ev);

/* ============ 状态机动作函数 ============ */

/** 处理终端尺寸变化 */
void act_resize(struct client *C, client_event ev);

/** 处理子进程退出 */
void act_child_exit(struct client *C, client_event ev);

/** 启用终端原始模式 */
void act_enable_raw_mode(struct client *C, client_event ev);

/** 处理 PTY 数据读取 */
void act_pty_read(struct client *c, client_event ev);

/** 处理标准输入读取 */
void act_stdin_read(struct client *c, client_event ev);

/** 处理会话分离 */
void act_detach(struct client *c, client_event ev);

/** 处理窗格分割 */
void act_pane_split(struct client *c, client_event ev);

/** 处理输入同步 **/
void act_sync_input(struct client *c, client_event ev);

#endif /* CLIENT_H */