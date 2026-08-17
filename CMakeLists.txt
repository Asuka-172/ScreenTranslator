cmake_minimum_required(VERSION 3.20)
project(ScreenTranslator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Network)
find_package(tesseract REQUIRED)
find_package(leptonica REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

set(SOURCES
    src/main.cpp
    ScreenTranslator.cpp
    CaptureOverlay.cpp
    TranslationService.cpp
    DocxExporter.cpp
)

set(HEADERS
    ScreenTranslator.h
    CaptureOverlay.h
    TranslationService.h
    DocxExporter.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} "CaptureOverlay.h" "TranslationService.h" "TranslationService.cpp")

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
)