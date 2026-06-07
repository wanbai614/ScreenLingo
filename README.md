# ScreenLingo — Windows 屏幕实时翻译助手

低功耗、非侵入式的 Windows 屏幕 OCR 翻译工具，基于 Qt6 C++ 开发。

用户正常操作底层软件时完全不受干扰，译文以透明气泡覆盖在原文上方。

---

## 快速流程

```
┌──────────┐    ┌──────────┐    ┌──────────────┐    ┌──────────┐
│ 框选区域  │ →  │ OCR 识别  │ →  │ 分批合并翻译   │ →  │ 气泡覆盖  │
│ Ctrl+Shift+F│   │ 3引擎可选 │    │ Ollama/DeepSeek│    │ 可拖动    │
└──────────┘    └──────────┘    └──────────────┘    └──────────┘
```

**三步走：**
1. 托盘右键 → 设置 → 选翻译服务 + OCR 引擎
2. `Ctrl+Shift+F` 框选翻译区域 → 勾选 UI Mode 逐元素翻译
3. 译文气泡覆盖原文 → ✋ 按钮切换拖动模式微调位置

---

## 功能特性

### 三种翻译模式

| 模式 | 快捷键 | 说明 |
|------|--------|------|
| **框选-UI 模式** | `Ctrl+Shift+F` | OCR 识别区域内每个 UI 元素，逐条翻译，气泡直接覆盖原文 |
| **框选-段落模式** | `Ctrl+Shift+F`（关闭 UI 模式） | 将区域内所有文本拼接为段落，一次翻译，支持自动换行 |
| **划词翻译** | `Ctrl+Shift+G` | 在任意应用中鼠标拖选文字，翻译气泡显示在光标旁 |

### 翻译服务

支持多种翻译后端，可在设置中切换：

| 翻译器 | 类型 | 说明 |
|--------|------|------|
| **Ollama** | 本地 LLM | 支持任意模型，4 路并发请求，JSON 结构化输出 |
| **OpenAI** | 云端 LLM | 兼容 OpenAI API 格式 |
| **DeepSeek** | 云端 LLM | 兼容 DeepSeek API |
| **DeepL** | 云端 API | 专业翻译服务 |
| **Google 翻译** | 云端 API | 免费在线翻译 |
| **百度翻译** | 云端 API | 国内翻译服务 |
| **本地词典** | 离线 | 无需网络，适合简单词汇 |

### OCR 引擎

- **Windows OCR**（默认）— Windows 11 内置 OCR，零配置
- **PaddleOCR**（可选）— 基于 ONNX Runtime 的离线 OCR
- **GLM-OCR**（可选）— 基于智谱 GLM-4V 的云端 VLM 文档解析，支持本地 Ollama 部署

### 翻译官角色（提示词预设）

内置 8 种行业翻译官，支持自定义新增：

- 通用翻译 · 编程术语 · 文学翻译 · 影视字幕
- 学术论文 · 游戏本地化 · Unreal Engine 开发 · 商务文档

### 视觉自定义

- 文字/背景颜色（调色板选择）
- 背景透明度（0% ~ 100%）
- 边框宽度与颜色
- 暗色/亮色主题切换
- 气泡字体大小

### 其他特性

- **智能布局引擎** — 译文气泡自动避开已有内容，不超出屏幕边界
- **自愈修复层** — 翻译失败自动重试（每文本最多 3 次），带 JSON 结构化校验
- **全局控制** — 一键显示/隐藏所有译文，暂停/恢复翻译
- **多屏支持** — 跨显示器工作，Per-Monitor DPI 感知
- **点击穿透** — 译文窗口不拦截鼠标事件
- **系统托盘** — 最小化到托盘，后台静默运行

---

## 截图

> 替换为实际截图

- 系统托盘菜单 + 浮动工具栏：`docs/screenshots/toolbar.png`
- 框选-UI 模式翻译效果：`docs/screenshots/ui-mode.png`
- 框选-段落模式翻译效果：`docs/screenshots/paragraph-mode.png`
- 划词翻译效果：`docs/screenshots/selection-mode.png`
- 设置界面：`docs/screenshots/settings.png`

---

## 快速开始

### 系统要求

- Windows 10/11 x64
- Qt 6.5+（MSVC 2022 或 MinGW 64-bit）
- Visual Studio 2022（推荐）或 MinGW-w64

### 直接运行

从 [Releases](https://github.com/wanbai614/ScreenLingo/releases) 下载最新版本，解压后运行 `ScreenLingo.exe`。

**首次启动后**：
1. 在系统托盘中右键 ScreenLingo 图标 → 设置
2. 选择翻译服务（Ollama 需先启动本地服务：`ollama serve`）
3. 配置 OCR 引擎
4. 使用 `Ctrl+Shift+F` 框选翻译区域

### 编译

详见 [docs/DEPLOY.md](docs/DEPLOY.md)

```bash
# 安装 Qt 6.5+ (MSVC 2022 64-bit)
git clone https://github.com/wanbai614/ScreenLingo.git
cd ScreenLingo

# 一键下载依赖（ONNX Runtime + PaddleOCR 模型）
powershell -File setup.ps1

# CMake 构建
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=D:/Qt/6.9.1/msvc2022_64
cmake --build build --config Release
```

---

## 项目结构

```
ScreenLingo/
├── src/
│   ├── main.cpp                    # 入口
│   ├── app/                        # 应用层（Application, SignalBus）
│   ├── common/                     # 公共（Config, Types, LanguageManager）
│   ├── core/
│   │   ├── capture/                # 屏幕捕获（DXGI Desktop Duplication）
│   │   ├── ocr/                    # OCR 引擎（Windows/PaddleOCR）
│   │   ├── translate/              # 翻译服务（plugins/ 目录）
│   │   └── selection/              # 鼠标选中监听（WH_MOUSE_LL 钩子）
│   ├── engine/
│   │   ├── hotkey/                 # 全局快捷键（RegisterHotKey）
│   │   └── layout/                 # 防重叠布局引擎
│   └── ui/
│       ├── floating/               # 浮动工具栏
│       ├── overlay/                # 译文覆盖层气泡
│       ├── popup/                  # 划词翻译弹窗
│       ├── selector/               # 区域框选器
│       ├── settings/               # 设置面板 + 提示词管理
│       └── tray/                   # 系统托盘
├── resources/                      # 资源文件（翻译、manifest）
├── docs/                           # 文档
├── package/                        # 打包部署目录
└── CMakeLists.txt
```

---

## 快捷键

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+Shift+F` | 框选翻译区域 / 快照翻译 |
| `Ctrl+Shift+G` | 划词翻译开关 |
| `Ctrl+Shift+H` | 全局显示/隐藏译文 |
| `Ctrl+Shift+P` | 暂停/恢复翻译 |
| `Ctrl+Shift+S` | 打开设置 |

---

## 许可

MIT License

---

## 作者

WanBai
