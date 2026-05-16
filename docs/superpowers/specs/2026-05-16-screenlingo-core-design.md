# ScreenLingo 核心设计文档

**版本:** v1.0.0  
**日期:** 2026-05-16  
**状态:** 已定稿 — 待进入实现计划

---

## 1. 项目概述

### 1.1 项目定位
ScreenLingo 是一款低功耗、非侵入式的 Windows 屏幕实时翻译助手，基于 Qt6 C++ 构建。核心原则：用户正常操作底层软件时完全不受干扰。

### 1.2 目标用户
需要在应用程序、游戏、文档中阅读外语内容，但不想切换上下文或手动复制文字的用户。

### 1.3 第一阶段范围（v1.0）
构建核心闭环管线，验证技术路线可行性：

- 单区域框选
- 快照翻译（手动触发，单帧）
- 实时翻译（持续监测）
- 暂停翻译（译文冻结，资源趋近于零）

**v1.0 暂不包含：** 长按翻译、多区域管理、区域移动/缩放/删除。

---

## 2. 技术选型

| 决策项 | 选择 | 理由 |
|--------|------|------|
| UI 框架 | Qt 6.x（最新稳定版） | 原生 Windows 集成，现代 C++ 支持 |
| C++ 标准 | C++20 | 现代特性，与 Qt6 对齐 |
| 构建系统 | CMake | 行业标准，Qt6 原生支持 |
| 屏幕抓取 | DXGI Desktop Duplication API | Windows 平台最低功耗方案 |
| OCR 引擎 | Windows.Media.Ocr (WinRT) | 系统内置，零额外依赖，功耗最低 |
| 翻译后端 | 插件化架构 | 可扩展，支持在线翻译 API 和大模型翻译 |
| 首批在线翻译插件 | DeepL | 欧洲语言翻译质量公认最高 |
| 首批大模型翻译插件 | OpenAI (GPT-4o) | LLM 翻译质量一流 |
| 配置存储 | QSettings (ini) + Windows 凭据保管库 | UI 明文配置 + API Key 加密存储 |
| 目标平台 | Windows 10+ (x64) | 与目标操作系统一致 |

---

## 3. 整体架构

### 3.1 架构模式
**模块化插件架构。** 所有模块通过接口通信，核心壳管理生命周期，信号总线解耦跨模块通信。

```
  App Core（应用壳 + 生命周期 + SignalBus）
   ├── ICaptureEngine   ← DxgiCaptureEngine（DXGI 桌面复制）
   ├── IOCREngine       ← WindowsOcrEngine（WinRT 系统 OCR）
   ├── ITranslator      ← DeepLTranslator / OpenAITranslator / ...
   ├── IOverlay         ← TranslationOverlay（由 OverlayManager 管理）
   └── ILayoutEngine    ← SmartLayoutEngine（三级避让布局）
```

### 3.2 设计原则
- **Fail Fast：** 致命错误初始化时即弹框退出；非致命错误优雅降级。
- **接口隔离：** 每个模块通过纯虚基类暴露最小接口。
- **信号/槽解耦：** 模块发信号，SignalBus 路由到订阅者。
- **禁止硬编码：** API 地址、密钥、默认值全部从配置读取。
- **YAGNI：** 只做 v1.0 需要的东西。外部 DLL 插件加载延后。

---

## 4. 目录结构

```
ScreenLingo/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── Application.h/cpp          # QApplication 子类，生命周期管理
│   │   └── SignalBus.h/cpp            # 跨模块信号总线
│   │
│   ├── core/
│   │   ├── capture/
│   │   │   ├── ICaptureEngine.h       # 抓屏接口
│   │   │   └── DxgiCaptureEngine.h/cpp
│   │   ├── ocr/
│   │   │   ├── IOCREngine.h           # OCR 接口
│   │   │   └── WindowsOcrEngine.h/cpp
│   │   └── translate/
│   │       ├── ITranslator.h          # 翻译插件接口
│   │       ├── TranslatorConfigField.h # 插件配置字段描述
│   │       ├── TranslatorManager.h/cpp
│   │       └── plugins/
│   │           ├── DeepLTranslator.h/cpp    # 在线翻译首实现
│   │           └── OpenAITranslator.h/cpp   # 大模型翻译首实现
│   │
│   ├── ui/
│   │   ├── overlay/
│   │   │   ├── TranslationOverlay.h/cpp    # 单个译文气泡窗口
│   │   │   └── OverlayManager.h/cpp        # 译文窗口管理器
│   │   ├── selector/
│   │   │   └── AreaSelector.h/cpp          # 区域框选蒙层
│   │   ├── settings/
│   │   │   └── SettingsPanel.h/cpp         # 设置面板
│   │   └── tray/
│   │       └── TrayManager.h/cpp           # 系统托盘管理
│   │
│   ├── engine/
│   │   ├── layout/
│   │   │   └── LayoutEngine.h/cpp          # 三级避让布局引擎
│   │   └── hotkey/
│   │       └── HotkeyManager.h/cpp         # 全局快捷键管理
│   │
│   └── common/
│       ├── Types.h                         # 共享数据结构
│       └── Config.h/cpp                    # 配置持久化
│
├── resources/
│   └── app.qrc
└── tests/
```

依赖方向：`common → core → engine → ui → app`

---

## 5. 核心数据流

```
[屏幕] ──DXGI抓帧──▶ [DxgiCaptureEngine]
                            │
                  原始帧数据（D3D11Texture2D）
                            │
                  ┌─────────▼────────┐
                  │  区域裁剪           │  只处理用户选中区域
                  │  + 画面变化检测      │  画面无变化则跳过后续全部
                  └─────────┬────────┘
                            │
                   裁剪后位图（QImage）
                            │
                  ┌─────────▼────────┐
                  │  WindowsOcrEngine │  调用 Windows.Media.Ocr
                  │  （异步执行）       │  返回文字 + 边界框列表
                  └─────────┬────────┘
                            │
                   OCRResult { text, boundingBoxes[] }
                            │
                  ┌─────────▼────────┐
                  │  TranslatorManager│  根据当前选择的服务
                  │                   │  路由到 DeepL / OpenAI 等
                  └─────────┬────────┘
                            │
                  TranslateResult { original, translated, bbox }
                            │
                  ┌─────────▼────────┐
                  │  LayoutEngine     │  三级避让，计算最佳位置
                  │                   │  1. 不遮挡原文
                  │                   │  2. 不覆盖已有译文
                  │                   │  3. 不超出屏幕边界
                  └─────────┬────────┘
                            │
                  LayoutResult { position, size, fontSize }
                            │
                  ┌─────────▼────────┐
                  │  OverlayManager   │  创建/更新/销毁叠加窗口
                  │                   │  WS_EX_TRANSPARENT
                  │                   │  + WA_TransparentForMouseEvents
                  └──────────────────┘
```

**关键设计决策：**
- **变化检测**放在 OCR 之前（GPU 端帧差异比对），画面静止时跳过所有后续处理
- **OCR 异步执行**，绝不阻塞主线程
- 一帧中识别出多段文字时**并行翻译**，全部完成后统一布局

---

## 6. 模块详细设计

### 6.1 DXGI 屏幕抓取引擎

```cpp
class ICaptureEngine : public QObject {
    Q_OBJECT
public:
    virtual bool initialize() = 0;
    virtual QImage captureRegion(const QRect& region, int screenIndex) = 0;
    virtual bool hasChanged(const QRect& region) = 0;  // GPU 帧差异
    virtual void shutdown() = 0;

signals:
    void captureError(const QString& message);
};
```

**实现要点：**
- 每显示器获取 `IDXGIOutputDuplication` 实例
- `hasChanged()` 通过 `LastPresentTime` / `AccumulatedFrames` 判断是否有新帧，避免完整拷贝
- `captureRegion()` 映射桌面纹理，只拷贝感兴趣区域
- 监听 `WM_DISPLAYCHANGE` 处理分辨率变化和显示器热插拔

### 6.2 Windows OCR 引擎

```cpp
class IOCREngine : public QObject {
    Q_OBJECT
public:
    virtual bool initialize(const QString& languageTag = L"auto") = 0;
    virtual void recognize(const QImage& image) = 0;

signals:
    void recognitionComplete(const OCRResult& result);
    void recognitionError(const QString& error);
};

struct OCRResult {
    QString     text;
    QVector<TextBox> boxes;
};

struct TextBox {
    QString     text;
    QRect       boundingRect;  // 相对于抓取区域
};
```

**实现要点：**
- 通过 WinRT 调用 `Windows.Media.Ocr.OcrEngine`
- 语言自动检测通过 `OcrEngine::AvailableRecognizerLanguages`
- 在后台线程执行，结果通过信号投递

### 6.3 翻译插件接口（含配置能力）

```cpp
struct TranslatorConfigField {
    QString key;            // "apiKey", "endpoint", "model"
    QString label;          // 设置界面显示名
    QString defaultValue;
    bool    isSecret;       // true → 界面显示密码输入框
    bool    isRequired;
};

class ITranslator : public QObject {
    Q_OBJECT
public:
    virtual QString name() const = 0;        // "DeepL", "OpenAI"
    virtual QString category() const = 0;    // "online" | "llm"
    virtual bool isAvailable() const = 0;    // API Key 已配置？

    virtual void translate(const TranslateRequest& req) = 0;
    virtual void cancelAll() = 0;

    // 配置接口
    virtual QVector<TranslatorConfigField> configFields() const = 0;
    virtual void setConfig(const QString& key, const QString& value) = 0;
    virtual QString getConfig(const QString& key) const = 0;

signals:
    void translationReady(const QString& original, const QString& translated);
    void translationError(const QString& error);
};

struct TranslateRequest {
    QString text;
    QString sourceLang;  // "auto" 自动检测
    QString targetLang;
};
```

**TranslatorManager 职责：**
- 持有所有翻译插件实例，维护当前激活的插件指针
- `registerPlugin()`, `setActive(name)`, `translate(text)` 路由到当前插件
- 转发 `translationReady` / `translationError` 信号

**DeepLTranslator（在线翻译类）：**
- 异步 HTTP 通过 `QNetworkAccessManager`
- 接口地址：`https://api-free.deepl.com/v2/translate`（可配置）
- API Key 从配置读取（存储于 Windows 凭据保管库）

**OpenAITranslator（大模型翻译类）：**
- 异步 HTTP 请求 OpenAI 兼容接口
- 可配置 model、base URL、system prompt
- 默认 system prompt: "You are a professional translator. Translate the following text naturally while preserving tone and meaning. Output only the translation, nothing else."

### 6.4 三级防重叠布局引擎

```cpp
struct LayoutRequest {
    QRect   sourceRect;        // 原文在屏幕上的位置
    QString translatedText;
    int     preferredFontSize;
};

struct LayoutResult {
    QPoint  position;
    int     fontSize;          // 可能缩小的字号
    int     maxWidth;
    bool    isTruncated;       // 是否被截断
};

class LayoutEngine {
public:
    LayoutResult compute(const LayoutRequest& request,
                         const QVector<QRect>& existingBubbles,
                         const QRect& screenBounds);

private:
    // 候选位置按优先级排序：[右侧, 下方, 左侧, 上方]
    QVector<QPoint> generateCandidates(const LayoutRequest& req);
    
    bool overlapsSource(const QRect& candidate, const QRect& source);
    bool overlapsExisting(const QRect& candidate, const QVector<QRect>& existing);
    bool exceedsBounds(const QRect& candidate, const QRect& screen);
    QRect clampToBounds(const QRect& candidate, const QRect& screen);
};
```

**算法流程：**
1. 按优先级生成候选位置：右侧 → 下方 → 左侧 → 上方
2. 逐候选检查：一级（不遮挡原文）→ 二级（不覆盖已有译文气泡）→ 三级（不超出屏幕边界，微幅越界则裁剪）
3. 所有候选均失败 → 缩小字号（最小至 8pt）后重试
4. 字号已最小仍放不下 → 截断文字加 "..."，强制放置于优先级最高的可行位置

核心算法约 200 行，纯几何计算，无外部依赖。

### 6.5 译文叠加窗口

```cpp
class TranslationOverlay : public QWidget {
    // 构造函数中设置的窗口属性：
    // - Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint
    // - Qt::WA_TransparentForMouseEvents
    // - Qt::WA_TranslucentBackground
    // - 同时通过 HWND 设置 WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE

public:
    void showTranslation(const LayoutResult& layout, const QString& text,
                         const StyleConfig& style);
    void hideTranslation();
    void updateStyle(const StyleConfig& style);  // 支持实时预览

protected:
    void paintEvent(QPaintEvent*) override;
};
```

**三层窗口属性确保完全穿透：**
```
用户点击位置
    │
    ▼
 ┌────────────┐
 │  Overlay   │  ← WS_EX_TRANSPARENT + WA_TransparentForMouseEvents
 │  事件不处理  │     点击直接穿透，无任何中间拦截
 └─────┬──────┘
       │ 穿透
       ▼
 ┌────────────┐
 │ 下方应用程序 │  ← 用户点击实际落入此处
 └────────────┘
```

**渲染流程：**
1. 绘制圆角矩形背景（用户设定的颜色 + alpha 通道控制透明度）
2. 绘制边框（用户设定的颜色 + 宽度）
3. 绘制译文文字（用户设定颜色、字号、字体）

### 6.6 OverlayManager

```cpp
class OverlayManager : public QObject {
    Q_OBJECT
public:
    void showTranslation(int id, const LayoutResult& layout,
                         const QString& text);
    void updateTranslation(int id, const QString& newText);
    void removeTranslation(int id);
    void removeAll();           // 全局隐藏
    void showAll();             // 全局恢复显示
    void updateAllStyles(const StyleConfig& style);  // 设置面板实时预览
};
```

### 6.7 区域选择器

```cpp
class AreaSelector : public QWidget {
    // 每显示器一个实例，全屏半透明蒙层
    // 50% 黑色蒙层 + 选区挖空显示
    // 8 个缩放控点（四角 + 四边中点）
    
signals:
    void areaConfirmed(const QRect& area, int screenIndex);
    void cancelled();
};
```

**交互行为：** 鼠标拖拽生成选区 → 拖拽控点缩放 → 拖拽中心移动选区 → Enter 确认 / Esc 取消。

### 6.8 全局快捷键管理器

```cpp
struct HotkeyBinding {
    QString id;
    QString label;          // 设置界面显示名
    QString defaultKeys;    // 默认快捷键，如 "Ctrl+Shift+T"
    QString currentKeys;    // 用户可覆盖
};

class HotkeyManager : public QObject {
    Q_OBJECT
public:
    bool registerBinding(const HotkeyBinding& binding);
    void unregisterAll();
    void updateBinding(const QString& id, const QString& newKeys);
    HotkeyBinding binding(const QString& id) const;
    bool hasConflict(const QString& keys, const QString& excludeId) const;

signals:
    void hotkeyTriggered(const QString& id);
};
```

**实现方式：** Win32 `RegisterHotKey` API + `QAbstractNativeEventFilter` 拦截 `WM_HOTKEY`。默认绑定存储于 `QVector<HotkeyBinding>`，持久化到 `QSettings`。

**默认快捷键绑定：**

| ID | 功能 | 默认快捷键 |
|----|------|-----------|
| mode_toggle | 模式切换 | Ctrl+Shift+T |
| area_select | 选择翻译区域 | Ctrl+Shift+A |
| global_hide | 显示/隐藏全部译文 | Ctrl+Shift+H |
| long_press | 长按翻译 | Ctrl+Shift+G |
| settings | 打开设置 | Ctrl+Shift+S |
| snapshot_once | 单次快照翻译 | Ctrl+Shift+Q |

### 6.9 系统托盘管理器

```cpp
class TrayManager : public QObject {
    Q_OBJECT
public:
    void initialize();
    void updateIcon(Mode mode);  // 正常图标 / 暂停图标（带黄色标记）

private:
    QSystemTrayIcon* trayIcon;
    QMenu* trayMenu;
    void buildMenu();
};
```

**托盘菜单结构：**
```
模式切换
  ├─ 实时翻译    ●（当前模式）
  ├─ 快照翻译
  └─ 暂停翻译    （暂停时图标变黄）
───────────────────
选择翻译区域...
显示/隐藏全部译文
───────────────────
设置...
───────────────────
退出
```

### 6.10 设置面板

分页对话框，带**实时预览**功能：

| 标签页 | 内容 |
|--------|------|
| 外观 | 文字颜色/字号/字体、背景颜色/透明度/圆角半径、边框颜色/宽度。面板内预览气泡即时刷新，同时屏幕上的已有译文实时更新（含暂停冻结的译文）。 |
| 翻译服务 | 服务选择下拉框、各插件配置字段（API Key 密码框、接口地址、模型选择、LLM 的 System Prompt）。 |
| 快捷键 | 全部快捷键绑定列表，点击修改弹出按键捕获对话框，冲突检测提示，一键恢复默认。 |
| 翻译区域 | 已选区域缩略图列表，支持启用/禁用、删除、新增。 |

**配置存储方式：**
- UI 外观设置 → `QSettings`（ini 文件，明文）
- API Keys → Windows 凭据保管库（加密存储）
- 快捷键绑定 → `QSettings`（ini 文件）

### 6.11 SignalBus 信号总线

```cpp
class SignalBus : public QObject {
    Q_OBJECT
    // 中央信号路由，避免模块间直接耦合

signals:
    // 抓帧 → OCR
    void frameCaptured(const QImage& frame, const QRect& region);

    // OCR → 翻译
    void textRecognized(const OCRResult& result);

    // 翻译 → 布局 → 叠加层
    void translationReady(const QString& original, const QString& translated,
                          const QRect& sourceRect);

    // 模式切换
    void modeChanged(Mode newMode);

    // 样式变更（设置面板 → 全部叠加窗口）
    void styleChanged(const StyleConfig& style);

    // 全局显示/隐藏
    void globalVisibilityChanged(bool visible);
};
```

---

## 7. 工作模式状态机

```
                    ┌─────────────┐
     快捷键/托盘 ───▶│  暂停翻译    │◀─── 快捷键/托盘
                    │（译文冻结）   │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
     快捷键/托盘 ───▶│  快照翻译    │  单帧 OCR+翻译 → 保持显示至下次触发
                    │（静止待机）   │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │  实时翻译    │  循环：抓帧→变化检测→OCR→翻译→更新气泡
                    │（持续监测）   │
                    └─────────────┘
```

| 模式 | 抓帧定时器 | OCR/翻译 | 译文气泡 |
|------|:---:|:---:|:---:|
| 实时翻译 | 100ms 间隔 | 有变化才执行 | 随文字动态增删 |
| 快照翻译 | 关闭 | 手动触发单次 | 保持显示 |
| 暂停翻译 | 关闭 | 停止 | 冻结但样式更新仍然生效 |
| 长按翻译 | 关闭（并行） | 按键期间运行 | 松手瞬间销毁 |

**长按翻译独立于状态机**——与当前模式并行运行，不改变模式状态。全局显示/隐藏优先级最高，独立于所有模式。

---

## 8. 错误处理策略

### 8.1 致命错误（弹框 + 退出）
- `DxgiCaptureEngine` 初始化失败（无 GPU / DXGI 不可用）
- `WindowsOcrEngine` 不可用（非 Win10 / 缺少语言包）
- `HotkeyManager` 关键快捷键注册失败（提示以管理员权限运行）

### 8.2 非致命错误（优雅降级）
- 翻译 API Key 未配置 → 托盘图标变灰 + 提示配置
- 翻译 API 网络错误 → 显示"翻译失败"气泡，3 秒后渐隐
- OCR 识别为空 → 静默跳过，不显示任何内容

### 8.3 运行时异常
- DXGI 帧丢失（桌面锁定、UAC 弹窗） → 下一帧重试，不崩溃
- 内存压力 → 拒绝创建新 Overlay，保留已有译文

---

## 9. 非功能需求指标

| 指标 | 目标 | 实现机制 |
|------|------|----------|
| 静止功耗 | CPU 趋近于零 | OCR 前画面变化检测 |
| 暂停功耗 | 后台处理完全停止 | 所有定时器关闭，线程空闲 |
| 长按响应延迟 | < 200ms（按下到显示） | 单帧 OCR，不排队 |
| 内存占用 | < 100MB 常驻 | 缩略图缓存有界，Overlay 延迟创建 |
| DPI 适配 | Per-Monitor DPI v2 | Qt::AA_UseHighDpiPixmaps + manifest 声明 |
| 多屏支持 | 识别鼠标所在屏幕 | 每显示器独立 Overlay |
| 点击穿透 | 完全无拦截 | WS_EX_TRANSPARENT + WA_TransparentForMouseEvents |

---

## 10. 第一阶段交付物清单（v1.0）

1. **DxgiCaptureEngine** — 帧抓取 + 变化检测
2. **WindowsOcrEngine** — 单区域文字识别
3. **DeepLTranslator + OpenAITranslator** — 两个翻译后端
4. **TranslatorManager** — 插件注册 + 路由
5. **LayoutEngine** — 单气泡三级避让
6. **TranslationOverlay + OverlayManager** — 可穿透叠加窗口
7. **AreaSelector** — 单区域拖拽框选
8. **HotkeyManager** — 6 个默认绑定，支持自定义
9. **TrayManager** — 系统托盘 + 模式菜单
10. **SettingsPanel** — 外观/翻译/快捷键/区域 四个标签页 + 实时预览
11. **SignalBus** — 模块解耦
12. **Application** — 生命周期、错误处理、配置持久化

### 10.1 明确延后到后续版本
- 长按翻译模式
- 多区域管理（移动、缩放、删除多个区域）
- 外部 DLL 插件加载（当前插件编译进主程序）
- Tesseract OCR 备选引擎
- 源语言自动检测（当前手动指定）
