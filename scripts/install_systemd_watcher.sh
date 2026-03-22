#!/bin/bash
set -euo pipefail

# 获取当前绝对路径
PROJECT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
BUILD_DIR="$PROJECT_DIR/build"
BINARY="$BUILD_DIR/file_sorter"

if [ ! -f "$BINARY" ]; then
    echo "错误：找不到可执行文件 '$BINARY'。"
    echo "请先运行 ./build.sh 来编译项目。"
    exit 1
fi

# 确保系统已安装 lsof，这是检测文件锁的核心依赖
if ! command -v lsof &> /dev/null; then
    echo "错误：系统未安装 'lsof' 工具。"
    echo "请运行 'sudo apt install lsof' (Debian/Ubuntu) 或 'sudo dnf install lsof' (CentOS/Fedora) 后重试。"
    exit 1
fi

# 从 config.yaml 中提取 source_dir
SOURCE_DIR_RAW=$(grep '^\s*source_dir:' config.yaml | sed -e "s/.*: *['\"]//" -e "s/['\"] *$//")
if [ -z "$SOURCE_DIR_RAW" ]; then
    echo "错误：无法在 config.yaml 中找到 'source_dir'。"
    exit 1
fi

if [[ "$SOURCE_DIR_RAW" = /* ]]; then
    SOURCE_DIR="$SOURCE_DIR_RAW"
else
    CLEAN_PATH="${SOURCE_DIR_RAW#./}"
    SOURCE_DIR="$PROJECT_DIR/$CLEAN_PATH"
fi

mkdir -p "$SOURCE_DIR"

echo "Project Directory: $PROJECT_DIR"
echo "Watching Input Directory: $SOURCE_DIR"

SYSTEMD_USER_DIR="$HOME/.config/systemd/user"
mkdir -p "$SYSTEMD_USER_DIR"

SERVICE_FILE="$SYSTEMD_USER_DIR/file-sorter.service"
PATH_FILE="$SYSTEMD_USER_DIR/file-sorter.path"
WRAPPER_SCRIPT="$SYSTEMD_USER_DIR/file-sorter-wrapper.sh"

# 1. 生成基于 lsof 的防抖包装器 (Wrapper Script)
# 注意：使用 'EOF' 确保内部的 $1 和 $2 原样保留，不在当前脚本展开
cat > "$WRAPPER_SCRIPT" << 'EOF'
#!/bin/bash
SOURCE_DIR="$1"
BINARY="$2"

echo "[Info] Trigger received. Starting IO lock check for: $SOURCE_DIR"

while true; do
    # lsof +D 会递归检查目录下所有被打开的文件
    # grep -q -v "^COMMAND" 用于排除表头，只要有实质输出就说明有文件正在被读写
    if lsof +D "$SOURCE_DIR" 2>/dev/null | grep -q -v "^COMMAND"; then
        echo "[Wait] I/O operations detected. Waiting 2 seconds..."
        sleep 2
    else
        # 双重验证机制：防止复制多个文件时，两个文件复制间隙产生的毫秒级无锁状态导致误判
        sleep 1
        if ! lsof +D "$SOURCE_DIR" 2>/dev/null | grep -q -v "^COMMAND"; then
            echo "[Info] Directory I/O clear. Handing over to file_sorter."
            break
        fi
    fi
done

# 替换当前 shell 进程为 C++ 程序，节省资源
exec "$BINARY"
EOF

chmod +x "$WRAPPER_SCRIPT"

# 2. 生成 .service 文件（改为调用包装器）
cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=File Sorter Processing Service
After=network.target

[Service]
Type=oneshot
WorkingDirectory=$PROJECT_DIR
ExecStart=$WRAPPER_SCRIPT "$SOURCE_DIR" "$BINARY"
StandardOutput=journal
StandardError=journal
EOF

# 3. 生成 .path 文件
cat > "$PATH_FILE" <<EOF
[Unit]
Description=Watch File Sorter Input Directory

[Path]
PathModified=$SOURCE_DIR
Unit=file-sorter.service

[Install]
WantedBy=default.target
EOF

# 重新加载并启用服务
systemctl --user daemon-reload
systemctl --user enable file-sorter.path
systemctl --user restart file-sorter.path

echo "------------------------------------------------------"
echo "✅ 自动化监控已成功安装并启动！(基于 IO 锁防抖机制)"
echo "------------------------------------------------------"
echo "更新机制说明："
echo "- 现已加入 lsof 进程锁检测。无论是拖入 1 个文件还是 100 个大文件，"
echo "- 系统都会耐心等待所有系统级 IO 释放后，才执行分类逻辑，彻底杜绝文件损坏异常。"
echo "------------------------------------------------------"