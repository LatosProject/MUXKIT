# muxkit

**[English](README.md) | [中文](README.zh-CN.md)**

---

Welcome to **muxkit**!

muxkit is a lightweight terminal multiplexer written in C. It enables multiple terminal sessions to be created, accessed, and controlled from a single screen. muxkit sessions can be detached from the screen and continue running in the background, then later reattached.

### Features

- ✨ **Session Management** - Create, detach, and reattach terminal sessions
- 🪟 **Multiple Panes** - Split terminal window into multiple panes vertically
- 📜 **Scrollback History** - Review terminal output with history scrolling
- 🌏 **Internationalization** - Built-in English and Chinese language support
- 🚀 **Lightweight** - Minimal dependencies, fast startup
- 🔒 **Daemon Mode** - Server runs as background daemon
- 💾 **Session Persistence** - Screen state preserved when detached

### Dependencies

muxkit depends on the following libraries:

- **libvterm** - Terminal emulator library (included in `vendor/`)
- **Standard C Library** - POSIX-compliant system

To build muxkit, you need:
- C compiler (gcc or clang)
- CMake 3.10 or higher
- make

### Installation

#### From Source

To build and install muxkit from source:

```bash
# Clone the repository
git clone https://github.com/LatosProject/muxkit.git
cd muxkit

# Build
cmake -B build -S .
cmake --build build

# Install (optional)
sudo cp build/muxkit /usr/local/bin/
```

#### Platform-Specific Notes

- **macOS**: Works out of the box with Xcode Command Line Tools
- **Linux**: Requires build-essential package

### Usage

#### Basic Commands

```bash
# Start a new session
muxkit

# List all sessions
muxkit -l

# Attach to a detached session (session ID 0)
muxkit -s 0

# Kill a session
muxkit -k 0

# Show help
muxkit -h
```

#### Key Bindings

All commands are prefixed with `Ctrl+B`:

| Key Combination | Action |
|----------------|--------|
| `Ctrl+B` `d` | Detach from current session |
| `Ctrl+B` `%` | Split pane vertically |
| `Ctrl+B` `o` | Switch to next pane |
| `Ctrl+B` `[` | Scroll up (view history) |
| `Ctrl+B` `]` | Scroll down |
| `Ctrl+B` `Ctrl+B` | Send literal Ctrl+B to shell |

**Note**: Press `Esc` or `q` to exit scroll mode.

### Configuration

Configuration files are located in `/tmp/muxkit-<uid>/`:
- `keybinds.conf` - Custom key bindings (optional)

Example `keybinds.conf`:
```
prefix d detach_session
prefix % new_pane
prefix o next_pane
```

Available actions:
- `detach_session` - Detach from session
- `new_pane` - Create new pane
- `next_pane` - Switch to next pane
- `scroll_up` - Scroll up to view history
- `scroll_down` - Scroll down

### Project Structure

```
muxkit/
├── src/
│   ├── core/       # Main entry point
│   ├── client/     # Client-side logic
│   ├── server/     # Server daemon
│   ├── ui/         # Rendering and input
│   └── common/     # Shared utilities
├── include/        # Header files
├── vendor/         # Third-party libraries (libvterm)
└── CMakeLists.txt  # Build configuration
```

### Architecture

```
┌──────────────────────────────────────────────────────┐
│                     Terminal                         │
└──────────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│                 Client Process                       │
│  ┌────────┐  ┌──────────┐  ┌──────────────────────┐  │
│  │ Raw    │  │ Event    │  │ Signal Handler       │  │
│  │ Mode   │  │ Loop     │  │ (SIGWINCH/SIGCHLD)   │  │
│  └────────┘  └──────────┘  └──────────────────────┘  │
└──────────────────────────────────────────────────────┘
                        │
              Unix Domain Socket
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│              Server Process (Daemon)                 │
│  ┌────────┐  ┌──────────┐  ┌──────────────────────┐  │
│  │Session │  │ Event    │  │ Child Monitor        │  │
│  │Manager │  │ Loop     │  │ (SIGCHLD)            │  │
│  └────────┘  └──────────┘  └──────────────────────┘  │
└──────────────────────────────────────────────────────┘
                        │
                  PTY Master/Slave
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│                  Shell Process                       │
│                 (bash/zsh/sh)                        │
└──────────────────────────────────────────────────────┘
```

### Contributing

Bug reports, feature suggestions, and code contributions are welcome!

Please open a GitHub issue or pull request at:
```
https://github.com/LatosProject/muxkit
```

Before contributing:
1. Read the code style in existing files
2. Add appropriate comments and documentation
3. Test your changes thoroughly

### Documentation

For detailed API documentation, see the header files in `include/`:
- `window.h` - Window and pane management
- `render.h` - Terminal rendering
- `client.h` - Client state machine
- `server.h` - Server daemon

Generate Doxygen documentation:
```bash
doxygen Doxyfile
```

### Debugging

Run muxkit with verbose logging. Log files will be created in `/tmp/muxkit-<uid>/`:
- `client.log` - Client log
- `server.log` - Server log

Check logs for debugging:
```bash
tail -f /tmp/muxkit-$(id -u)/client.log
tail -f /tmp/muxkit-$(id -u)/server.log
```

### Technical Details

| Technology | Description |
|------------|-------------|
| **PTY (Pseudo-terminal)** | Virtual terminal pairs using `posix_openpt`, `grantpt`, `ptsname` |
| **Unix Domain Sockets** | Local IPC between client and server |
| **FD Passing** | Cross-process file descriptor passing via `SCM_RIGHTS` |
| **Signal Handling** | `SIGCHLD` for child monitoring, `SIGWINCH` for window resize |
| **Daemon Process** | Double-fork pattern for background service |
| **termios** | Raw mode terminal control |
| **libvterm** | Terminal emulation library |

### License

MIT License - Copyright (c) 2024 LatosProject

See [LICENSE](LICENSE) file for details.

---

**Version**: 0.4.3
**Author**: LatosProject
**Homepage**: https://github.com/LatosProject/muxkit
