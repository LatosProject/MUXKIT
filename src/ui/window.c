/**
 * window.c - muxkit 窗口和窗格管理实现
 *
 * 本模块实现窗口和窗格的管理功能：
 * - 窗口创建和销毁
 * - 窗格创建、销毁、尺寸调整
 * - libvterm 终端模拟器集成
 * - 屏幕网格 (grid) 管理
 * - 历史滚动回调
 *
 * libvterm 集成：
 * - vterm_new: 创建终端模拟器实例
 * - vterm_screen: 获取屏幕对象
 * - screen_sb_pushline: 滚动回调，保存历史行
 * - vterm_output_callback: 输出回调，发送到 PTY
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

#include "window.h"
#include "list.h"
#include "render.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// vterm 屏幕滚动回调
static int screen_sb_pushline(int cols, const VTermScreenCell *cells,
                              void *user) {
  struct window_pane *p = user;
  if (!p || !p->grid || !p->grid->history_cells)
    return 0;

  struct grid *g = p->grid;
  unsigned int dst_line = g->history_count % g->history_size;
  struct cell *dst = &g->history_cells[dst_line * g->width];

  // libvterm 提供的 cells 复制
  for (unsigned int x = 0; x < g->width && (int)x < cols; x++) {
    const VTermScreenCell *vc = &cells[x];
    struct cell *c = &dst[x];
    if (vc->chars[0]) {
      unicode_to_utf8(vc->chars[0], c->ch);
    } else {
      c->ch[0] = ' ';
      c->ch[1] = 0;
    }
    c->width = vc->width ? vc->width : 1;
    c->fg = VTERM_COLOR_IS_INDEXED(&vc->fg) ? vc->fg.indexed.idx : 0;
    c->bg = VTERM_COLOR_IS_INDEXED(&vc->bg) ? vc->bg.indexed.idx : 0;
    c->flags = (VTERM_COLOR_IS_DEFAULT_FG(&vc->fg) ? 0x01 : 0) |
               (VTERM_COLOR_IS_DEFAULT_BG(&vc->bg) ? 0x02 : 0);
    c->attr = (vc->attrs.bold ? 0x01 : 0) | (vc->attrs.underline ? 0x02 : 0) |
              (vc->attrs.italic ? 0x04 : 0) | (vc->attrs.reverse ? 0x08 : 0);
  }
  g->history_count++;
  return 0;
}

static VTermScreenCallbacks screen_callbacks = {
    .sb_pushline = screen_sb_pushline,
};

// vterm 输出回调 - 将终端响应发送回 PTY
static void vterm_output_callback(const char *s, size_t len, void *user) {
  struct window_pane *p = user;
  if (p->master_fd >= 0) {
    write(p->master_fd, s, len);
  }
}

/*
  窗格返回信息
*/
void pane_set_master_fd(struct window_pane *p, int fd) {
  if (!p)
    return;
  p->master_fd = fd;
  if (p->vt) {
    /* 通过 vterm_input_write() 发给 vterm 的数据会经过 vterm_output_callback
       发送到 master_fd */
    vterm_output_set_callback(p->vt, vterm_output_callback, p);
  }
}

/*
  窗格设置尺寸
*/
void pane_resize(struct window_pane *p, unsigned int sx, unsigned int sy) {
  if (!p || !p->grid)
    return;
  struct cell *new_cells = calloc(sx * sy, sizeof(struct cell));
  if (!new_cells)
    return;
  for (unsigned int y = 0; y < p->grid->height && y < sy; y++) {
    unsigned int copy_width = (p->grid->width < sx) ? p->grid->width : sx;
    memcpy(&new_cells[y * sx], &p->grid->cells[y * p->grid->width],
           copy_width * sizeof(struct cell));
  }

  free(p->grid->cells);
  p->grid->cells = new_cells;
  p->grid->width = sx;
  p->grid->height = sy;
  p->sx = sx;
  p->sy = sy;

  // 同步 libvterm 尺寸
  if (p->vt) {
    vterm_set_size(p->vt, sy, sx);
  }

  if (p->cx >= sx)
    p->cx = sx - 1;
  if (p->cy >= sy)
    p->cy = sy - 1;
}

/*
  创建窗口
*/
struct window *window_create(const char *name) {
  struct window *w = calloc(1, sizeof(*w));
  if (!w)
    return NULL;

  list_init(&w->panes);
  w->name = name ? strdup(name) : NULL;
  w->next_pane_id = 0;

  return w;
}

/*
  销毁窗口
*/
void window_destroy(struct window *w) {
  if (!w)
    return;
  free(w->name);
  free(w);
}

struct window_pane *pane_create(struct window *w, unsigned int sx,
                                unsigned int sy, unsigned int xoff,
                                unsigned int yoff) {
  struct window_pane *p = calloc(1, sizeof(*p));
  if (!p)
    return NULL;
  p->sx = sx;
  p->sy = sy;
  p->xoff = xoff;
  p->yoff = yoff;
  p->cx = 0;
  p->cy = 0;
  p->window = w;
  p->id = w->next_pane_id++;

  p->grid = calloc(1, sizeof(*p->grid));
  if (!p->grid) {
    free(p);
    return NULL;
  }
  if (p->grid) {
    p->grid->width = sx;
    p->grid->height = sy;
    p->grid->cells = calloc(sx * sy, sizeof(struct cell));
    grid_init_history(p->grid, 1000); // 初始化历史缓冲区
  }

  // 初始化 libvterm
  p->vt = vterm_new(sy, sx);
  if (p->vt) {
    vterm_set_utf8(p->vt, 1); // 设置输入为 UTF-8 编码
    p->vts = vterm_obtain_screen(
        p->vt); // 初始化screen 屏幕单元格内容（字符+颜色+属性）
    vterm_screen_enable_altscreen(p->vts,
                                  1); // 启用备用屏幕（维护两个屏幕缓冲区）
    vterm_screen_set_callbacks(p->vts, &screen_callbacks, p); // 设置滚动回调
    vterm_screen_reset(p->vts, 1);                            // 初始化内存
  }

  list_add_tail(&p->link, &w->panes);
  return p;
}

/*
  销毁窗格
*/
void pane_destroy(struct window_pane *p) {
  if (!p)
    return;
  if (p->vt)
    vterm_free(p->vt);
  if (p->grid) {
    grid_free_history(p->grid);
    free(p->grid->cells);
    free(p->grid);
  }
  free(p);
}