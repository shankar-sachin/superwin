// WASAPI wrapper: master endpoint volume + per-application session volumes.
//
// Master volume is exposed in true decibels (IAudioEndpointVolume); per-app
// sessions are scalar [0,1] (ISimpleAudioVolume) with dB computed for display.
#pragma once

#include <string>
#include <vector>

#include <wrl/client.h>  // Microsoft::WRL::ComPtr
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>

namespace superwin {

struct AudioSessionInfo {
    std::wstring name;   // friendly app name
    uint32_t     pid = 0;
    float        volume = 0;  // scalar 0..1
    bool         muted = false;
};

class AudioController {
public:
    AudioController();
    ~AudioController();

    bool Valid() const { return device_ != nullptr; }

    // Master endpoint.
    float MasterVolume();         // scalar 0..1
    void  SetMasterVolume(float); // scalar 0..1
    bool  MasterMuted();
    void  SetMasterMuted(bool);

    // Per-app sessions. Refresh() rebuilds the list; subsequent Set*() use the
    // index into the most recent Refresh() result.
    std::vector<AudioSessionInfo> Refresh();
    void SetSessionVolume(size_t index, float scalar);
    void SetSessionMuted(size_t index, bool muted);

private:
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator>  enumerator_;
    Microsoft::WRL::ComPtr<IMMDevice>            device_;
    Microsoft::WRL::ComPtr<IAudioEndpointVolume> endpoint_;
    Microsoft::WRL::ComPtr<IAudioSessionManager2> manager_;
    std::vector<Microsoft::WRL::ComPtr<ISimpleAudioVolume>> sessionVolumes_;
};

}  // namespace superwin
