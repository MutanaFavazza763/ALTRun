cmake_minimum_required(VERSION 3.28)

project(ALTRun
    VERSION 2.0.0
    DESCRIPTION "ALTRun - an alternative Run. Fast launcher for Windows."
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(ALTRUN_SOURCES
    src/main.cpp
    src/app/App.cpp
    src/app/Hotkey.cpp
    src/app/Settings.cpp
    src/core/Encoding.cpp
    src/core/Formatter.cpp
    src/core/Launcher.cpp
    src/core/Logger.cpp
    src/core/Matcher.cpp
    src/core/Shortcut.cpp
    src/core/ShortcutStore.cpp
    src/core/Text.cpp
    src/ui/D2DContext.cpp
    src/ui/MainWindow.cpp
    src/ui/Window.cpp
)

set(ALTRUN_HEADERS
    src/app/App.h
    src/app/Hotkey.h
    src/app/Settings.h
    src/core/Encoding.h
    src/core/Formatter.h
    src/core/Launcher.h
    src/core/Logger.h
    src/core/Matcher.h
    src/core/Shortcut.h
    src/core/ShortcutStore.h
    src/core/Text.h
    src/ui/D2DContext.h
    src/ui/MainWindow.h
    src/ui/Window.h
)

add_executable(ALTRun WIN32 ${ALTRUN_SOURCES} ${ALTRUN_HEADERS})

target_include_directories(ALTRun PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(ALTRun PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_compile_options(ALTRun PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4 /permissive- /EHsc>
)

target_link_libraries(ALTRun PRIVATE
    d2d1
    dwrite
    dwmapi
    shcore
    comctl32
    imm32
    shell32
    shlwapi
    user32
    gdi32
    ole32
    advapi32
)

# Keep the executable next to runtime assets (skin, lang files, ini).
add_custom_command(TARGET ALTRun POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ALTRun>/res
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:ALTRun>/res
    COMMENT "Copying runtime resources next to ALTRun.exe")
