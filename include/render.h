/**
 * render.h - muxkit 渲染模块
 *
 * 定义终端渲染相关的数据结构和接口：
 * - struct cell: 单元格结构，存储字符、颜色、属性
 * - struct grid: 屏幕网格，包含当前屏幕和历史缓冲区
 * - struct screen: 屏幕状态，包含光标位置和标题
 *
 * 主要功能：
 * - 窗格渲染 (render_pane)
 * - 状态栏渲染 (render_status_bar)
 * - 窗格边框渲染 (render_pane_borders)
 * - 历史滚动管理 (grid_scroll_up/down)
 * - 屏幕网格序列化/反序列化 (用于会话分离/附加)
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

#ifndef RENDER_H
#define RENDER_H
#define DEFAULT_HISTORY_SIZE 1000

#include "server.h"
#include "window.h"
#include <stdint.h>
#include <sys/types.h>

struct session;
struct window_pane;
struct client;

/**
 * 单元格结构体
 * 存储屏幕上每个字符的信息
 */
struct cell {
  char ch[5];    /* UTF-8 字符 (最多4字节 + null) */
  uint8_t width; /* 显示宽度 (1 或 2) */
  uint8_t fg;    /* 前景色索引 (0-255) */
  uint8_t bg;    /* 背景色索引 (0-255) */
  uint8_t attr; /* 属性: bit0=bold, bit1=underline, bit2=italic, bit3=reverse */
  uint8_t flags; /* 标志位: bit0=默认fg, bit1=默认bg */
};

/**
 * 屏幕网格结构体
 * 包含当前屏幕内容和历史滚动缓冲区
 */
struct grid {
  struct cell *cells;  /* cells[y * width + x] */
  unsigned int width;  /* 网格宽度 */
  unsigned int height; /* 网格高度 */

  struct cell *history_cells; /* 历史缓冲区 (环形) */
  unsigned int history_size;  /* 历史缓冲区大小 */
  unsigned int history_count; /* 已保存的历史行数 */
  unsigned int scroll_offset; /* 当前滚动偏移 */

  uint8_t *line_flags;         /* 每行一个标志 */
  uint8_t *history_line_flags; /* 历史行标志 continuation = 0x01 else 0x00 */
};

/**
 * 屏幕状态结构体
 */
struct screen {
  char *title;           /* 终端标题 */
  char *path;            /* 当前路径 */
  unsigned int cx;       /* 光标 x */
  unsigned int cy;       /* 光标 y */
  int color;             /* 颜色 */
  unsigned int saved_cx; /* 保存的光标位置 (用于 ESC 7/8) */
  unsigned int saved_cy;
};

/* ============ 渲染函数 ============ */

/**
 * @brief 初始化屏幕状态
 * @param s 屏幕状态指针
 */
void render_init(struct screen *s);

/**
 * @brief 重置屏幕状态
 * 将光标位置和保存的光标位置重置为初始值
 * @param s 屏幕状态指针
 */
void screen_reinit(struct screen *s);

/**
 * @brief 清理屏幕状态
 * 释放标题和路径字符串
 * @param s 屏幕状态指针
 */
void render_cleanup(struct screen *s);

/**
 * @brief 渲染整个会话
 * 遍历所有窗格并渲染到终端
 * @param s 会话指针
 */
void render_screen(struct session *s);

/**
 * @brief 渲染单个窗格
 * 输出窗格内容到终端，包括颜色、属性和光标定位
 * @param p 窗格指针
 */
void render_pane(struct window_pane *p);

/**
 * @brief 渲染状态栏
 * 在终端底部显示窗口名称、历史标记和版本信息
 * @param c 客户端指针
 */
void render_status_bar(struct client *c);

/**
 * @brief 渲染窗格边框
 * 在窗格右侧绘制垂直分隔线
 * @param w 窗格指针
 */
void render_pane_borders(struct window_pane *w);

/* ============ 历史管理函数 ============ */

/**
 * @brief 初始化历史缓冲区
 * 分配环形缓冲区用于保存历史屏幕内容
 * @param g         网格指针
 * @param max_lines 最大历史行数
 */
void grid_init_history(struct grid *g, unsigned int max_lines);

/**
 * @brief 释放历史缓冲区
 * 释放历史数据并重置计数器
 * @param g 网格指针
 */
void grid_free_history(struct grid *g);

/**
 * @brief 根据新尺寸重新调整历史缓冲区布局
 * 当窗口宽度改变时，重新组织历史缓冲区中的内容
 * @param g         网格指针
 * @param new_width 新的网格宽度
 * @return 0 成功，-1 失败
 */
int grid_resize_history(struct grid *g, unsigned int new_width);

/**
 * @brief 将指定行推入历史
 * 将网格中的一行复制到历史缓冲区（环形）
 * @param g    网格指针
 * @param line 行号
 */
void grid_push_line_to_history(struct grid *g, unsigned int line);

/**
 * @brief 向上滚动 (查看历史)
 * 增加滚动偏移量以查看更早的历史内容
 * @param g     网格指针
 * @param lines 滚动行数
 */
void grid_scroll_up(struct grid *g, unsigned int lines);

/**
 * @brief 向下滚动 (返回当前)
 * 减少滚动偏移量以返回当前屏幕
 * @param g     网格指针
 * @param lines 滚动行数
 */
void grid_scroll_down(struct grid *g, unsigned int lines);

/**
 * @brief 获取显示行 (考虑滚动偏移)
 * 根据当前滚动位置返回对应的屏幕行或历史行
 * @param g 网格指针
 * @param y 行号（相对于当前视图）
 * @return 单元格数组指针，超出范围返回 NULL
 */
struct cell *grid_get_display_line(struct grid *g, unsigned int y);

/* ============ 序列化函数 ============ */

/**
 * @brief 序列化屏幕网格
 *
 * 将网格数据（包括当前屏幕和历史）打包为二进制格式。
 * 用于会话分离时保存屏幕状态到内存。
 *
 * @param g       网格指针
 * @param pane_id 窗格 ID
 * @param cx      光标 x 坐标
 * @param cy      光标 y 坐标
 * @param out_buf 输出缓冲区指针（调用者需要 free）
 * @return 序列化数据的字节数，失败返回 0
 */
size_t grid_serialize(struct grid *g, unsigned int pane_id, unsigned int cx,
                      unsigned int cy, void **out_buf);

/**
 * @brief 反序列化屏幕网格
 *
 * 从二进制数据恢复网格状态。
 * 用于会话附加时恢复屏幕内容和光标位置。
 *
 * @param g        网格指针
 * @param pane_id  输出：窗格 ID
 * @param cx       输出：光标 x 坐标
 * @param cy       输出：光标 y 坐标
 * @param buf      序列化数据缓冲区
 * @param len      数据长度
 * @return 0 成功，-1 失败（数据格式错误）
 */
int grid_deserialize(struct grid *g, unsigned int *pane_id, unsigned int *cx,
                     unsigned int *cy, const void *buf, size_t len);

#endif /* RENDER_H */