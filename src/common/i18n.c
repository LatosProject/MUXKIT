/**
 * i18n.c - 国际化 (Internationalization) 模块
 *
 * 本模块实现了 muxkit 的多语言支持功能：
 * - 支持英文 (en) 和中文 (zh) 两种语言
 * - 根据系统环境变量 (LANG, LC_ALL, LC_MESSAGES) 自动检测语言
 * - 提供统一的翻译接口 _() 宏用于获取本地化字符串
 *
 * 使用方法：
 *   1. 程序启动时调用 i18n_init() 初始化语言设置
 *   2. 使用 TR(MSG_XXX) 宏获取翻译后的字符串
 *   3. 可通过 i18n_set_language() 手动切换语言
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

#include "i18n.h"
#include <stdlib.h>
#include <string.h>

/* 当前语言设置，默认为英文 */
static language_t current_lang = LANG_EN;

/*
 * 英文翻译表
 * 使用 C99 指定初始化器语法，按消息 ID 索引
 */
static const char *messages_en[MSG_COUNT] = {
    /* 帮助信息 - 显示在 -h 选项输出中 */
    [MSG_HELP_TITLE] = "muxkit - a minimal terminal multiplexer\n\n",
    [MSG_HELP_VERSION] = "        Version: %s By LatosProject\n\n",
    [MSG_HELP_USAGE] = "Usage: %s [options]\n\n",
    [MSG_HELP_OPTIONS] = "Options:\n",
    [MSG_HELP_OPT_LIST] = "  -l         List all sessions\n",
    [MSG_HELP_OPT_ATTACH] = "  -s <id>    Attach to detached session by id\n",
    [MSG_HELP_OPT_KILL] = "  -k <id>    Kill session by id\n",
    [MSG_HELP_OPT_HELP] = "  -h         Show this help message\n\n",
    [MSG_HELP_KEYBINDINGS] = "Key bindings:\n",
    [MSG_HELP_KEY_DETACH] = "  Ctrl+B d   Detach from current session\n",
    [MSG_HELP_KEY_SPLIT] = "  Ctrl+B %   Split pane vertically\n",
    [MSG_HELP_KEY_NEXT] = "  Ctrl+B o   Switch to next pane\n",
    [MSG_HELP_KEY_SCROLL_UP] = "  Ctrl+B [   Scroll up (view history)\n",
    [MSG_HELP_KEY_SCROLL_DOWN] = "  Ctrl+B ]   Scroll down\n\n",
    [MSG_HELP_EXAMPLES] = "Examples:\n",
    [MSG_HELP_EX_NEW] = "  %s           Start a new session\n",
    [MSG_HELP_EX_LIST] = "  %s -l        List all sessions\n",
    [MSG_HELP_EX_ATTACH] = "  %s -s 0      Attach to session 0\n",
    [MSG_HELP_EX_KILL] = "  %s -k 0      Kill session 0\n",

    /* 错误信息 - 各类操作失败时显示 */
    [MSG_ERR_MKDIR] = "mkdir failed\n",
    [MSG_ERR_STAT] = "stat failed\n",
    [MSG_ERR_FORK] = "Fork failed\n",
    [MSG_ERR_OPEN_PTY] = "open slave pty failed\n",
    [MSG_ERR_EXEC] = "Execve failed\n",
    [MSG_ERR_PROTOCOL_VERSION] = "protocol version mismatch\n",

    /* 会话管理 - 会话列表和操作反馈 */
    [MSG_SESSION_FORMAT] = "%d: %s (pid %d)\n",
    [MSG_NO_SESSIONS] = "(no sessions)\n",
    [MSG_SESSION_KILLED] = "killed session %d\n",
    [MSG_SESSION_NOT_FOUND] = "session %d not found\n",
    [MSG_ATTACH_FAILED] =
        "attach failed: session %d not found or not detached\n",
    [MSG_NESTED_WARNING] = "sessions should be nested with care\n",

    /* 状态栏 - 底部状态栏显示的文本 */
    [MSG_STATUS_HISTORY] = "[history]",

    /* 窗口名称 - 窗口标题显示 */
    [MSG_WINDOW_NEW] = "New Window",
    [MSG_WINDOW_ATTACHED] = "Attached Window",
};

/*
 * 中文翻译表
 * 与英文表结构相同，提供简体中文本地化
 */
static const char *messages_zh[MSG_COUNT] = {
    /* 帮助信息 */
    [MSG_HELP_TITLE] = "muxkit - 轻量级终端复用器\n\n",
    [MSG_HELP_VERSION] = "        版本: %s 作者: LatosProject\n\n",
    [MSG_HELP_USAGE] = "用法: %s [选项]\n\n",
    [MSG_HELP_OPTIONS] = "选项:\n",
    [MSG_HELP_OPT_LIST] = "  -l         列出所有会话\n",
    [MSG_HELP_OPT_ATTACH] = "  -s <id>    连接到指定会话\n",
    [MSG_HELP_OPT_KILL] = "  -k <id>    终止指定会话\n",
    [MSG_HELP_OPT_HELP] = "  -h         显示帮助信息\n\n",
    [MSG_HELP_KEYBINDINGS] = "快捷键:\n",
    [MSG_HELP_KEY_DETACH] = "  Ctrl+B d   分离当前会话\n",
    [MSG_HELP_KEY_SPLIT] = "  Ctrl+B %   垂直分割窗格\n",
    [MSG_HELP_KEY_NEXT] = "  Ctrl+B o   切换到下一窗格\n",
    [MSG_HELP_KEY_SCROLL_UP] = "  Ctrl+B [   向上滚动(查看历史)\n",
    [MSG_HELP_KEY_SCROLL_DOWN] = "  Ctrl+B ]   向下滚动\n\n",
    [MSG_HELP_EXAMPLES] = "示例:\n",
    [MSG_HELP_EX_NEW] = "  %s           启动新会话\n",
    [MSG_HELP_EX_LIST] = "  %s -l        列出所有会话\n",
    [MSG_HELP_EX_ATTACH] = "  %s -s 0      连接到会话 0\n",
    [MSG_HELP_EX_KILL] = "  %s -k 0      终止会话 0\n",

    /* 错误信息 - 各类操作失败时显示 */
    [MSG_ERR_MKDIR] = "创建目录失败\n",
    [MSG_ERR_STAT] = "获取文件状态失败\n",
    [MSG_ERR_FORK] = "创建进程失败\n",
    [MSG_ERR_OPEN_PTY] = "打开伪终端失败\n",
    [MSG_ERR_EXEC] = "执行程序失败\n",
    [MSG_ERR_PROTOCOL_VERSION] = "协议版本错误\n",

    /* 会话管理 - 会话列表和操作反馈 */
    [MSG_SESSION_FORMAT] = "%d: %s (进程号 %d)\n",
    [MSG_NO_SESSIONS] = "(无会话)\n",
    [MSG_SESSION_KILLED] = "已终止会话 %d\n",
    [MSG_SESSION_NOT_FOUND] = "会话 %d 不存在\n",
    [MSG_ATTACH_FAILED] = "连接失败: 会话 %d 不存在或未分离\n",
    [MSG_NESTED_WARNING] = "警告: 不建议嵌套运行会话\n",

    /* 状态栏 - 底部状态栏显示的文本 */
    [MSG_STATUS_HISTORY] = "[历史]",

    /* 窗口名称 - 窗口标题显示 */
    [MSG_WINDOW_NEW] = "新窗口",
    [MSG_WINDOW_ATTACHED] = "已连接窗口",
};

/**
 * i18n_init - 初始化国际化模块
 *
 * 检测系统语言环境变量，按以下优先级：
 * 1. LANG
 * 2. LC_ALL
 * 3. LC_MESSAGES
 *
 * 如果检测到 "zh" 开头的语言代码，则使用中文；
 * 否则默认使用英文。
 */
void i18n_init(void) {
  const char *lang = getenv("LANG");
  if (lang == NULL) {
    lang = getenv("LC_ALL");
  }
  if (lang == NULL) {
    lang = getenv("LC_MESSAGES");
  }

  if (lang != NULL) {
    if (strncmp(lang, "zh", 2) == 0) {
      current_lang = LANG_ZH;
    } else {
      current_lang = LANG_EN;
    }
  }
}

/**
 * i18n_set_language - 手动设置当前语言
 * @lang: 目标语言 (LANG_EN 或 LANG_ZH)
 */
void i18n_set_language(language_t lang) { current_lang = lang; }

/**
 * i18n_get_language - 获取当前语言设置
 * @return: 当前语言枚举值
 */
language_t i18n_get_language(void) { return current_lang; }

/**
 * _ - 翻译函数 (Translation function)
 * @id: 消息 ID (message_id_t 枚举值)
 * @return: 当前语言对应的翻译字符串
 *
 * 这是核心翻译接口，通常通过 TR() 宏调用。
 * 如果当前语言的翻译不存在，则回退到英文。
 */
const char *_(message_id_t id) {
  if (id < 0 || id >= MSG_COUNT) {
    return "";
  }

  const char *msg = NULL;
  switch (current_lang) {
  case LANG_ZH:
    msg = messages_zh[id];
    break;
  case LANG_EN:
  default:
    msg = messages_en[id];
    break;
  }

  return msg ? msg : messages_en[id];
}
