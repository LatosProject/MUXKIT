/**
 * main.c - muxkit 程序入口
 *
 * 本模块是 muxkit 终端复用器的主入口点，负责：
 * - 解析命令行参数 (-h, -l, -s, -k)
 * - 初始化运行时目录 (/tmp/muxkit-<uid>/)
 * - 设置 Unix 域套接字路径
 * - 启动客户端主循环
 *
 * 支持的命令行选项：
 *   -h, --help  显示帮助信息
 *   -l          列出所有会话
 *   -s <id>     附加到指定会话
 *   -k <id>     终止指定会话
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
#include "client.h"
#include "i18n.h"
#include "log.h"
#include "version.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
struct client client;
char *socket_path;
int detached_session_id = -1;
int list_sessions = 0;
int kill_session_id = -1;

static void print_help(const char *prog) {
  printf("%s", TR(MSG_HELP_TITLE));
  printf(TR(MSG_HELP_VERSION), MUXKIT_VERSION);
  printf(TR(MSG_HELP_USAGE), prog);
  printf("%s", TR(MSG_HELP_OPTIONS));
  printf("%s", TR(MSG_HELP_OPT_LIST));
  printf("%s", TR(MSG_HELP_OPT_ATTACH));
  printf("%s", TR(MSG_HELP_OPT_KILL));
  printf("%s", TR(MSG_HELP_OPT_HELP));
  printf("%s", TR(MSG_HELP_KEYBINDINGS));
  printf("%s", TR(MSG_HELP_KEY_DETACH));
  printf("%s", TR(MSG_HELP_KEY_SPLIT));
  printf("%s", TR(MSG_HELP_KEY_NEXT));
  printf("%s", TR(MSG_HELP_KEY_SCROLL_UP));
  printf("%s", TR(MSG_HELP_KEY_SCROLL_DOWN));
  printf("%s", TR(MSG_HELP_EXAMPLES));
  printf(TR(MSG_HELP_EX_NEW), prog);
  printf(TR(MSG_HELP_EX_LIST), prog);
  printf(TR(MSG_HELP_EX_ATTACH), prog);
  printf(TR(MSG_HELP_EX_KILL), prog);
}

int main(int argc, char *argv[]) {
  i18n_init();

  if (argc == 2 &&
      (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    print_help(argv[0]);
    return 0;
  }
  if (argc == 2 && (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "-L") == 0)) {
    list_sessions = 1;
  }
  if (argc == 3 && (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "-S") == 0)) {
    detached_session_id = strtol(argv[2], NULL, 10);
    log_info("attaching to session id=%d\n", detached_session_id);
  }
  if (argc == 3 && (strcmp(argv[1], "-k") == 0 || strcmp(argv[1], "-K") == 0)) {
    kill_session_id = strtol(argv[2], NULL, 10);
    log_info("killing session id=%d\n", kill_session_id);
  }
  uid_t uid = getuid();

  // 初始化程序目录
  char dir[MUXKIT_BUF_SMALL] = {0};
  snprintf(dir, sizeof(dir), "%smuxkit-%d", MUXKIT_SOCK, uid);
  if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
    perror(TR(MSG_ERR_MKDIR));
    return -1;
  }
  struct stat info;
  if (lstat(dir, &info) != 0 || !(info.st_mode & S_IFDIR)) {
    perror(TR(MSG_ERR_STAT));
    return -1;
  }

  static char buf[MUXKIT_BUF_SMALL] = {0};
  snprintf(buf, sizeof(buf), "%s/default", dir);
  socket_path = buf;
  client_init(&client);
  if (client_main(&client) < 0) {
    return -1;
  }
  return 0;
}