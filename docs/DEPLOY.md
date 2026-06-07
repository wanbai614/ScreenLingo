# ScreenLingo 部署文档

## 环境依赖

### 必需

| 组件 | 版本要求 | 说明 |
|------|----------|------|
| **Windows** | 10/11 x64 | 最低 Windows 10 1903 |
| **Qt** | 6.5+ | Core / Gui / Widgets / Network 模块 |
| **CMake** | 3.22+ | 构建系统 |
| **编译器** | MSVC 2022 或 MinGW-w64 8.1+ | MSVC 推荐（完整 OCR 支持） |
| **DirectX** | 11 | DXGI Desktop Duplication API |

### 可选

| 组件 | 用途 |
|------|------|
| **ONNX Runtime** | PaddleOCR 本地 OCR 引擎（需放置在 `lib/onnxruntime/`） |
| **Python 3.8+** | PaddleOCR 子进程引擎（需 `paddleocr` 和 `paddlepaddle`） |
| **Ollama** | 本地 LLM 翻译（需运行 `ollama serve`） |

---

## 编译

### Windows (MSVC 2022) — 推荐

```bash
# 1. 安装 Qt 6.5+ MSVC 2022 64-bit
# 下载: https://www.qt.io/download-qt-installer
# 安装时勾选: Qt 6.x → MSVC 2022 64-bit

# 2. 安装 Visual Studio 2022 Community
# 下载: https://visualstudio.microsoft.com/downloads/
# 安装时勾选: "使用 C++ 的桌面开发"

# 3. 克隆项目
git clone https://github.com/niceai2008/ScreenLingo.git
cd ScreenLingo

# 4. CMake 配置 & 编译
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_PREFIX_PATH=D:/Qt/6.9.1/msvc2022_64

cmake --build build --config Release

# 输出: build/Release/ScreenLingo.exe
```

### Windows (MinGW) — 开发/测试用

> **注意**: MinGW 编译时 Windows OCR 为 Stub（不可用），请使用 PaddleOCR 子进程引擎。

```bash
# 安装 Qt 6.5+ MinGW 64-bit
# 确保 g++ 和 mingw32-make 在 PATH 中

cmake -B build_mingw -G "MinGW Makefiles" ^
    -DCMAKE_PREFIX_PATH=D:/Qt/6.9.1/mingw_64

cmake --build build_mingw --config Release
```

### PaddleOCR ONNX 引擎（可选）

```bash
# 1. 下载 ONNX Runtime Windows x64
# https://github.com/microsoft/onnxruntime/releases

# 2. 解压到项目 lib 目录
mkdir -p lib/onnxruntime
# 复制 include/ 和 lib/ 到 lib/onnxruntime/

# 3. 重新配置 CMake（自动检测）
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_PREFIX_PATH=D:/Qt/6.9.1/msvc2022_64
```

---

## 打包部署

### 自动打包

项目 `package/` 目录已包含部署就绪的文件结构：

```
package/
├── ScreenLingo.exe          # 编译产出的 exe
├── Qt6Core.dll              # Qt 运行时 DLL
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── Qt6Network.dll
├── Qt6Svg.dll
├── D3Dcompiler_47.dll
├── onnxruntime.dll          # 可选：PaddleOCR ONNX
├── onnxruntime_providers_shared.dll
├── opengl32sw.dll
├── msvcp140.dll             # VC++ 运行时
├── vcruntime140.dll
├── vcruntime140_1.dll
├── concrt140.dll
├── platforms/               # Qt 平台插件
│   └── qwindows.dll
├── styles/                  # Qt 样式插件
│   └── qwindowsvistastyle.dll
├── imageformats/            # Qt 图片格式插件
├── iconengines/
├── networkinformation/
├── generic/
├── tls/
├── translations/            # Qt 翻译文件
│   └── screenlingo_zh_CN.qm
├── models/                  # OCR 模型文件（可选）
│   └── RapidOCR/
└── paddleocr_server.py      # PaddleOCR 子进程脚本（可选）
```

### 使用 windeployqt 自动收集 Qt DLL

```bash
# 1. 编译 Release 版本
cmake --build build --config Release

# 2. 复制 exe 到 package
cp build/Release/ScreenLingo.exe package/

# 3. 运行 Qt 部署工具
cd package
D:/Qt/6.9.1/msvc2022_64/bin/windeployqt.exe ScreenLingo.exe

# 4. 复制 VC++ 运行时（如果系统没有安装）
# 从 VS 2022 安装目录复制:
#   msvcp140.dll, vcruntime140.dll, vcruntime140_1.dll, concrt140.dll
```

### 创建发布包

```bash
# 打包为 zip
cd package
7z a ../ScreenLingo-v1.0.0-win64.zip *

# 或使用 PowerShell
Compress-Archive -Path * -DestinationPath ../ScreenLingo-v1.0.0-win64.zip
```

---

## 运行环境检查

发布包在目标机器上运行时需要：

1. **Windows 10 1903+** 或 Windows 11
2. **Visual C++ 可再发行组件包**（`package/` 目录已自带 DLL）
3. **Ollama**（如使用 Ollama 翻译）：从 https://ollama.com 下载安装
4. **Python 3.8+**（如使用 PaddleOCR 子进程）：需安装 `paddleocr` 和 `paddlepaddle`

目标机器无需安装 Qt。

---

## 配置文件位置

程序运行时配置存储在：

```
%APPDATA%/ScreenLingo/ScreenLingo.ini
```

包含：
- 翻译服务选择及 API 密钥
- OCR 引擎选择
- 源语言/目标语言
- 主题（暗色/亮色）
- 气泡样式配置
- 提示词预设

调试日志位置：

```
%TEMP%/screenlingo_debug.log
```
