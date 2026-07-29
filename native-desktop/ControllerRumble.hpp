#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void controllerRumblePulse(float lowFrequency, float highFrequency, int durationMilliseconds);
void controllerRumbleUpdate();
void controllerRumbleStop();

#ifdef __cplusplus
}
#endif
