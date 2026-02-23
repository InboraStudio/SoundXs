cmake_minimum_required(VERSION 3.25)
project(SoundXs VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

set(SOURCES
    src/main.cpp
    src/DeviceManager.cpp
    src/AudioEngine.cpp
    src/DSPEngine.cpp
    src/SettingsManager.cpp
    src/MainWindow.cpp
)

set(HEADERS
    include/DeviceManager.h
    include/AudioEngine.h
    include/DSPEngine.h
    include/SettingsManager.h
    include/MainWindow.h
)

add_executable(SoundXs WIN32
    ${SOURCES}
    ${HEADERS}
    resources/resources.qrc
    resources/app.rc
)

target_include_directories(SoundXs PRIVATE include)

target_link_libraries(SoundXs PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    ole32
    oleaut32
    ksuser
    avrt
    uuid
    winmm
)

target_compile_definitions(SoundXs PRIVATE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(SoundXs PRIVATE /O2 /W3 /MP /utf-8)
    target_link_options(SoundXs PRIVATE /SUBSYSTEM:WINDOWS)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(SoundXs PRIVATE -O2 -Wall -Wextra -Wpedantic)
    target_link_options(SoundXs PRIVATE -mwindows)
endif()

find_program(WINDEPLOYQT windeployqt6 windeployqt
    HINTS "${Qt6_DIR}/../../../bin"
)
if(WINDEPLOYQT)
    add_custom_command(TARGET SoundXs POST_BUILD
        COMMAND "${WINDEPLOYQT}" "$<TARGET_FILE:SoundXs>"
        COMMENT "Running windeployqt..."
    )
endif()
