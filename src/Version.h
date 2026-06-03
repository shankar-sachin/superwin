// Single source of truth for the SuperWin version. Bump these when shipping a
// release; the installer (SuperWin.iss) and the appcast must match so the
// auto-updater can tell when a newer build is available.
#pragma once

#define SUPERWIN_VERSION_MAJOR 2
#define SUPERWIN_VERSION_MINOR 1
#define SUPERWIN_VERSION_PATCH 1

// Comma form for VERSIONINFO (FILEVERSION / PRODUCTVERSION).
#define SUPERWIN_VERSION_RC 2, 1, 1, 0

// String form for display / WinSparkle / the installer.
#define SUPERWIN_VERSION_STRING "2.1.1"
#define SUPERWIN_VERSION_WSTRING L"2.1.1"
