cmake_minimum_required(VERSION 3.28)

project(ALTRun
    VERSION 2.0.4
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
    src/app/DesktopSpace.cpp
    src/app/Hotkey.cpp
    src/app/Settings.cpp
    src/app/ShellIntegration.cpp
    src/core/Encoding.cpp
    src/core/FontSpec.cpp
    src/core/Formatter.cpp
    src/core/Launcher.cpp
    src/core/Localization.cpp
    src/core/Logger.cpp
    src/core/Matcher.cpp
    src/core/ParamHistory.cpp
    src/core/Shortcut.cpp
    src/core/ShortcutSource.cpp
    src/core/ShortcutStore.cpp
    src/core/Text.cpp
    src/ui/AboutWindow.cpp
    src/ui/ConfigWindow.cpp
    src/ui/D2DContext.cpp
    src/ui/MainWindow.cpp
    src/ui/ParamWindow.cpp
    src/ui/ShortcutEditWindow.cpp
    src/ui/ShortcutManagerWindow.cpp
    src/ui/SkinImage.cpp
    src/ui/Window.cpp
    src/ui/WelcomeWindow.cpp
)

set(ALTRUN_HEADERS
    src/app/App.h
    src/app/DesktopSpace.h
    src/app/Hotkey.h
    src/app/Settings.h
    src/app/ShellIntegration.h
    src/core/Encoding.h
    src/core/FontSpec.h
    src/core/Formatter.h
    src/core/Launcher.h
    src/core/Localization.h
    src/core/LocalizationBuiltin.inc
    src/core/Logger.h
    src/core/Matcher.h
    src/core/ParamHistory.h
    src/core/Shortcut.h
    src/core/ShortcutSource.h
    src/core/ShortcutStore.h
    src/core/Text.h
    src/ui/AboutWindow.h
    src/ui/ConfigWindow.h
    src/ui/D2DContext.h
    src/ui/MainWindow.h
    src/ui/ParamWindow.h
    src/ui/ShortcutEditWindow.h
    src/ui/ShortcutManagerWindow.h
    src/ui/SkinImage.h
    src/ui/UiText.h
    src/ui/Window.h
    src/ui/WelcomeWindow.h
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
    windowscodecs   # WIC: skin background decode
    winmm           # PlaySoundW: Popup.wav notification
)

# Keep the executable next to runtime assets (skin, lang files, ini).
add_custom_command(TARGET ALTRun POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ALTRun>/res
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:ALTRun>/res
    COMMENT "Copying runtime resources next to ALTRun.exe")

# --- unit tests -------------------------------------------------------------
# UI-independent core logic only, so the tests run headless. Enable with
# -DALTRUN_BUILD_TESTS=OFF to skip, or run `ctest -C Release` after building.
option(ALTRUN_BUILD_TESTS "Build ALTRun core unit tests" ON)

if(ALTRUN_BUILD_TESTS)
    set(ALTRUN_CORE_SOURCES
        src/core/Encoding.cpp
        src/core/FontSpec.cpp
        src/core/Formatter.cpp
        src/core/Launcher.cpp
        src/core/Localization.cpp
        src/core/Logger.cpp
        src/core/Matcher.cpp
        src/core/ParamHistory.cpp
        src/core/Shortcut.cpp
        src/core/ShortcutSource.cpp
        src/core/ShortcutStore.cpp
        src/core/Text.cpp
    )

    add_executable(altRunTests tests/test_main.cpp ${ALTRUN_CORE_SOURCES})

    target_include_directories(altRunTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    target_compile_definitions(altRunTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    target_compile_options(altRunTests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4 /permissive- /EHsc>
    )

    target_link_libraries(altRunTests PRIVATE
        shell32
        shlwapi
        ole32
        user32
        gdi32
        advapi32
    )

    enable_testing()
    add_test(NAME altRunCore COMMAND altRunTests)
endif()
