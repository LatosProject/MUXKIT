/**
 * render.c - muxkit 渲染模块实现
 *
 * 本模块实现终端渲染功能：
 * - 窗格内容渲染 (ANSI 转义序列)
 * - 状态栏渲染
 * - 窗格边框渲染
 * - 历史滚动管理 (环形缓冲区)
 * - 屏幕网格序列化/反序列化
 *
 * 渲染流程：
 * 1. 隐藏光标
 * 2. 逐行输出字符和颜色属性
 * 3. 定位光标到正确位置
 * 4. 显示光标
 *
 * 历史滚动：
 * - 使用环形缓冲区保存历史行
 * - scroll_offset 控制当前视图偏移
 * - grid_get_display_line 返回正确的显示行
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

#include "render.h"
#include "client.h"
#include "i18n.h"
#include "list.h"
#include "main.h"
#include "version.h"
#include "window.h"
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define CURSOR_HIDE "\033[?25l"
#define CURSOR_SHOW "\033[?25h"
/*
  历史初始化
 */
void grid_init_history(struct grid *g, unsigned int max_lines) {
  g->history_cells = calloc(max_lines * g->width, sizeof(struct cell));
  g->history_line_flags = calloc(max_lines, sizeof(uint8_t));
  g->history_size = max_lines;
  g->scroll_offset = 0;
  g->history_count = 0;
}

/*
  历史向下滚动
*/
void grid_scroll_down(struct grid *g, unsigned int lines) {
  if (lines > g->scroll_offset)
    g->scroll_offset = 0;
  else
    g->scroll_offset -= lines;
}

/*
  历史向上滚动
*/
void grid_scroll_up(struct grid *g, unsigned int lines) {
  // 可滚动的最大行数是 min(history_count, history_size)
  unsigned int max_scroll =
      (g->history_count < g->history_size) ? g->history_count : g->history_size;
  if (g->scroll_offset + lines > max_scroll)
    g->scroll_offset = max_scroll;
  else
    g->scroll_offset += lines;
}

/*
  销毁历史
*/
void grid_free_history(struct grid *g) {
  if (g->history_cells) {
    free(g->history_cells);
    g->history_cells = NULL;
  }
  if (g->history_line_flags) {
    free(g->history_line_flags);
    g->history_line_flags = NULL;
  }
  g->history_count = 0;
  g->scroll_offset = 0;
}

/*
  将网格制定行添入历史
*/
void grid_push_line_to_history(struct grid *g, unsigned int line) {
  if (!g->history_cells || g->history_size == 0)
    return;
  // 计算历史中的目标位置（环形缓冲区）
  unsigned int dst_line = g->history_count % g->history_size;
  // 复制该行到历史
  memcpy(&g->history_cells[dst_line * g->width], &g->cells[line * g->width],
         g->width * sizeof(struct cell));
  g->history_count++; // 始终递增，用于环形缓冲区索引
}

/*
  获取网格制定行
*/
struct cell *grid_get_display_line(struct grid *g, unsigned int y) {
  if (g->scroll_offset == 0) { // 未滚动
    return &g->cells[y * g->width];
  }
  if (!g->history_count || g->history_size == 0)
    return NULL;

  // 可用的历史行数
  unsigned int available =
      (g->history_count < g->history_size) ? g->history_count : g->history_size;

  int history_line = (int)available - (int)g->scroll_offset + (int)y;
  // 滚动超出历史范围
  if (history_line < 0)
    return NULL;
  // 非历史记录部分
  if (history_line >= (int)available) {
    int screen_y = history_line - available;
    return &g->cells[screen_y * g->width];
  }

  unsigned int actual;
  if (g->history_count <= g->history_size) {
    actual = history_line;
  } else {
    unsigned int oldest = g->history_count % g->history_size;
    actual = (oldest + history_line) % g->history_size;
  }
  return &g->history_cells[actual * g->width];
}

/*
  渲染初始化
*/
void render_init(struct screen *s) {
  s->title = "\0";
  s->path = NULL;
  s->color = -1;
}

/*
  屏幕初始化
*/
void screen_reinit(struct screen *s) {
  s->cx = 0;
  s->cy = 0;
  s->saved_cx = UINT_MAX;
  s->saved_cy = UINT_MAX;
}

/*
  渲染清理
*/
void render_cleanup(struct screen *s) {
  s->title = NULL;
  s->path = NULL;
}

/*
  渲染屏幕
*/
void render_screen(struct session *s) {
  struct window *w = s->active_window;
  write(STDOUT_FILENO, CURSOR_HIDE, strlen(CURSOR_HIDE));
  struct window_pane *p;
  // 从链头panes开始，每次取一个包含link的节点，返回给p
  list_for_each_entry(p, &w->panes, link) { render_pane(p); }
}

/*
  渲染网格
*/
void render_pane(struct window_pane *p) {
  if (!p || !p->grid)
    return;
  // 隐藏光标
  write(STDOUT_FILENO, CURSOR_HIDE, 6);

  char buf[128];
  struct grid *g = p->grid;
  uint8_t last_fg = 0, last_bg = 0, last_attr = 0, last_flags = 0x03;

  // 重置颜色
  write(STDOUT_FILENO, "\033[0m", 4);

  for (unsigned int y = 0; y < p->sy; y++) {
    // ANSI 标准规定终端从 (1,1) 开始
    int len =
        snprintf(buf, sizeof(buf), "\033[%u;%uH", p->yoff + y + 1, p->xoff + 1);
    write(STDOUT_FILENO, buf, len);
    struct cell *line = grid_get_display_line(g, y);
    if (!line) {
      for (unsigned int x = 0; x < p->sx; x++) {
        write(STDOUT_FILENO, " ", 1);
      }
      continue;
    }
    for (unsigned int x = 0; x < p->sx;) {
      struct cell *c = &line[x];

      // 检查是否需要更新颜色/属性
      int need_update = (c->fg != last_fg || c->bg != last_bg ||
                         c->attr != last_attr || c->flags != last_flags);

      if (need_update) {
        // 重置
        write(STDOUT_FILENO, "\033[0m", 4);
        // 设置属性
        if (c->attr & 0x01)
          write(STDOUT_FILENO, "\033[1m", 4); // bold
        if (c->attr & 0x02)
          write(STDOUT_FILENO, "\033[4m", 4); // underline
        if (c->attr & 0x04)
          write(STDOUT_FILENO, "\033[3m", 4); // italic
        if (c->attr & 0x08)
          write(STDOUT_FILENO, "\033[7m", 4); // reverse

        // 设置前景色 (非默认)
        if (!(c->flags & 0x01)) {
          len = snprintf(buf, sizeof(buf), "\033[38;5;%um", c->fg);
          write(STDOUT_FILENO, buf, len);
        }

        // 设置背景色 (非默认)
        if (!(c->flags & 0x02)) {
          len = snprintf(buf, sizeof(buf), "\033[48;5;%um", c->bg);
          write(STDOUT_FILENO, buf, len);
        }

        last_fg = c->fg;
        last_bg = c->bg;
        last_attr = c->attr;
        last_flags = c->flags;
      }

      if (c->ch[0]) {
        write(STDOUT_FILENO, c->ch, strlen(c->ch));
        // 宽字符占多列，跳过后续单元格
        x += (c->width > 0) ? c->width : 1;
      } else {
        write(STDOUT_FILENO, " ", 1);
        x++;
      }
    }
  }
  // 重置颜色
  write(STDOUT_FILENO, "\033[0m", 4);

  // 历史模式下隐藏光标，正常模式下显示
  if (g->scroll_offset > 0) {
    write(STDOUT_FILENO, CURSOR_HIDE, 6);
  } else {
    // 光标移动到 pane 内的正确位置
    struct client *c = container_of(p, struct client, pane);
    int clen;
    if (c->sync_input_mode) {
      clen = snprintf(buf, sizeof(buf), "\033[%u;%uH\033[6 q",
                      p->yoff + p->cy + 1, p->xoff + p->cx + 1);
    } else {
      clen = snprintf(buf, sizeof(buf), "\033[%u;%uH\033[2 q", p->yoff + p->cy + 1,
                      p->xoff + p->cx + 1);
    }
    write(STDOUT_FILENO, buf, clen);
    write(STDOUT_FILENO, CURSOR_SHOW, 6);
  }
}

/*
  渲染状态栏
*/
void render_status_bar(struct client *c) {
  char buf[MUXKIT_BUF_MEDIUM];
  unsigned int row = c->ws.ws_row + 1; // 最后一行
  unsigned int cols = c->ws.ws_col;
  write(STDOUT_FILENO, CURSOR_HIDE, 6);
  // 移动到最后一行，蓝色背景白色文字
  int len = snprintf(buf, sizeof(buf), "\033[%u;1H\033[44;97m", row);
  write(STDOUT_FILENO, buf, len);

  // 写状态内容
  const char *wname = c->pane->window->name ? c->pane->window->name : "unnamed";
  int wstr_len = snprintf(buf, sizeof(buf), " %s ", wname);
  write(STDOUT_FILENO, buf, wstr_len);

  // 计算窗口名称的显示宽度（中文字符占2列）
  unsigned int wname_display_width = 2; // 两边的空格
  const unsigned char *p = (const unsigned char *)wname;
  while (*p) {
    if (*p >= 0x80) {
      // UTF-8 多字节字符，跳过后续字节
      if ((*p & 0xE0) == 0xC0) {
        p += 2;
        wname_display_width += 1;
      } else if ((*p & 0xF0) == 0xE0) {
        p += 3;
        wname_display_width += 2;
      } else if ((*p & 0xF8) == 0xF0) {
        p += 4;
        wname_display_width += 2;
      } else {
        p++;
        wname_display_width += 1;
      }
    } else {
      p++;
      wname_display_width++;
    }
  }

  int history_display_width = 0;
  if (c->pane->grid->scroll_offset) {
    const char *history_str = TR(MSG_STATUS_HISTORY);
    int hstr_len = snprintf(buf, sizeof(buf), "%s", history_str);
    write(STDOUT_FILENO, buf, hstr_len);
    // 计算历史标签的显示宽度
    p = (const unsigned char *)history_str;
    while (*p) {
      if (*p >= 0x80) {
        if ((*p & 0xE0) == 0xC0) {
          p += 2;
          history_display_width += 1;
        } else if ((*p & 0xF0) == 0xE0) {
          p += 3;
          history_display_width += 2;
        } else if ((*p & 0xF8) == 0xF0) {
          p += 4;
          history_display_width += 2;
        } else {
          p++;
          history_display_width += 1;
        }
      } else {
        p++;
        history_display_width++;
      }
    }
  }

  // 用空格填满整行
  int vstr_len = snprintf(buf, sizeof(buf), "%s", MUXKIT_VERSION_STRING);
  unsigned int used_width = wname_display_width + history_display_width;

  for (unsigned int i = used_width; i < cols; i++) {
    if (i >= cols - 1 - vstr_len) {
      write(STDOUT_FILENO, buf, vstr_len);
      write(STDOUT_FILENO, " ", 1);
      break;
    }
    write(STDOUT_FILENO, " ", 1);
  }

  // 清除到行尾，防止残留字符
  write(STDOUT_FILENO, "\033[K", 3);
  // 重置属性
  write(STDOUT_FILENO, "\033[0m", 4);
  if (c->pane->grid->scroll_offset == 0) {
    // 光标移动到 pane 内的正确位置 （vt解析）
    int clen;
    if (c->sync_input_mode) {
      clen = snprintf(buf, sizeof(buf), "\033[%u;%uH\033[6 q",
                      c->pane->yoff + c->pane->cy + 1,
                      c->pane->xoff + c->pane->cx + 1);
    } else {
      clen = snprintf(buf, sizeof(buf), "\033[%u;%uH\033[2 q",
                      c->pane->yoff + c->pane->cy + 1,
                      c->pane->xoff + c->pane->cx + 1);
    }
    write(STDOUT_FILENO, buf, clen);
    write(STDOUT_FILENO, CURSOR_SHOW, 6);
  }
}

/*
  渲染网格分割线
*/
void render_pane_borders(struct window_pane *p) {
  write(STDOUT_FILENO, CURSOR_HIDE, 6);
  char buf[MUXKIT_BUF_MEDIUM];
  for (unsigned int y = 0; y < p->sy; y++) {
    int len = snprintf(buf, sizeof(buf), "\033[%u;%uH\033[34m│\033[0m",
                       p->yoff + y + 1, p->xoff + p->sx + 1);
    write(STDOUT_FILENO, buf, len);
  }

  // 光标移动到 pane 内的正确位置 （vt解析）
  int clen;
  struct client *c = container_of(p, struct client, pane);
  if (c->sync_input_mode) {
    clen = snprintf(buf, sizeof(buf), "\033[%u;%uH\033[6 q", p->yoff + p->cy + 1,
                    p->xoff + p->cx + 1);
  } else {
    clen = snprintf(buf, sizeof(buf), "\033[%u;%uH\033[2 q", p->yoff + p->cy + 1,
                    p->xoff + p->cx + 1);
  }
  write(STDOUT_FILENO, buf, clen);
  write(STDOUT_FILENO, CURSOR_SHOW, 6);
}

/*
  网格序列化
*/
size_t grid_serialize(struct grid *g, unsigned int pane_id, unsigned int cx,
                      unsigned int cy, void **out_buf) {
  unsigned int stored_history =
      (g->history_count < g->history_size) ? g->history_count : g->history_size;

  size_t cells_size = g->width * g->height * sizeof(*g->cells);
  size_t hist_cells_size = stored_history * g->width * sizeof(*g->cells);
  size_t total = 8 * sizeof(unsigned int) + cells_size + hist_cells_size;

  char *buf = malloc(total);
  if (!buf)
    return 0;
  char *p = buf;
  memcpy(p, &pane_id, sizeof(pane_id));
  p += sizeof(pane_id);
  memcpy(p, &cx, sizeof(cx));
  p += sizeof(cx);
  memcpy(p, &cy, sizeof(cy));
  p += sizeof(cy);
  memcpy(p, &g->width, sizeof(g->width));
  p += sizeof(g->width);
  memcpy(p, &g->height, sizeof(g->height));
  p += sizeof(g->height);
  memcpy(p, &g->history_size, sizeof(g->history_size));
  p += sizeof(g->history_size);
  memcpy(p, &g->history_count, sizeof(g->history_count));
  p += sizeof(g->history_count);
  memcpy(p, &g->scroll_offset, sizeof(g->scroll_offset));
  p += sizeof(g->scroll_offset);
  memcpy(p, g->cells, cells_size);
  p += cells_size;

  if (hist_cells_size > 0 && g->history_cells) {
    if (g->history_count <= g->history_size) {
      memcpy(p, g->history_cells, hist_cells_size);
    } else {
      unsigned int oldest = g->history_count % g->history_size;
      size_t old_one =
          (g->history_size - oldest) * g->width * sizeof(*g->cells);
      size_t new_one = oldest * g->width * sizeof(*g->cells);
      memcpy(p, &g->history_cells[oldest * g->width], old_one);
      memcpy(p + old_one, g->history_cells, new_one);
    }
  }
  *out_buf = buf;
  return total;
}

/*
  网格反序列化
*/
int grid_deserialize(struct grid *g, unsigned int *pane_id, unsigned int *cx,
                     unsigned int *cy, const void *buf, size_t len) {
  const char *p = buf;

  if (len < 8 * sizeof(unsigned int))
    return -1;

  memcpy(pane_id, p, sizeof(*pane_id));
  p += sizeof(*pane_id);
  memcpy(cx, p, sizeof(*cx));
  p += sizeof(*cx);
  memcpy(cy, p, sizeof(*cy));
  p += sizeof(*cy);
  memcpy(&g->width, p, sizeof(g->width));
  p += sizeof(g->width);
  memcpy(&g->height, p, sizeof(g->height));
  p += sizeof(g->height);
  memcpy(&g->history_size, p, sizeof(g->history_size));
  p += sizeof(g->history_size);
  memcpy(&g->history_count, p, sizeof(g->history_count));
  p += sizeof(g->history_count);
  memcpy(&g->scroll_offset, p, sizeof(g->scroll_offset));
  p += sizeof(g->scroll_offset);

  // cells
  size_t cells_size = g->width * g->height * sizeof(struct cell);
  unsigned int stored =
      (g->history_count < g->history_size) ? g->history_count : g->history_size;
  size_t hist_size = stored * g->width * sizeof(struct cell);

  if (len < 8 * sizeof(unsigned int) + cells_size + hist_size)
    return -1;

  // 释放旧数据（pane_create 时已分配）
  free(g->cells);
  free(g->history_cells);

  g->cells = malloc(cells_size);
  if (!g->cells)
    return -1;
  memcpy(g->cells, p, cells_size);
  p += cells_size;

  // history
  if (g->history_size > 0) {
    g->history_cells = calloc(g->history_size * g->width, sizeof(struct cell));
    if (!g->history_cells)
      return -1;
    if (stored > 0) {
      memcpy(g->history_cells, p, hist_size);
    }
    // 重置 history_count，因为序列化时已经展开成顺序排列了
    g->history_count = stored;
  } else {
    g->history_cells = NULL;
  }

  return 0;
}

/*
  判断 cell 是否为空白（
*/
static int cell_is_blank(const struct cell *c) {
  return (c->ch[0] == ' ' || c->ch[0] == 0) && (c->flags & 0x03) == 0x03 &&
         c->attr == 0;
}

/*
  判断 cell 是否为视觉空白
*/
static int cell_is_visually_blank(const struct cell *c) {
  return c->ch[0] == ' ' || c->ch[0] == 0;
}

/*
  根据新宽度重新调整历史缓冲区布局
*/
int grid_resize_history(struct grid *g, unsigned int new_width) {
  if (!g->history_cells || g->history_size == 0 || new_width == 0)
    return 0;
  if (new_width == g->width)
    return 0;

  unsigned int old_width = g->width;
  unsigned int stored =
      (g->history_count < g->history_size) ? g->history_count : g->history_size;
  if (stored == 0)
    return 0;

  // 获取 flags 和 lines
  struct cell *old_lines = calloc(stored * old_width, sizeof(struct cell));
  uint8_t *old_flags = calloc(stored, sizeof(uint8_t));
  if (!old_lines || !old_flags) {
    free(old_lines);
    free(old_flags);
    return -1;
  }

  for (unsigned int i = 0; i < stored; i++) {
    unsigned int idx;
    if (g->history_count <= g->history_size)
      idx = i;
    else
      // ring buff
      idx = ((g->history_count % g->history_size) + i) % g->history_size;
    memcpy(&old_lines[i * old_width], &g->history_cells[idx * old_width],
           old_width * sizeof(struct cell));
    // 复制 flags
    if (g->history_line_flags)
      old_flags[i] = g->history_line_flags[idx];
  }

  unsigned int max_out = (stored * old_width + new_width - 1) / new_width +
                         stored; // 最坏的情况下多空一行 + stored
  struct cell *out_cells = calloc(max_out * new_width, sizeof(struct cell));
  uint8_t *out_flags = calloc(max_out, sizeof(uint8_t));
  // 单行临时缓冲区
  struct cell *logical = calloc(stored * old_width, sizeof(struct cell));
  if (!out_cells || !out_flags || !logical) {
    free(old_lines);
    free(old_flags);
    free(out_cells);
    free(out_flags);
    free(logical);
    return -1;
  }

  // 初始化 out_cells 的 flags 为默认颜色，避免 padding 单元格渲染为黑色背景
  for (unsigned int j = 0; j < max_out * new_width; j++)
    out_cells[j].flags = 0x03;

  // reflow
  unsigned int out_row = 0;
  unsigned int i = 0;
  while (i < stored) {
    unsigned int logical_len = 0;

    // 收集起始行（continuation=false 或第一行）
    memcpy(&logical[logical_len], &old_lines[i * old_width],
           old_width * sizeof(struct cell));
    logical_len += old_width;
    i++;

    // 收集后续的 continuation 行（continuation=true 表示"我是前一行的延续"）
    while (i < stored && (old_flags[i] & 0x01)) {
      memcpy(&logical[logical_len], &old_lines[i * old_width],
             old_width * sizeof(struct cell));
      logical_len += old_width;
      i++;
    }

    // 裁剪末尾空白 cell（宽松判断：只看字符内容）
    while (logical_len > 0 && cell_is_visually_blank(&logical[logical_len - 1]))
      logical_len--;

    if (logical_len == 0) {
      // 空逻辑行，保留一个空行
      out_flags[out_row] = 0x00;
      out_row++;
      continue;
    }

    // 新行数
    unsigned int num_new = (logical_len + new_width - 1) / new_width;
    for (unsigned int j = 0; j < logical_len; j++)
      // (起始行数 + j所属行) * 宽度 + 所属行第几列
      out_cells[(out_row + j / new_width) * new_width + j % new_width] =
          logical[j];

    // 标记 flags：第一行是逻辑行起始（0x00），后续行是延续（0x01）
    out_flags[out_row] = 0x00;
    for (unsigned int k = 1; k < num_new; k++)
      out_flags[out_row + k] = 0x01;
    out_row += num_new;
  }

  free(logical);

  // 裁剪末尾的全空白行（vterm resize 推入的屏幕空白填充）
  while (out_row > 0) {
    struct cell *row = &out_cells[(out_row - 1) * new_width];
    int all_blank = 1;
    for (unsigned int x = 0; x < new_width; x++) {
      if (!cell_is_visually_blank(&row[x])) {
        all_blank = 0;
        break;
      }
    }
    if (all_blank)
      out_row--;
    else
      break;
  }

  // 取最后 history_size 行放入最终缓冲区
  struct cell *new_hist =
      calloc(g->history_size * new_width, sizeof(struct cell));
  uint8_t *new_flg = calloc(g->history_size, sizeof(uint8_t));
  if (!new_hist || !new_flg) {
    free(old_lines);
    free(old_flags);
    free(out_cells);
    free(out_flags);
    free(new_hist);
    free(new_flg);
    return -1;
  }

  unsigned int keep = (out_row < g->history_size) ? out_row : g->history_size;
  unsigned int skip = out_row - keep;
  memcpy(new_hist, &out_cells[skip * new_width],
         keep * new_width * sizeof(struct cell));
  memcpy(new_flg, &out_flags[skip], keep * sizeof(uint8_t));

  // 清理
  free(old_lines);
  free(old_flags);
  free(out_cells);
  free(out_flags);
  free(g->history_cells);
  free(g->history_line_flags);

  g->history_cells = new_hist;
  g->history_line_flags = new_flg;
  g->history_count = keep;

  if (g->scroll_offset > keep)
    g->scroll_offset = keep;

  return 0;
}