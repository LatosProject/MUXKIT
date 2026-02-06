# muxkit

**[English](README.md) | [中文](README.zh-CN.md)**

---

欢迎使用 **muxkit**！

muxkit 是一个用 C 语言编写的轻量级终端复用器。它允许在单个屏幕中创建、访问和控制多个终端会话。muxkit 会话可以从屏幕分离并在后台继续运行，之后再重新附加。

### 功能特性

- ✨ **会话管理** - 创建、分离和重新附加终端会话
- 🪟 **多窗格支持** - 垂直分割终端窗口为多个窗格
- 📜 **历史回滚** - 通过历史滚动查看终端输出
- 🌏 **国际化** - 内置英文和中文语言支持
- 🚀 **轻量级** - 最小依赖，快速启动
- 🔒 **守护进程模式** - 服务器作为后台守护进程运行
- 💾 **会话持久化** - 分离时保留屏幕状态

### 依赖项

muxkit 依赖以下库：

- **libvterm** - 终端模拟器库（包含在 `vendor/` 目录）
- **标准 C 库** - POSIX 兼容系统

构建 muxkit 需要：
- C 编译器（gcc 或 clang）
- CMake 3.10 或更高版本
- make

### 安装

#### 从源码编译

从源码构建和安装 muxkit：

```bash
# 克隆仓库
git clone https://github.com/LatosProject/muxkit.git
cd muxkit

# 构建
cmake -B build -S .
cmake --build build

# 安装（可选）
sudo cp build/muxkit /usr/local/bin/
```

#### 平台特定说明

- **macOS**：使用 Xcode 命令行工具即可直接运行
- **Linux**：需要安装 build-essential 软件包

### 使用方法

#### 基本命令

```bash
# 启动新会话
muxkit

# 列出所有会话
muxkit -l

# 附加到分离的会话（会话 ID 为 0）
muxkit -s 0

# 终止会话
muxkit -k 0

# 显示帮助
muxkit -h
```

#### 快捷键

所有命令都以 `Ctrl+B` 为前缀：

| 组合键 | 功能 |
|--------|------|
| `Ctrl+B` `d` | 分离当前会话 |
| `Ctrl+B` `%` | 垂直分割窗格 |
| `Ctrl+B` `o` | 切换到下一窗格 |
| `Ctrl+B` `[` | 向上滚动（查看历史） |
| `Ctrl+B` `]` | 向下滚动 |
| `Ctrl+B` `Ctrl+B` | 发送 Ctrl+B 到 shell |

**注意**：按 `Esc` 或 `q` 退出滚动模式。

### 配置

配置文件位于 `/tmp/muxkit-<uid>/`：
- `keybinds.conf` - 自定义快捷键（可选）

`keybinds.conf` 示例：
```
prefix d detach_session
prefix % new_pane
prefix o next_pane
```

可用的操作：
- `detach_session` - 分离会话
- `new_pane` - 新建窗格
- `next_pane` - 切换到下一个窗格
- `scroll_up` - 向上滚动查看历史
- `scroll_down` - 向下滚动

### 项目结构

```
muxkit/
├── src/
│   ├── core/       # 主入口
│   ├── client/     # 客户端逻辑
│   ├── server/     # 服务器守护进程
│   ├── ui/         # 渲染和输入
│   └── common/     # 共享工具
├── include/        # 头文件
├── vendor/         # 第三方库（libvterm）
└── CMakeLists.txt  # 构建配置
```

### 架构设计

```
┌──────────────────────────────────────────────────────┐
│                      终端                            │
└──────────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│                   客户端进程                         │
│  ┌────────┐  ┌──────────┐  ┌──────────────────────┐  │
│  │ Raw    │  │ 事件循环 │  │ 信号处理             │  │
│  │ 模式   │  │          │  │ (SIGWINCH/SIGCHLD)   │  │
│  └────────┘  └──────────┘  └──────────────────────┘  │
└──────────────────────────────────────────────────────┘
                        │
                Unix 域套接字
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│               服务器进程（守护进程）                 │
│  ┌────────┐  ┌──────────┐  ┌──────────────────────┐  │
│  │ 会话   │  │ 事件循环 │  │ 子进程监控           │  │
│  │ 管理器 │  │          │  │ (SIGCHLD)            │  │
│  └────────┘  └──────────┘  └──────────────────────┘  │
└──────────────────────────────────────────────────────┘
                        │
                  PTY 主/从端
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│                   Shell 进程                         │
│                 (bash/zsh/sh)                        │
└──────────────────────────────────────────────────────┘
```

### 贡献

欢迎提交 Bug 报告、功能建议和代码贡献！

请在以下地址提交 GitHub issue 或 pull request：
```
https://github.com/LatosProject/muxkit
```

贡献前请：
1. 阅读现有文件中的代码风格
2. 添加适当的注释和文档
3. 充分测试您的更改

### 文档

详细的 API 文档请参见 `include/` 目录下的头文件：
- `window.h` - 窗口和窗格管理
- `render.h` - 终端渲染
- `client.h` - 客户端状态机
- `server.h` - 服务器守护进程

生成 Doxygen 文档：
```bash
doxygen Doxyfile
```

### 调试

使用详细日志运行 muxkit。日志文件将创建在 `/tmp/muxkit-<uid>/`：
- `client.log` - 客户端日志
- `server.log` - 服务器日志

查看调试日志：
```bash
tail -f /tmp/muxkit-$(id -u)/client.log
tail -f /tmp/muxkit-$(id -u)/server.log
```

### 技术细节

| 技术 | 说明 |
|------|------|
| **PTY（伪终端）** | 使用 `posix_openpt`、`grantpt`、`ptsname` 创建虚拟终端对 |
| **Unix 域套接字** | 客户端与服务端之间的本地进程间通信 |
| **文件描述符传递** | 通过 `SCM_RIGHTS` 跨进程传递文件描述符 |
| **信号处理** | `SIGCHLD` 监控子进程退出，`SIGWINCH` 响应窗口大小变化 |
| **守护进程** | Double-fork 模式创建后台服务进程 |
| **termios** | 终端 raw 模式控制，禁用行缓冲和回显 |
| **libvterm** | 终端模拟库，解析转义序列，维护屏幕缓冲区 |

### 参考资料

本项目参考了 [tmux](https://github.com/tmux/tmux) 的设计思想，适合学习 Unix 系统编程。

涉及的知识点：
- 《Advanced Programming in the UNIX Environment》(APUE)
- POSIX 标准 PTY 接口
- Unix 域套接字编程
- 守护进程创建

### 许可证

MIT License - Copyright (c) 2024 LatosProject

详情请见 [LICENSE](LICENSE) 文件。

---

**版本**: 0.4.3
**作者**: LatosProject
**主页**: https://github.com/LatosProject/muxkit
