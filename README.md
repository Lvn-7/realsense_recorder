# RealSense Recorder

一个轻量的 Ubuntu 原生录像软件。它通过 V4L2/OpenCV 读取 RealSense 的彩色相机，因此不依赖完整的 RealSense SDK。

## 功能

- 列出当前连接相机的型号、序列号、USB ID、连接速率和视频节点
- 自动选择 RealSense 彩色视频节点
- 实时画面预览
- 开始和结束 MP4 录像
- PNG 拍照
- 自由选择保存目录

## 支持环境

- Ubuntu 22.04 或更新版本
- Qt 5
- OpenCV 4（需要 FFmpeg 视频编码支持）
- USB 模式下的 RealSense 相机，例如 D415、D435、D455

## 安装依赖

```bash
cd /home/lvn/realsense_recorder
chmod +x scripts/install_dependencies.sh
./scripts/install_dependencies.sh
```

## 编译

```bash
cd /home/lvn/realsense_recorder
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

## 运行

```bash
cd /home/lvn/realsense_recorder
./build/realsense-recorder
```

默认保存目录为 `~/Videos/RealSense`。录像文件名类似：

```text
recording_20260924_113000_123.mp4
```

照片保存为 PNG：

```text
photo_20260924_113015_456.png
```

如果只想在终端查看相机信息：

```bash
./build/realsense-recorder --list
```

## 常见问题

如果相机能够被 `lsusb` 识别，但软件无法打开画面，请确认当前用户拥有视频设备权限：

```bash
sudo usermod -aG video "$USER"
```

重新登录后权限生效。还应关闭可能正在占用相机的 RealSense Viewer、浏览器或其他录像程序。
