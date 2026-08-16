#include "ControllerRumble.hpp"

#import <AppKit/AppKit.h>
#import <CoreHaptics/CoreHaptics.h>
#import <GameController/GameController.h>

static CHHapticEngine* activeEngine = nil;
static id<CHHapticPatternPlayer> activePlayer = nil;

static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

void controllerRumblePulse(float lowFrequency, float highFrequency, int durationMilliseconds) {
    if (@available(macOS 11.0, *)) {
        @autoreleasepool {
            GCController* controller = GCController.controllers.firstObject;
            if (!controller.haptics) return;
            activeEngine = [controller.haptics createEngineWithLocality:GCHapticsLocalityDefault];
            if (!activeEngine) return;
            NSError* error = nil;
            if (![activeEngine startAndReturnError:&error]) return;
            const float intensity = clamp01(lowFrequency > highFrequency ? lowFrequency : highFrequency);
            const float total = lowFrequency + highFrequency;
            const float sharpness = clamp01(highFrequency / (total > 0.01f ? total : 0.01f));
            NSArray* parameters = @[
                [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:intensity],
                [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:sharpness]
            ];
            const bool transient = durationMilliseconds <= 40;
            CHHapticEvent* event = transient
                ? [[CHHapticEvent alloc]
                    initWithEventType:CHHapticEventTypeHapticTransient
                    parameters:parameters
                    relativeTime:0.0]
                : [[CHHapticEvent alloc]
                    initWithEventType:CHHapticEventTypeHapticContinuous
                    parameters:parameters
                    relativeTime:0.0
                    duration:durationMilliseconds / 1000.0];
            CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[event] parameters:@[] error:&error];
            activePlayer = pattern ? [activeEngine createPlayerWithPattern:pattern error:&error] : nil;
            [activePlayer startAtTime:CHHapticTimeImmediate error:&error];
        }
    }
}

void controllerRumbleUpdate() {}
void controllerRumbleStop() {
    if (@available(macOS 11.0, *)) {
        @autoreleasepool {
            [activePlayer stopAtTime:CHHapticTimeImmediate error:nil];
            [activeEngine stopWithCompletionHandler:nil];
            activePlayer = nil;
            activeEngine = nil;
        }
    }
}

void touchpadHapticPulse(TouchpadHapticEffect effect, int vibrationSetting) {
    if (vibrationSetting <= 0) return;
    @autoreleasepool {
        const NSHapticFeedbackPattern pattern =
            effect == TouchpadHapticNavigate
                ? NSHapticFeedbackPatternAlignment
                : NSHapticFeedbackPatternLevelChange;
        [[NSHapticFeedbackManager defaultPerformer]
            performFeedbackPattern:pattern
            performanceTime:NSHapticFeedbackPerformanceTimeNow];
    }
}
