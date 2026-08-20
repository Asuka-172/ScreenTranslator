cmake_minimum_required(VERSION 3.20)
project(ScreenTranslator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Network)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(tesseract REQUIRED)
find_package(leptonica REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

set(SOURCES
    src/main.cpp
    ScreenTranslator.cpp
    CaptureOverlay.cpp
    TranslationService.cpp
    CustomOnlineTranslator.cpp
    OfflineTranslator.cpp
    TranslationEngineManager.cpp
    DocxExporter.cpp
    AppConfig.cpp
    ThemeManager.cpp
    SettingsDialog.cpp
    TextToSpeech.cpp
    GlobalHotkey.cpp
    PluginManager.cpp
    PluginTranslator.cpp
)

set(HEADERS
    ScreenTranslator.h
    CaptureOverlay.h
    TranslationService.h
    CustomOnlineTranslator.h
    OfflineTranslator.h
    TranslationEngineManager.h
    ITranslator.h
    DocxExporter.h
    AppConfig.h
    ThemeManager.h
    SettingsDialog.h
    TextToSpeech.h
    GlobalHotkey.h
    PluginManager.h
    PluginTranslator.h
    IPlugin.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    ${OpenCV_LIBS}
    Tesseract::libtesseract
    leptonica
    CURL::libcurl
    nlohmann_json::nlohmann_json
    ole32
)

# 百度翻译示例插件（独立 .dll）
add_library(baidu_translate MODULE plugins/BaiduTranslatorPlugin.cpp)
target_include_directories(baidu_translate PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(baidu_translate PRIVATE
    Qt6::Core
    CURL::libcurl
)
set_target_properties(baidu_translate PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
