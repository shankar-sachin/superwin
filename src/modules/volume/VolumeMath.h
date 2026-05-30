// Pure conversions shared by every volume slider and readout.
//
// Windows audio exposes two notions of "volume":
//   * scalar amplitude in [0, 1]  (ISimpleAudioVolume / per-app, linear)
//   * decibels                    (IAudioEndpointVolume / master, true dB)
//
// The UI shows percent and dB everywhere, so these helpers centralise the
// math (and the "silence" clamping) in one tested place. No Windows deps.
#pragma once

#include <string>

namespace superwin {

// Anything quieter than this is treated as silence (-inf dB).
inline constexpr float kSilenceDb = -96.0f;

// scalar [0,1]  <->  percent [0,100]
int   PercentFromScalar(float scalar);
float ScalarFromPercent(int percent);

// scalar [0,1]  <->  decibels.  scalar == 0 maps to kSilenceDb.
float DbFromScalar(float scalar);
float ScalarFromDb(float db);

// Human-readable dB, e.g. "-12.3 dB" or "-∞ dB" at silence.
std::string FormatDb(float db);

// Clamp helpers used by the slider widgets.
float ClampScalar(float scalar);
int   ClampPercent(int percent);

}  // namespace superwin
