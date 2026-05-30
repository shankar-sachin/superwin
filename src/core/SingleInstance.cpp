#include "core/SingleInstance.h"

namespace superwin {
namespace {
constexpr wchar_t kMutexName[] = L"Local\\SuperWin.SingleInstance.Mutex";
constexpr wchar_t kMessageName[] = L"SuperWin.Activate.7F3A1C90";
}  // namespace

SingleInstance::SingleInstance() {
    handle_ = ::CreateMutexW(nullptr, TRUE, kMutexName);
    alreadyRunning_ = (handle_ != nullptr && ::GetLastError() == ERROR_ALREADY_EXISTS);
}

SingleInstance::~SingleInstance() {
    if (handle_) {
        ::ReleaseMutex(handle_);
        ::CloseHandle(handle_);
    }
}

UINT SingleInstance::ActivationMessage() {
    static const UINT msg = ::RegisterWindowMessageW(kMessageName);
    return msg;
}

void SingleInstance::NotifyExisting() {
    // HWND_BROADCAST reaches top-level windows; the primary instance filters
    // on ActivationMessage() and raises its main window.
    ::PostMessageW(HWND_BROADCAST, ActivationMessage(), 0, 0);
}

}  // namespace superwin
