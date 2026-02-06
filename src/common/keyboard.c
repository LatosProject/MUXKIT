/**
 * keyboard.c - muxkit 键盘处理模块实现
 *
 * 本模块实现键盘快捷键处理：
 * - 默认快捷键初始化
 * - 从配置文件加载自定义快捷键
 * - 按键分发和动作执行
 *
 * 快捷键触发流程：
 *   用户按下 Ctrl+B -> 设置 ctrl_b_pressed 标志
 *   用户按下下一个键 -> handle_key() 查表执行
 *
 * 配置文件格式 (keybinds.conf):
 *   prefix d detach_session
 *   prefix % new_pane
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

#include "keyboard.h"
#include "client.h"
#include "log.h"
#include "main.h"
#include "render.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_KEYBINDS 16
extern char *socket_path;

struct keybind {
  char key;
  enum key_table table;
  void (*handler)(struct client *c);
};

struct action_map {
  const char *name;
  void (*handler)(struct client *c);
};

void detach_session(struct client *c) { dispatch_event(c, EV_DETACHED); }
void new_pane(struct client *c) { dispatch_event(c, EV_PANE_SPLIT); }
void next_pane(struct client *c) {
  struct window_pane *next =
      list_entry(c->pane->link.next, struct window_pane, link);
  if (&next->link == &c->pane->window->panes) {
    // 到达链表头，回到第一个 pane
    next = list_entry(c->pane->window->panes.next, struct window_pane, link);
  }
  c->pane = next;
  render_pane(c->pane);
}
void scroll_up(struct client *c) {
  if (c->pane && c->pane->grid) {
    grid_scroll_up(c->pane->grid, c->pane->sy);
    render_pane(c->pane);
    render_status_bar(c);
  }
}
void scroll_down(struct client *c) {
  if (c->pane && c->pane->grid) {
    grid_scroll_down(c->pane->grid, c->pane->sy);
    render_pane(c->pane);
    render_status_bar(c);
  }
}

static struct keybind keybinds[MAX_KEYBINDS];
struct action_map actions[] = {
    {"detach_session", detach_session}, {"new_pane", new_pane},
    {"next_pane", next_pane},           {"scroll_up", scroll_up},
    {"scroll_down", scroll_down},
};
int keybind_count = 0;

void handle_key(struct client *c, enum key_table table, char key) {
  int lower_key = (int)key;
  if (isalpha(key) && isupper(key)) {
    lower_key = tolower(key);
  }
  for (int i = 0; i < keybind_count; i++) {
    if (keybinds[i].table == table && keybinds[i].key == lower_key) {
      keybinds[i].handler(c);
      return;
    }
  }
  // 没有匹配的快捷键，发送 Ctrl+B + 原字符到 PTY
  char cb = 0x02;
  write(c->pane->master_fd, &cb, 1);
  write(c->pane->master_fd, &key, 1);
}
void keybind_init() {
  keybinds[keybind_count++] = (struct keybind){'d', KEY_PREFIX, detach_session};
  keybinds[keybind_count++] = (struct keybind){'%', KEY_PREFIX, new_pane};
  keybinds[keybind_count++] = (struct keybind){'o', KEY_PREFIX, next_pane};
  keybinds[keybind_count++] = (struct keybind){'[', KEY_PREFIX, scroll_up};
  keybinds[keybind_count++] = (struct keybind){']', KEY_PREFIX, scroll_down};

  // tmp/muxkit-1000/default -> /tmp/muxkit-1000/
  char dirpath[MUXKIT_BUF_PATH];
  strncpy(dirpath, socket_path, sizeof(dirpath) - 1);
  dirpath[sizeof(dirpath) - 1] = '\0';
  char *last_slash = strrchr(dirpath, '/');
  if (last_slash) {
    *(last_slash + 1) = '\0';
  }

  char filename[MUXKIT_BUF_PATH];
  snprintf(filename, sizeof(filename), "%skeybinds.conf", dirpath);
  log_debug("keybinds config: %s", filename);
  FILE *fp = fopen(filename, "r");
  if (!fp)
    return;

  char line[128];
  void (*handler)(struct client *c) = NULL;
  int n_actions = sizeof(actions) / sizeof(actions[0]);
  while (fgets(line, sizeof(line), fp)) {
    if (line[0] == '#') {
      continue;
    }
    char table_str[16], key_char[4], action_str[32];
    if (sscanf(line, "%15s %3s %31s", table_str, key_char, action_str) != 3) {
      continue;
    }
    char key = key_char[0];
    enum key_table table = KEY_PREFIX;
    for (int i = 0; i < n_actions; i++) {
      if (strcmp(action_str, actions[i].name) == 0) {
        for (int z = 0; z < keybind_count; z++) {
          if (actions[i].handler == keybinds[z].handler) {
            keybinds[z].key = key;
          }
        }
      }
    }
  }
  fclose(fp);
};