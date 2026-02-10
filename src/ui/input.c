/**
 * input.c - muxkit 输入处理模块实现
 *
 * 本模块实现 PTY 输入处理和 VTerm 同步功能：
 * - 将 PTY 输出数据写入 libvterm 进行终端模拟
 * - 从 libvterm 同步解析后的屏幕内容到 grid
 * - 从 grid 恢复 VTerm 状态 (用于会话附加)
 * - Unicode codepoint 到 UTF-8 编码转换
 *
 * 数据流：
 *   PTY 输出 -> pane_input() -> libvterm -> sync_grid_from_vterm() -> grid
 *
 * 会话恢复流程：
 *   grid (反序列化) -> sync_vterm_from_grid() -> libvterm
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

#include "main.h"
#include "render.h"
#include "util.h"
#include "window.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
// 从 grid 同步屏幕内容到 VTerm
void sync_vterm_from_grid(struct window_pane *p) {
  if (!p->vt || !p->grid)
    return;

  struct grid *g = p->grid;
  char seq[MUXKIT_INPUT_SEQ];
  int len;

  // 清屏并重置状态
  vterm_input_write(p->vt, "\033[H\033[2J\033[0m", 11);

  uint8_t last_fg = 0, last_bg = 0, last_attr = 0, last_flags = 0x03;

  for (unsigned int y = 0; y < g->height; y++) {
    len = snprintf(seq, sizeof(seq), "\033[%u;1H", y + 1);
    vterm_input_write(p->vt, seq, len);

    for (unsigned int x = 0; x < g->width;) {
      struct cell *c = &g->cells[y * g->width + x];

      // 只在属性变化时更新
      if (c->attr != last_attr || c->fg != last_fg || c->bg != last_bg ||
          c->flags != last_flags) {
        vterm_input_write(p->vt, "\033[0m", 4);
        if (c->attr & 0x01)
          vterm_input_write(p->vt, "\033[1m", 4);
        if (c->attr & 0x02)
          vterm_input_write(p->vt, "\033[4m", 4);
        if (c->attr & 0x04)
          vterm_input_write(p->vt, "\033[3m", 4);
        if (c->attr & 0x08)
          vterm_input_write(p->vt, "\033[7m", 4);
        if (!(c->flags & 0x01)) {
          len = snprintf(seq, sizeof(seq), "\033[38;5;%um", c->fg);
          vterm_input_write(p->vt, seq, len);
        }
        if (!(c->flags & 0x02)) {
          len = snprintf(seq, sizeof(seq), "\033[48;5;%um", c->bg);
          vterm_input_write(p->vt, seq, len);
        }
        last_attr = c->attr;
        last_fg = c->fg;
        last_bg = c->bg;
        last_flags = c->flags;
      }

      if (c->ch[0]) {
        vterm_input_write(p->vt, c->ch, strlen(c->ch));
        x += (c->width > 0) ? c->width : 1;
      } else {
        vterm_input_write(p->vt, " ", 1);
        x++;
      }
    }
  }

  // 恢复保存的光标位置
  len = snprintf(seq, sizeof(seq), "\033[%u;%uH", p->cy + 1, p->cx + 1);
  vterm_input_write(p->vt, seq, len);
}

// 从 libvterm 同步屏幕内容到 grid
void sync_grid_from_vterm(struct window_pane *p) {
  if (!p->vts || !p->grid)
    return;

  for (unsigned int y = 0; y < p->sy; y++) {
    for (unsigned int x = 0; x < p->sx; x++) {
      // 终端解析完成的数据
      VTermPos pos = {.row = y, .col = x};
      VTermScreenCell cell;
      memset(&cell, 0, sizeof(cell));
      vterm_screen_get_cell(p->vts, pos, &cell);

      // grid 中的单元格
      struct cell *c = &p->grid->cells[y * p->grid->width + x];
      memset(c, 0, sizeof(*c));

      if (cell.chars[0]) {
        unicode_to_utf8(cell.chars[0], c->ch);
      } else {
        c->ch[0] = 0;
      }
      c->width = cell.width; // 始终从 libvterm 获取宽度

      // 提取颜色
      c->flags = 0;
      if (VTERM_COLOR_IS_DEFAULT_FG(&cell.fg)) {
        c->flags |= 0x01; // 使用默认前景色
      } else if (VTERM_COLOR_IS_INDEXED(&cell.fg)) {
        c->fg = cell.fg.indexed.idx;
      } else if (VTERM_COLOR_IS_RGB(&cell.fg)) {
        // RGB 转 216 色立方体 + 灰度
        uint8_t r = cell.fg.rgb.red, g = cell.fg.rgb.green,
                b = cell.fg.rgb.blue;
        c->fg = 16 + (r / 51) * 36 + (g / 51) * 6 + (b / 51);
      }

      if (VTERM_COLOR_IS_DEFAULT_BG(&cell.bg)) {
        c->flags |= 0x02; // 使用默认背景色
      } else if (VTERM_COLOR_IS_INDEXED(&cell.bg)) {
        c->bg = cell.bg.indexed.idx;
      } else if (VTERM_COLOR_IS_RGB(&cell.bg)) {
        uint8_t r = cell.bg.rgb.red, g = cell.bg.rgb.green,
                b = cell.bg.rgb.blue;
        c->bg = 16 + (r / 51) * 36 + (g / 51) * 6 + (b / 51);
      }

      // 提取属性
      c->attr = 0;
      if (cell.attrs.bold)
        c->attr |= 0x01;
      if (cell.attrs.underline)
        c->attr |= 0x02;
      if (cell.attrs.italic)
        c->attr |= 0x04;
      if (cell.attrs.reverse)
        c->attr |= 0x08;
    }
  }

  // 同步光标位置
  VTermPos cursor;
  VTermState *state = vterm_obtain_state(p->vt); // state 状态机跟踪光标位置
  vterm_state_get_cursorpos(state, &cursor);     // 查询光标
  p->cx = (unsigned int)cursor.col;
  p->cy = (unsigned int)cursor.row;

  // 同步行标志 (continuation)
  if (p->grid->line_flags) {
    for (unsigned int y = 0; y < p->sy; y++) {
      const VTermLineInfo *info = vterm_state_get_lineinfo(state, y);
      p->grid->line_flags[y] = (info && info->continuation) ? 0x01 : 0;
    }
  }
}

/*
  网格输入到 vterm
*/
void pane_input(struct window_pane *p, const char *data, size_t len) {
  if (!p->vt)
    return;

  vterm_input_write(p->vt, data, len);
  sync_grid_from_vterm(p);
}
