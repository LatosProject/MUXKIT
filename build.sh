#!/bin/bash
# 构建脚本 - 支持多架构编译

set -e

VERSION="1.0.0"
BUILD_DIR="build"

# 显示帮助
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --native       Build for native architecture (default)"
    echo "  --amd64        Cross-compile for amd64/x86_64"
    echo "  --arm64        Cross-compile for arm64/aarch64"
    echo "  --all          Build for all architectures"
    echo "  --clean        Clean build directory"
    echo "  -h, --help     Show this help"
    echo ""
    echo "Examples:"
    echo "  $0              # Build for current architecture"
    echo "  $0 --arm64      # Cross-compile for ARM64"
    echo "  $0 --all        # Build for all architectures"
}

# 清理
clean() {
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
}

# 构建指定架构
build_arch() {
    local arch=$1
    local cmake_args=""

    case $arch in
        native)
            echo "=== Building for native architecture ==="
            mkdir -p "$BUILD_DIR/native"
            cd "$BUILD_DIR/native"
            cmake ../..
            ;;
        amd64)
            echo "=== Building for amd64 ==="
            mkdir -p "$BUILD_DIR/amd64"
            cd "$BUILD_DIR/amd64"
            # 需要安装: apt install gcc-x86-64-linux-gnu
            cmake ../.. -DCMAKE_C_COMPILER=x86_64-linux-gnu-gcc -DTARGET_ARCH=x86_64
            ;;
        arm64)
            echo "=== Building for arm64 ==="
            mkdir -p "$BUILD_DIR/arm64"
            cd "$BUILD_DIR/arm64"
            # 需要安装: apt install gcc-aarch64-linux-gnu
            cmake ../.. -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DTARGET_ARCH=aarch64
            ;;
        *)
            echo "Unknown architecture: $arch"
            exit 1
            ;;
    esac

    make -j$(nproc)
    cd ../..

    echo "Built: $BUILD_DIR/$arch/muxkit-$VERSION-*"
    ls -la "$BUILD_DIR/$arch"/muxkit-* 2>/dev/null || true
}

# 主逻辑
if [ $# -eq 0 ]; then
    build_arch native
    exit 0
fi

case $1 in
    --native)
        build_arch native
        ;;
    --amd64)
        build_arch amd64
        ;;
    --arm64)
        build_arch arm64
        ;;
    --all)
        build_arch native
        build_arch amd64
        build_arch arm64
        echo ""
        echo "=== All builds complete ==="
        find "$BUILD_DIR" -name "muxkit-*" -type f ! -name "*.o" -exec ls -la {} \;
        ;;
    --clean)
        clean
        ;;
    -h|--help)
        show_help
        ;;
    *)
        echo "Unknown option: $1"
        show_help
        exit 1
        ;;
esac
