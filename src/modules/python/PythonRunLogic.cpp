#include "modules/python/PythonRunLogic.h"

namespace superwin {

std::vector<std::wstring> DefaultInterpreterCandidates() {
    return {L"py.exe", L"python.exe", L"python3.exe"};
}

std::wstring QuoteArg(const std::wstring& arg) {
    // No quoting needed when there are no spaces/tabs/quotes.
    if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;

    std::wstring out = L"\"";
    for (size_t i = 0;; ++i) {
        unsigned backslashes = 0;
        while (i < arg.size() && arg[i] == L'\\') { ++backslashes; ++i; }
        if (i == arg.size()) {
            // Escape trailing backslashes so they don't escape the closing quote.
            out.append(backslashes * 2, L'\\');
            break;
        }
        if (arg[i] == L'"') {
            out.append(backslashes * 2 + 1, L'\\');  // escape the backslashes + the quote
            out += L'"';
        } else {
            out.append(backslashes, L'\\');
            out += arg[i];
        }
    }
    out += L'"';
    return out;
}

std::wstring BuildPythonCommandLine(const std::wstring& interpreter, const std::wstring& scriptPath) {
    return QuoteArg(interpreter) + L" -u " + QuoteArg(scriptPath);
}

}  // namespace superwin
