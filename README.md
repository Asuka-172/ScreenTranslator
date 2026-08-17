# ScreenTranslator 屏幕翻译器

一个 Windows 桌面屏幕翻译工具：**框选屏幕区域 → OCR 识别文字 → 自动翻译**，结果实时显示在悬浮窗口的历史记录中。支持翻译朗读、全局热键、多主题、插件引擎等扩展能力。

## 功能特性

- **屏幕截图识别**：按 `F1` 进入截图模式，拖拽框选任意屏幕区域，自动识别其中的文字
- **OCR 文字识别**：基于 Tesseract，支持中英文（`eng+chi_sim`），带 OpenCV 图像预处理增强识别率
- **自动翻译**：调用 Google 翻译接口，支持自动检测源语言，可切换插件翻译引擎
- **翻译朗读（TTS）**：基于 Windows SAPI，翻译完成后自动朗读，语速/音量可调
- **全局热键**：系统级热键（窗口失焦也能触发），`F1`/`F2`/`F4` 可自定义，另支持「显示/隐藏窗口」「朗读最近一条」
- **主题系统**：深色 / 浅色 / 高对比度三套主题，字体、字号、窗口透明度可调
- **插件系统**：`IPlugin` 接口 + `QLibrary` 动态加载 `.dll`，内置百度翻译示例插件
- **历史记录与导出**：悬浮窗展示翻译历史，底部按钮导出为 `.txt` 或 `.docx`

## 使用说明

| 操作 | 快捷键/方式 |
|------|------------|
| 截图翻译 | `F1` |
| 语言设置 | `F2` |
| 打开设置 | `F4` |
| 取消截图 | `Esc` |
| 导出记录 | 窗口底部按钮（.txt / .docx） |
| 复制/清空/设置/退出 | 窗口内右键菜单 |

所有热键可在设置页「全局热键」中自定义；主题、字体、透明度、TTS 等在设置页调整后即时生效并持久化。

## 技术栈

- **Qt 6**（Widgets / Core / Gui / Network）— GUI 与网络请求
- **OpenCV** — 图像预处理（灰度、锐化、CLAHE、自适应阈值、形态学）
- **Tesseract + Leptonica** — OCR 文字识别
- **Windows SAPI**（COM）— 语音合成（TTS）
- **Windows RegisterHotKey** — 系统级全局热键
- **CURL / nlohmann_json** — 依赖库（插件 HTTP 请求等）

## 插件

插件以动态库形式放在可执行文件同目录（或 `plugins/` 子目录）下，启动时自动加载。

内置的百度翻译示例插件需要配置百度翻译开放平台的密钥，通过环境变量提供：

```bash
set BAIDU_APPID=你的appid
set BAIDU_SECRET=你的secret
```

未配置时会返回明确提示。配置后可在设置页「翻译与词典」中切换到百度翻译引擎。

## 构建

### 依赖

- Visual Studio 2022（MSVC，x64）
- CMake ≥ 3.20
- [vcpkg](https://github.com/microsoft/vcpkg) 及以下包：

```bash
vcpkg install opencv tesseract leptonica curl nlohmann-json --triplet x64-windows
```

- Qt 6（MSVC 2022 版本）

### 编译

项目已提供 `CMakePresets.json`，其中 `toolchainFile` 与 `CMAKE_PREFIX_PATH` 需按本机路径调整：

```bash
cmake --preset default
cmake --build out/build --config Release
```

### 运行前准备

- 将 Tesseract 训练数据（`eng.traineddata`、`chi_sim.traineddata` 等）放入可执行文件同目录下的 `tessdata/` 文件夹。
- TTS 朗读依赖系统语音包（Windows 通常自带中文语音 Huihui）。

## 工作原理

1. 截图区域 → 转 `cv::Mat`
2. 预处理：暗底检测反转 → 2 倍放大 → 拉普拉斯锐化 → CLAHE → 中值滤波 → 自适应阈值 → 闭运算
3. 后台线程异步 OCR
4. 结果回主线程，调用翻译引擎（Google 或插件）异步获取译文
5. 译文追加到历史记录，按配置触发 TTS 朗读

## 许可

仅供个人学习使用。
