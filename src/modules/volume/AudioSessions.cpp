#include "modules/volume/AudioSessions.h"

#include <Windows.h>
#include <psapi.h>
#include <Functiondiscoverykeys_devpkey.h>

namespace superwin {

using Microsoft::WRL::ComPtr;

namespace {

std::wstring ProcessName(uint32_t pid) {
    if (pid == 0) return L"System Sounds";
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return L"PID " + std::to_wstring(pid);
    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    std::wstring name = L"PID " + std::to_wstring(pid);
    if (::QueryFullProcessImageNameW(h, 0, path, &size)) {
        std::wstring full(path, size);
        const size_t slash = full.find_last_of(L"\\/");
        name = (slash == std::wstring::npos) ? full : full.substr(slash + 1);
    }
    ::CloseHandle(h);
    return name;
}

}  // namespace

AudioController::AudioController() {
    // COM is initialized by the host (apartment-threaded); just create the graph.
    if (FAILED(::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&enumerator_))))
        return;
    if (FAILED(enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_)))
        return;
    device_->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, &endpoint_);
    device_->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, &manager_);
}

AudioController::~AudioController() = default;

float AudioController::MasterVolume() {
    float v = 0;
    if (endpoint_) endpoint_->GetMasterVolumeLevelScalar(&v);
    return v;
}

void AudioController::SetMasterVolume(float scalar) {
    if (endpoint_) endpoint_->SetMasterVolumeLevelScalar(scalar, nullptr);
}

bool AudioController::MasterMuted() {
    BOOL m = FALSE;
    if (endpoint_) endpoint_->GetMute(&m);
    return m != FALSE;
}

void AudioController::SetMasterMuted(bool muted) {
    if (endpoint_) endpoint_->SetMute(muted ? TRUE : FALSE, nullptr);
}

std::vector<AudioSessionInfo> AudioController::Refresh() {
    std::vector<AudioSessionInfo> result;
    sessionVolumes_.clear();
    if (!manager_) return result;

    ComPtr<IAudioSessionEnumerator> sessions;
    if (FAILED(manager_->GetSessionEnumerator(&sessions))) return result;

    int count = 0;
    sessions->GetCount(&count);
    for (int i = 0; i < count; ++i) {
        ComPtr<IAudioSessionControl> ctrl;
        if (FAILED(sessions->GetSession(i, &ctrl))) continue;
        ComPtr<IAudioSessionControl2> ctrl2;
        if (FAILED(ctrl.As(&ctrl2))) continue;

        // Skip inactive/expired sessions.
        AudioSessionState state{};
        ctrl->GetState(&state);

        DWORD pid = 0;
        ctrl2->GetProcessId(&pid);

        ComPtr<ISimpleAudioVolume> vol;
        if (FAILED(ctrl.As(&vol))) continue;

        AudioSessionInfo info;
        info.pid = pid;
        info.name = (ctrl2->IsSystemSoundsSession() == S_OK) ? L"System Sounds"
                                                             : ProcessName(pid);
        float scalar = 0;
        vol->GetMasterVolume(&scalar);
        info.volume = scalar;
        BOOL muted = FALSE;
        vol->GetMute(&muted);
        info.muted = muted != FALSE;

        sessionVolumes_.push_back(vol);
        result.push_back(std::move(info));
    }
    return result;
}

void AudioController::SetSessionVolume(size_t index, float scalar) {
    if (index < sessionVolumes_.size() && sessionVolumes_[index])
        sessionVolumes_[index]->SetMasterVolume(scalar, nullptr);
}

void AudioController::SetSessionMuted(size_t index, bool muted) {
    if (index < sessionVolumes_.size() && sessionVolumes_[index])
        sessionVolumes_[index]->SetMute(muted ? TRUE : FALSE, nullptr);
}

}  // namespace superwin
