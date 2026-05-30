// Ensures only one SuperWin process runs at a time. A second launch detects
// the first via a named mutex, asks it to surface its window (through a
// broadcast registered-window-message), and then exits.
#pragma once

#include <Windows.h>

namespace superwin {

class SingleInstance {
public:
    SingleInstance();
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    // True if another SuperWin instance already owns the mutex.
    bool AlreadyRunning() const { return alreadyRunning_; }

    // Broadcast the "show yourself" message so the existing instance can
    // bring its main window to the foreground. Call from the second instance.
    static void NotifyExisting();

    // The registered message id that the primary instance listens for.
    static UINT ActivationMessage();

private:
    HANDLE handle_ = nullptr;
    bool   alreadyRunning_ = false;
};

}  // namespace superwin
