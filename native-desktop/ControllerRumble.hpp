#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TouchpadHapticEffect {
    TouchpadHapticNavigate,
    TouchpadHapticConfirm
} TouchpadHapticEffect;

void controllerRumblePulse(float lowFrequency, float highFrequency, int durationMilliseconds);
void controllerRumbleUpdate();
void controllerRumbleStop();
void touchpadHapticPulse(TouchpadHapticEffect effect, int vibrationSetting);

#ifdef __cplusplus
}
#endif
