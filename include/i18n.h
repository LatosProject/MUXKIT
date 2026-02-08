/**
 * i18n.h - muxkit 国际化模块
 *
 * 提供多语言支持：
 * - 自动检测系统语言环境 (LANG/LC_ALL)
 * - 支持英语和中文
 * - 使用 TR() 宏获取翻译字符串
 *
 * 使用方法：
 *   i18n_init();
 *   printf("%s", TR(MSG_HELP_TITLE));
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

#ifndef I18N_H
#define I18N_H

/**
 * 语言枚举
 */
typedef enum {
  LANG_EN, /* English */
  LANG_ZH, /* 中文 */
} language_t;

/**
 * 消息 ID 枚举
 */
typedef enum {
  /* 帮助信息 */
  MSG_HELP_TITLE,
  MSG_HELP_VERSION,
  MSG_HELP_USAGE,
  MSG_HELP_OPTIONS,
  MSG_HELP_OPT_LIST,
  MSG_HELP_OPT_ATTACH,
  MSG_HELP_OPT_KILL,
  MSG_HELP_OPT_HELP,
  MSG_HELP_KEYBINDINGS,
  MSG_HELP_KEY_DETACH,
  MSG_HELP_KEY_SPLIT,
  MSG_HELP_KEY_NEXT,
  MSG_HELP_KEY_SCROLL_UP,
  MSG_HELP_KEY_SCROLL_DOWN,
  MSG_HELP_EXAMPLES,
  MSG_HELP_EX_NEW,
  MSG_HELP_EX_LIST,
  MSG_HELP_EX_ATTACH,
  MSG_HELP_EX_KILL,

  /* 错误信息 */
  MSG_ERR_MKDIR,
  MSG_ERR_STAT,
  MSG_ERR_FORK,
  MSG_ERR_OPEN_PTY,
  MSG_ERR_EXEC,
  MSG_ERR_PROTOCOL_VERSION,

  /* 会话管理 */
  MSG_SESSION_FORMAT,
  MSG_NO_SESSIONS,
  MSG_SESSION_KILLED,
  MSG_SESSION_NOT_FOUND,
  MSG_ATTACH_FAILED,
  MSG_NESTED_WARNING,

  /* 状态栏 */
  MSG_STATUS_HISTORY,

  /* 窗口名称 */
  MSG_WINDOW_NEW,
  MSG_WINDOW_ATTACHED,

  MSG_COUNT /* 消息总数 */
} message_id_t;

/**
 * 初始化 i18n，自动检测语言环境
 */
void i18n_init(void);

/**
 * 设置语言
 * @param lang 语言
 */
void i18n_set_language(language_t lang);

/**
 * 获取当前语言
 * @return 当前语言
 */
language_t i18n_get_language(void);

/**
 * 获取翻译字符串
 * @param id 消息 ID
 * @return 翻译后的字符串
 */
const char *_(message_id_t id);

/** 便捷宏 */
#define TR(id) _(id)

#endif /* I18N_H */
