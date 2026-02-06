/**
 * window.h - muxkit 窗口和窗格管理模块
 *
 * 定义窗口和窗格的数据结构：
 * - struct window: 窗口结构，包含多个窗格的链表
 * - struct window_pane: 窗格结构，包含终端网格、光标位置、libvterm 实例
 *
 * 主要功能：
 * - 窗口创建和销毁
 * - 窗格创建、销毁和尺寸调整
 * - libvterm 终端模拟器集成
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

#ifndef WINDOW_H
#define WINDOW_H

#include "list.h"
#include "vterm.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>

struct grid; /* 前向声明 */

/**
 * 窗口结构体
 * 每个窗口包含多个窗格 (pane)
 */
struct window {
  struct list_head link;        /* 窗口链表节点 */
  struct list_head panes;       /* 窗格链表头 */
  unsigned int id;              /* 窗口 ID */
  char *name;                   /* 窗口名称 */
  unsigned int active_point;    /* 活动点 */
  int flags;                    /* 标志位 */
  unsigned int next_pane_id;    /* 下一个 pane 的 ID */
};

/**
 * 窗格结构体
 * 每个窗格对应一个 PTY 和终端模拟器
 */
struct window_pane {
  struct list_head link;        /* 窗格链表节点 */
  struct grid *grid;            /* 屏幕网格 */
  unsigned int cx, cy;          /* 光标位置 */
  unsigned int id;              /* 窗格 ID */
  unsigned int active_point;    /* 活动点 */
  struct winsize ws;            /* 终端尺寸 */
  struct window *window;        /* 所属窗口 */
  int master_fd;                /* PTY 主端 fd */
  int slave_fd;                 /* PTY 从端 fd */
  pid_t slave_pid;              /* shell 进程 PID */

  unsigned int sx;              /* 宽度 */
  unsigned int sy;              /* 高度 */

  unsigned int xoff;            /* X 偏移 */
  unsigned int yoff;            /* Y 偏移 */

  int child_exited;             /* 子进程退出标志 */
  int flags;                    /* 标志位 */

  /* libvterm 终端模拟器 */
  VTerm *vt;                    /* vterm 实例 */
  VTermScreen *vts;             /* vterm 屏幕 */
};

/* ============ 窗口函数 ============ */

/**
 * @brief 创建窗口
 *
 * 分配并初始化一个新窗口，包括窗格链表。
 *
 * @param name 窗口名称（可选，传 NULL 则为 unnamed）
 * @return 窗口指针，失败返回 NULL
 */
struct window *window_create(const char *name);

/**
 * @brief 销毁窗口
 *
 * 释放窗口资源，不会自动销毁其中的窗格。
 *
 * @param w 窗口指针
 */
void window_destroy(struct window *w);

/* ============ 窗格函数 ============ */

/**
 * @brief 创建窗格
 *
 * 创建一个新窗格，包括：
 * - 分配屏幕网格 (grid) 和历史缓冲区
 * - 初始化 libvterm 终端模拟器
 * - 设置窗格位置和尺寸
 *
 * @param w    所属窗口
 * @param sx   宽度（列数）
 * @param sy   高度（行数）
 * @param xoff X 偏移（在终端中的起始列）
 * @param yoff Y 偏移（在终端中的起始行）
 * @return 窗格指针，失败返回 NULL
 */
struct window_pane *pane_create(struct window *w, unsigned int sx,
                                unsigned int sy, unsigned int xoff,
                                unsigned int yoff);

/**
 * @brief 销毁窗格
 *
 * 释放窗格资源，包括：
 * - libvterm 实例
 * - 屏幕网格和历史缓冲区
 *
 * @param p 窗格指针
 * @note 不会关闭 PTY 文件描述符，需要调用者处理
 */
void pane_destroy(struct window_pane *p);

/**
 * @brief 调整窗格尺寸
 *
 * 重新分配网格并同步 libvterm 尺寸。
 * 尽可能保留原有内容。
 *
 * @param p  窗格指针
 * @param sx 新宽度
 * @param sy 新高度
 */
void pane_resize(struct window_pane *p, unsigned int sx, unsigned int sy);

/**
 * @brief 设置窗格的 PTY 主端 fd
 *
 * 关联 PTY 文件描述符到窗格，并配置 libvterm 输出回调。
 *
 * @param p  窗格指针
 * @param fd PTY 主端文件描述符
 */
void pane_set_master_fd(struct window_pane *p, int fd);

#endif /* WINDOW_H */