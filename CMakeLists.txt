cmake_minimum_required(VERSION 3.27)
project(SuperWin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8 /W4 /permissive- /EHsc)
    # /MP: compile translation units across all cores. The WinUI sources each
    # pull in the large C++/WinRT + Windows App SDK headers, so multi-processor
    # compilation cuts a cold build from many minutes to a fraction of that.
    add_compile_options(/MP)
endif()
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

# ---------------------------------------------------------------------------
# Vendored header-only dependency (nlohmann/json). Keeps the core library and
# the test build free of any NuGet/vcpkg restore.
# ---------------------------------------------------------------------------
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

# ---------------------------------------------------------------------------
# superwin_core: pure logic + Win32 infrastructure (no WinUI, no NuGet).
# Both the app and the tests link this, so the testable logic compiles and
# runs independently of the Windows App SDK integration.
# ---------------------------------------------------------------------------
add_library(superwin_core STATIC
    src/core/Settings.cpp
    src/core/SingleInstance.cpp
    src/core/Hotkeys.cpp
    src/core/TrayIcon.cpp
    src/core/Autostart.cpp
    src/modules/volume/VolumeMath.cpp
    src/modules/clipboard/ClipStore.cpp
    src/modules/clipboard/ClipText.cpp
    src/modules/hash/HashLogic.cpp
    src/modules/netinfo/NetInfoLogic.cpp
    src/modules/convert/ConvertLogic.cpp
    src/modules/password/PasswordLogic.cpp
    src/modules/text/TextLogic.cpp
    src/modules/jsonfmt/JsonLogic.cpp
    src/modules/guid/GuidLogic.cpp
    src/modules/graph/GraphLogic.cpp
    src/modules/graph/Expr.cpp
    src/modules/security/SecurityLogic.cpp
)
target_include_directories(superwin_core PUBLIC src)
target_link_libraries(superwin_core PUBLIC
    nlohmann_json Shell32 Ole32 User32 bcrypt iphlpapi ws2_32)

# ---------------------------------------------------------------------------
# Unit tests (Catch2 via FetchContent -- not available on NuGet).
# ---------------------------------------------------------------------------
option(SUPERWIN_BUILD_TESTS "Build unit tests" ON)
if (SUPERWIN_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(SuperWin_tests
        tests/VolumeMathTests.cpp
        tests/ClipStoreTests.cpp
        tests/HashLogicTests.cpp
        tests/ConvertLogicTests.cpp
        tests/PasswordLogicTests.cpp
        tests/TextLogicTests.cpp
        tests/JsonLogicTests.cpp
        tests/GuidLogicTests.cpp
        tests/GraphLogicTests.cpp
        tests/ExprTests.cpp
        tests/SecurityLogicTests.cpp
    )
    target_link_libraries(SuperWin_tests PRIVATE superwin_core Catch2::Catch2WithMain)
    # Catch2's bundled entry point is wmain(); steer the CRT startup to match so
    # the linker doesn't default to looking for a (nonexistent) main().
    if (MSVC)
        target_link_options(SuperWin_tests PRIVATE /ENTRY:wmainCRTStartup)
    endif()

    enable_testing()
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(SuperWin_tests)
endif()

# ---------------------------------------------------------------------------
# SuperWin application.
#
# Stage 0b: a minimal Win32 tray host (main.cpp) that exercises the core
# infrastructure end-to-end. Stage 0c layers WinUI 3 on top by enabling
# SUPERWIN_ENABLE_WINUI, which pulls the Windows App SDK + C++/WinRT via NuGet
# PackageReference and adds the WinUI sources.
# ---------------------------------------------------------------------------
option(SUPERWIN_ENABLE_WINUI "Build the WinUI 3 dashboard (requires Windows App SDK via NuGet)" OFF)

set(SUPERWIN_APP_SOURCES src/main.cpp src/app.manifest)

add_executable(SuperWin WIN32 ${SUPERWIN_APP_SOURCES})
target_link_libraries(SuperWin PRIVATE superwin_core Comctl32)
set_target_properties(SuperWin PROPERTIES OUTPUT_NAME "SuperWin")

# ---- Version resource + built-in self-updater (WinHTTP; no third-party dep) ----
target_sources(SuperWin PRIVATE src/version.rc src/core/Updater.cpp)
target_link_libraries(SuperWin PRIVATE winhttp)
add_custom_command(TARGET SuperWin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/SuperWin.ico"
        "$<TARGET_FILE_DIR:SuperWin>/SuperWin.ico"
    COMMENT "Deploying app icon next to SuperWin.exe")

if (SUPERWIN_ENABLE_WINUI)
    target_compile_definitions(SuperWin PRIVATE SUPERWIN_ENABLE_WINUI=1)

    # WinUI 3 UI: shell + module pages (these pull in C++/WinRT headers).
    target_sources(SuperWin PRIVATE
        src/app/Shell.cpp
        src/app/HomePage.cpp
        src/app/SettingsPage.cpp
        src/app/AppHost.cpp
        src/modules/volume/VolumePage.cpp
        src/modules/volume/AudioSessions.cpp
        src/modules/clipboard/ClipPage.cpp
        src/modules/clipboard/ClipPicker.cpp
        src/modules/diagnostics/DiagPage.cpp
        src/modules/diagnostics/HardwareProbe.cpp
        src/modules/notepad/EditorPage.cpp
        src/modules/colorpicker/ColorPickerPage.cpp
        src/modules/keepawake/KeepAwakePage.cpp
        src/modules/hash/HashPage.cpp
        src/modules/netinfo/NetInfoPage.cpp
        src/modules/convert/ConvertPage.cpp
        src/modules/password/PasswordPage.cpp
        src/modules/text/TextPage.cpp
        src/modules/jsonfmt/JsonPage.cpp
        src/modules/guid/GuidPage.cpp
        src/modules/graph/GraphPage.cpp
        src/modules/security/SecurityPage.cpp
        src/modules/alwaystop/AlwaysOnTopPage.cpp
    )
    set_property(TARGET SuperWin PROPERTY VS_PACKAGE_REFERENCES
        "Microsoft.Windows.CppWinRT_2.0.250303.1"
        "Microsoft.WindowsAppSDK_1.8.260508005"
    )
    # Unpackaged (no MSIX) desktop app -> bootstrapper loads the runtime.
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_WindowsPackageType "None")
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_AppxPackage "false")
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_UseWinUI "true")
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_CppWinRTOptimized "true")
    # Self-contained: deploy the whole Windows App Runtime (DLLs + a correctly
    # merged resources.pri containing the WinUI theme resources) next to the exe.
    # This fixes XamlControlsResources/theme lookup for an unpackaged app and
    # removes the need to install the runtime redist separately.
    #
    # Self-contained is mutually exclusive with the bootstrapper: the runtime is
    # loaded from local DLLs, so we must NOT enable WindowsAppSdkBootstrapInitialize
    # (it would fail-fast before main trying to find a framework package). We keep
    # undocked reg-free WinRT so WinUI types activate from the local DLLs.
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_WindowsAppSDKSelfContained "true")
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_WindowsAppSdkUndockedRegFreeWinRTInitialize "true")
    # WinUI's MRT loads theme resources from "resources.pri" next to the exe.
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_ProjectPriFileName "resources.pri")
endif()
