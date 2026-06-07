// Pure helpers for the Python IDE's "Run" feature: which interpreter names to
// look for, and how to build a correctly-quoted command line. Kept free of any
// process spawning / filesystem access so it is unit-testable in superwin_core;
// the actual SearchPathW lookup and CreateProcessW spawning live in PythonPage.
#pragma once

#include <string>
#include <vector>

namespace superwin {

// Interpreter executables to probe on PATH, in preference order. `py.exe` (the
// Windows launcher) first, then python/python3.
std::vector<std::wstring> DefaultInterpreterCandidates();

// Quote a single command-line argument per the Windows CommandLineToArgvW rules
// (wrap in quotes when it contains spaces/quotes; escape embedded quotes and the
// backslashes that precede them). Used so paths with spaces run correctly.
std::wstring QuoteArg(const std::wstring& arg);

// Build the full command line to run `scriptPath` with `interpreter`, passing
// -u so stdout/stderr are unbuffered and stream live to the output pane.
std::wstring BuildPythonCommandLine(const std::wstring& interpreter, const std::wstring& scriptPath);

}  // namespace superwin
