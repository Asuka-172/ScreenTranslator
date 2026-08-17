# ScreenTranslator 屏幕翻译器

一个 Windows 桌面屏幕翻译工具：**框选屏幕区域 → OCR 识别文字 → 自动翻译**，结果实时显示在悬浮窗口的历史记录中。

## 功能特性

- 📸 **屏幕截图识别**：按 `F1` 进入截图模式，拖拽框选任意屏幕区域，自动识别其中的文字
- 🔍 **OCR 文字识别**：基于 Tesseract，支持中英文（`eng+chi_sim`），带 OpenCV 图像预处理增强识别率
- 🌐 **自动翻译**：调用 Google 翻译接口，支持自动检测源语言
- 📋 **历史记录**：悬浮窗口内展示翻译历史，带时间戳；右键菜单支持复制/清空
- 🎨 **悬浮窗口**：无边框、置顶、可拖拽、半透明，自动吸附屏幕右侧
- 🌏 **多语言**：按 `F2` 设置源语言/目标语言，支持中、英、日、韩、法、德、西等

## 使用说明

| 操作 | 快捷键/方式 |
|------|------------|
| 截图翻译 | `F1` |
| 语言设置 | `F2` |
| 取消截图 | `Esc` |
| 复制最新记录 / 全部记录 | 窗口内右键菜单 |
| 清空记录 / 退出 | 窗口内右键菜单 |

## 技术栈

- **Qt 6**（Widgets / Core / Gui / Network）— GUI 与网络请求
- **OpenCV** — 图像预处理（灰度、锐化、CLAHE、自适应阈值、形态学）
- **Tesseract + Leptonica** — OCR 文字识别
- **CURL / nlohmann_json** — 依赖库

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

将 Tesseract 训练数据（`eng.traineddata`、`chi_sim.traineddata` 等）放入可执行文件同目录下的 `tessdata/` 文件夹。

## 工作原理

1. 截图区域 → 转 `cv::Mat`
2. 预处理：暗底检测反转 → 2 倍放大 → 拉普拉斯锐化 → CLAHE → 中值滤波 → 自适应阈值 → 闭运算
3. 后台线程异步 OCR
4. 结果回主线程，调用 Google 翻译接口异步获取译文
5. 译文追加到历史记录

## 许可

仅供个人学习使用。
