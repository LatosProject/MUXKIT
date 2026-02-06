/**
 * keyboard.h - muxkit 键盘处理模块
 *
 * 定义键盘快捷键处理接口：
 * - handle_key: 处理 Ctrl+B 前缀后的按键
 * - keybind_init: 初始化默认快捷键并加载配置文件
 *
 * 默认快捷键 (Ctrl+B 后):
 *   d - 分离会话
 *   % - 分割窗格
 *   o - 切换窗格
 *   [ - 向上滚动
 *   ] - 向下滚动
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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "client.h"

/**
 * 按键表枚举
 * 目前只有 PREFIX 表 (Ctrl+B 后)
 */
enum key_table { KEY_PREFIX };

/**
 * 处理按键
 * @param c     客户端上下文
 * @param table 按键表
 * @param key   按键字符
 */
void handle_key(struct client *c, enum key_table table, char key);

/**
 * 初始化快捷键
 * 设置默认快捷键并从配置文件加载自定义设置
 */
void keybind_init(void);

#endif /* KEYBOARD_H */
