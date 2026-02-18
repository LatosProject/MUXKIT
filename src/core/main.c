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
#include <getopt.h>
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
int new_session_detach = -1;

static void print_help(const char *prog) {
  printf("%s", TR(MSG_HELP_TITLE));
  printf(TR(MSG_HELP_VERSION), MUXKIT_VERSION);
  printf(TR(MSG_HELP_USAGE), prog);
  printf("%s", TR(MSG_HELP_OPTIONS));
  printf("%s", TR(MSG_HELP_OPT_LIST));
  printf("%s", TR(MSG_HELP_OPT_ATTACH));
  printf("%s", TR(MSG_HELP_OPT_KILL));
  printf("%s", TR(MSG_HELP_OPT_NEW));
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
  printf(TR(MSG_HELP_EX_NEW_DETACH), prog);
}

int main(int argc, char *argv[]) {
  i18n_init();
  if (argc == 2 && strcmp(argv[1], "new-session") == 0) {
    new_session_detach = 1;
    optind = argc;
  }
  int opt;
  int option_index = 0;
  static struct option long_options[] = {
      {"h", no_argument, 0, 'h'},
      {"help", no_argument, 0, 'h'},

      {"l", no_argument, 0, 'l'},
      {"s", required_argument, 0, 's'},
      {"k", required_argument, 0, 'k'},
      {"send_keys", required_argument, 0, '_'},
      {"new-session", no_argument, 0, 'n'},
      {"n", no_argument, 0, 'n'},
      {"list-panes", required_argument, 0, 'p'},
      {0, 0, 0, 0}};

  while ((opt = getopt_long(argc, argv, "hls:k:_:np:", long_options,
                            &option_index)) != -1) {
    switch (opt) {
    case 'h':
      print_help(argv[0]);
      return 0;
    case 'l':
      list_sessions = 1;
      break;
    case 's':
      detached_session_id = strtol(optarg, NULL, 10);
      log_info("attaching to session id=%d\n", detached_session_id);
      break;
    case 'k':
      kill_session_id = strtol(optarg, NULL, 10);
      log_info("killing session id=%d\n", kill_session_id);
      break;
    case '_':
      // TODO
      break;
    case 'p':
      // TODO
      break;
    case 'n':
      new_session_detach = 1;
      break;
    case '?':
      if (optind < argc && strcmp(argv[optind], "new-session") == 0) {
        optind++;
        continue;
      }
      printf("%s", TR(MSG_ERR_COMMAND));
      return -1;
    default:
      break;
    }
  }

  // 有效解析位置
  if (optind < argc) {
    printf("%s", TR(MSG_ERR_COMMAND));
    return -1;
  }

  if (new_session_detach == 1) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      return -1;
    }
    if (pid > 0) {
      return 0; // 父进程退出
    }
    // 子进程
    if (setsid() < 0) {
      perror("setsid");
      return -1;
    }
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
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