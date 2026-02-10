/**
 * input.h - muxkit 输入处理模块
 *
 * 定义 PTY 输入处理接口：
 * - pane_input: 处理来自 PTY 的数据，通过 libvterm 解析
 * - sync_vterm_from_grid: 从 grid 同步屏幕内容到 VTerm
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

#ifndef INPUT_H
#define INPUT_H

#include "window.h"
#include <stddef.h>

/**
 * @brief 处理 PTY 输入数据
 *
 * 数据流：PTY → libvterm 解析 → 同步到 grid。
 * libvterm 会解析 ANSI 转义序列并更新屏幕状态。
 *
 * @param p    窗格指针
 * @param data 输入数据缓冲区
 * @param len  数据长度（字节）
 */
void pane_input(struct window_pane *p, const char *data, size_t len);

/**
 * @brief 从 grid 同步屏幕内容到 VTerm
 *
 * 用于会话附加时恢复 VTerm 内部状态。
 * 通过发送 ANSI 转义序列重建屏幕内容和属性。
 *
 * @param p 窗格指针
 */
void sync_vterm_from_grid(struct window_pane *p);

/**
 * @brief 从 libvterm 同步屏幕内容到 grid
 *
 * 将 libvterm 解析后的屏幕内容同步到 grid 数据结构。
 * 包括字符、颜色、属性和光标位置。
 *
 * @param p 窗格指针
 */
void sync_grid_from_vterm(struct window_pane *p);

#endif /* INPUT_H */
