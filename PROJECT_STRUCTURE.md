# muxkit 项目结构说明

## 目录结构

```
muxkit/
├── src/                      # 源代码目录
│   ├── core/                # 核心模块
│   │   └── main.c          # 程序入口点
│   ├── client/              # 客户端模块
│   │   └── client.c        # 客户端状态机和事件处理
│   ├── server/              # 服务端模块
│   │   ├── server.c        # 服务端守护进程和会话管理
│   │   └── spawn.c         # Shell 进程创建
│   ├── ui/                  # 用户界面模块
│   │   ├── window.c        # 窗口和窗格管理
│   │   ├── render.c        # 终端渲染和历史滚动
│   │   └── input.c         # PTY 输入处理和 VTerm 同步
│   └── common/              # 公共工具模块
│       ├── util.c          # 通用工具函数
│       ├── log.c           # 日志系统
│       ├── i18n.c          # 国际化支持
│       └── keyboard.c      # 键盘快捷键处理
├── include/                 # 头文件目录
│   ├── client.h
│   ├── server.h
│   ├── spawn.h
│   ├── window.h
│   ├── render.h
│   ├── input.h
│   ├── util.h
│   ├── log.h
│   ├── i18n.h
│   ├── keyboard.h
│   ├── main.h
│   ├── list.h              # 双向链表实现
│   ├── version.h           # 版本信息
│   └── muxkit-protocol.h   # 客户端-服务端通信协议
├── vendor/                  # 第三方库
│   └── vterm/              # libvterm 终端模拟器
├── build/                   # 构建输出目录 (git ignored)
├── CMakeLists.txt          # CMake 构建配置
├── build.sh                # 构建脚本
├── keybinds.conf           # 键盘快捷键配置
├── README.md               # 项目说明文档
├── LICENSE                 # MIT 许可证
├── .gitignore              # Git 忽略规则
└── PROJECT_STRUCTURE.md    # 本文档

## 模块说明

### Core 模块
- **main.c**: 程序入口，负责命令行参数解析、运行时目录初始化、客户端启动

### Client 模块
- **client.c**: 客户端核心，实现有限状态机 (FSM)，处理终端输入输出、窗口调整、会话分离等

### Server 模块
- **server.c**: 服务端守护进程，负责会话管理、客户端连接管理、多窗格支持
- **spawn.c**: 在 PTY 上创建 shell 子进程

### UI 模块
- **window.c**: 窗口和窗格管理，libvterm 集成
- **render.c**: 终端渲染、历史滚动、屏幕网格序列化
- **input.c**: PTY 输入处理、VTerm 同步、UTF-8 编码转换

### Common 模块
- **util.c**: 通用工具函数（shell 检测、文件描述符传递等）
- **log.c**: 日志系统实现
- **i18n.c**: 国际化支持（英语/中文）
- **keyboard.c**: 键盘快捷键处理和配置加载

## 构建说明

### 依赖
- CMake 3.10+
- C99 编译器 (GCC/Clang)
- libvterm (已包含在 vendor/)

### 构建步骤
```bash
# 创建构建目录
mkdir -p build && cd build

# 配置项目
cmake ..

# 编译 (使用 4 个并行任务)
make -j4

# 或使用快捷脚本
./build.sh
```

### 构建输出
- `build/muxkit-VERSION-ARCH`: 可执行文件
- `build/muxkit`: 符号链接指向可执行文件

### Debug 构建
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

Debug 模式会启用日志输出。

## 代码规范

### 文件组织
- 源文件 (.c) 放在 `src/` 的对应子目录
- 头文件 (.h) 统一放在 `include/`
- 每个模块的源文件和头文件名称对应

### 头文件引用
```c
#include "client.h"     // 项目头文件
#include <stdio.h>      // 系统头文件
```

### 命名约定
- 函数: `module_function_name()` (如 `client_init()`)
- 结构体: `struct name` (如 `struct client`)
- 宏: `MODULE_CONSTANT` (如 `MUXKIT_BUF_LARGE`)

## 许可证
MIT License - Copyright (c) 2024 LatosProject
