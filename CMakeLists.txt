cmake_minimum_required(VERSION 3.27)
project(SuperWin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8 /W4 /permissive- /EHsc)
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
)
target_include_directories(superwin_core PUBLIC src)
target_link_libraries(superwin_core PUBLIC nlohmann_json Shell32 Ole32 User32)

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

set(SUPERWIN_APP_SOURCES src/main.cpp)

add_executable(SuperWin WIN32 ${SUPERWIN_APP_SOURCES})
target_link_libraries(SuperWin PRIVATE superwin_core Comctl32)
set_target_properties(SuperWin PROPERTIES OUTPUT_NAME "SuperWin")

if (SUPERWIN_ENABLE_WINUI)
    target_compile_definitions(SuperWin PRIVATE SUPERWIN_ENABLE_WINUI=1)
    set_property(TARGET SuperWin PROPERTY VS_PACKAGE_REFERENCES
        "Microsoft.Windows.CppWinRT_2.0.250303.1"
        "Microsoft.WindowsAppSDK_1.8.260508005"
    )
    # Unpackaged (no MSIX) desktop app -> bootstrapper loads the runtime.
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_WindowsPackageType "None")
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_AppxPackage "false")
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_UseWinUI "true")
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_CppWinRTOptimized "true")
    # Auto-initializers from the WindowsAppSDK Foundation package: bootstrap the
    # runtime and enable undocked reg-free WinRT activation before main(), so an
    # unpackaged app can construct WinUI types without registration/MSIX.
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_WindowsAppSdkBootstrapInitialize "true")
    set_property(TARGET SuperWin PROPERTY VS_GLOBAL_WindowsAppSdkUndockedRegFreeWinRTInitialize "true")

    # The auto-initializer calls MddBootstrapInitialize2/Shutdown from the
    # Foundation package's Bootstrap.lib. The package props add it to
    # AdditionalDependencies, but CMake's link settings win the import-order
    # race, so link it explicitly. Glob the NuGet cache for version-robustness.
    set(_nuget_root "$ENV{USERPROFILE}/.nuget/packages")
    if (DEFINED ENV{NUGET_PACKAGES})
        set(_nuget_root "$ENV{NUGET_PACKAGES}")
    endif()
    file(GLOB _wasdk_bootstrap_libs
        "${_nuget_root}/microsoft.windowsappsdk.foundation/*/lib/native/x64/Microsoft.WindowsAppRuntime.Bootstrap.lib")
    if (_wasdk_bootstrap_libs)
        list(GET _wasdk_bootstrap_libs -1 _wasdk_bootstrap_lib)
        target_link_libraries(SuperWin PRIVATE "${_wasdk_bootstrap_lib}")
        message(STATUS "Linking WindowsAppSDK bootstrap: ${_wasdk_bootstrap_lib}")
    else()
        message(WARNING "WindowsAppRuntime.Bootstrap.lib not found under ${_nuget_root}; run a NuGet restore first.")
    endif()
endif()
