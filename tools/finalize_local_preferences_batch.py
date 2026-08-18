#!/usr/bin/env python3
"""Finalize local preference ownership without granting menu/gameplay authority."""
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
HPP=ROOT/"native-android/app/src/main/cpp/game/Game.hpp"
DESKTOP=ROOT/"native-desktop/main.cpp"
ANDROID=ROOT/"native-android/app/src/main/cpp/native_bridge.cpp"
TEST=ROOT/"native/tests/gameplay_state_contracts_test.cpp"
BASH=ROOT/"scripts/verify-gameplay.sh"
PS=ROOT/"scripts/verify-gameplay.ps1"
OLD_HELPER=ROOT/"tools/apply_local_settings_ownership.py"

def once(text,old,new,label):
    n=text.count(old)
    if n!=1: raise RuntimeError(f"{label}: expected one match, found {n}")
    return text.replace(old,new,1)

def main():
  try:
    hpp=HPP.read_text(encoding="utf-8")
    desktop=DESKTOP.read_text(encoding="utf-8")
    android=ANDROID.read_text(encoding="utf-8")
    test=TEST.read_text(encoding="utf-8")
    bash=BASH.read_text(encoding="utf-8")
    ps=PS.read_text(encoding="utf-8")

    api='''    void applyLocalPreferences(const LocalSettingsState& settings) {\n        auto& local=state_.localSettings;\n        local.musicVolume=settings.musicVolume;\n        local.sfxVolume=settings.sfxVolume;\n        local.musicMuted=settings.musicMuted;\n        local.sfxMuted=settings.sfxMuted;\n        local.graphicsPreset=settings.graphicsPreset;\n        local.shadows=settings.shadows;\n        local.portalWindow=settings.portalWindow;\n        local.particles=settings.particles;\n        local.fpsCounter=settings.fpsCounter;\n        local.mouseLookSensitivity=settings.mouseLookSensitivity;\n        local.touchLookSensitivity=settings.touchLookSensitivity;\n        local.controllerLookSensitivity=settings.controllerLookSensitivity;\n        local.controllerTriggerSensitivity=settings.controllerTriggerSensitivity;\n        local.controllerVibration=settings.controllerVibration;\n        local.keyboardBindings=settings.keyboardBindings;\n    }\n    void setMobileFraming(bool enabled) { state_.localSettings.mobileFraming=enabled; }\n'''
    hpp=once(hpp,
      "    void setPersistentProgression(std::int64_t tokens, int shotLevel, int lungeLevel, int attackLevel);\n",
      "    void setPersistentProgression(std::int64_t tokens, int shotLevel, int lungeLevel, int attackLevel);\n"+api,
      "local preference owner APIs")

    desktop=once(desktop,
      "    if(version>=2)game.networkMutableState().localSettings=settings;\n",
      "    if(version>=2)game.applyLocalPreferences(settings);\n",
      "desktop persistence owner")

    old_android='extern "C" JNIEXPORT void JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_setLocalSettings(JNIEnv*,jclass,jfloat music,jfloat sfx,jboolean musicMuted,jboolean sfxMuted,jint preset,jboolean shadows,jboolean portal,jboolean particles,jboolean fps){auto& settings=gGame.networkMutableState().localSettings;settings.musicVolume=clampf(music,0,1);settings.sfxVolume=clampf(sfx,0,1);settings.musicMuted=musicMuted==JNI_TRUE;settings.sfxMuted=sfxMuted==JNI_TRUE;settings.graphicsPreset=std::max(0,std::min(2,static_cast<int>(preset)));settings.shadows=shadows==JNI_TRUE;settings.portalWindow=portal==JNI_TRUE;settings.particles=particles==JNI_TRUE;settings.fpsCounter=fps==JNI_TRUE;settings.mobileFraming=true;}\n'
    new_android='extern "C" JNIEXPORT void JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_setLocalSettings(JNIEnv*,jclass,jfloat music,jfloat sfx,jboolean musicMuted,jboolean sfxMuted,jint preset,jboolean shadows,jboolean portal,jboolean particles,jboolean fps){LocalSettingsState settings=gGame.state().localSettings;settings.musicVolume=clampf(music,0,1);settings.sfxVolume=clampf(sfx,0,1);settings.musicMuted=musicMuted==JNI_TRUE;settings.sfxMuted=sfxMuted==JNI_TRUE;settings.graphicsPreset=std::max(0,std::min(2,static_cast<int>(preset)));settings.shadows=shadows==JNI_TRUE;settings.portalWindow=portal==JNI_TRUE;settings.particles=particles==JNI_TRUE;settings.fpsCounter=fps==JNI_TRUE;gGame.applyLocalPreferences(settings);gGame.setMobileFraming(true);}\n'
    android=once(android,old_android,new_android,"Android preference/platform capability owner")

    contract='''\n    {\n        Game game;\n        auto& current = game.networkMutableState().localSettings;\n        current.menuPage = LocalMenuPage::Controls;\n        current.menuScroll = 77.0f;\n        current.menuHistoryDepth = 2;\n        current.rebindingAction = 4;\n        current.mobileFraming = true;\n\n        LocalSettingsState incoming = current;\n        incoming.musicVolume = 0.22f;\n        incoming.graphicsPreset = 2;\n        incoming.keyboardBindings[0] = 73;\n        incoming.menuPage = LocalMenuPage::Graphics;\n        incoming.menuScroll = 9.0f;\n        incoming.menuHistoryDepth = 0;\n        incoming.rebindingAction = -1;\n        incoming.mobileFraming = false;\n\n        game.applyLocalPreferences(incoming);\n        const auto& applied = game.state().localSettings;\n        if (!near(applied.musicVolume, 0.22f) || applied.graphicsPreset != 2 ||\n            applied.keyboardBindings[0] != 73)\n            return fail("local_preferences_apply_persistent_fields");\n        if (applied.menuPage != LocalMenuPage::Controls || !near(applied.menuScroll, 77.0f) ||\n            applied.menuHistoryDepth != 2 || applied.rebindingAction != 4)\n            return fail("local_preferences_preserve_menu_session");\n        if (!applied.mobileFraming)\n            return fail("local_preferences_do_not_own_mobile_framing");\n        game.setMobileFraming(false);\n        if (game.state().localSettings.mobileFraming)\n            return fail("mobile_framing_has_explicit_owner_api");\n    }\n'''
    test=once(test,
      "\n    {\n        Game guest;\n        guest.configureNetworkGuest(1);\n",
      contract+"\n    {\n        Game guest;\n        guest.configureNetworkGuest(1);\n",
      "local preference ownership contract")

    bash=once(bash,
      "run_logged local-settings-ownership-plan python3 tools/apply_local_settings_ownership.py\n",
      "",
      "remove obsolete Bash settings planner")
    ps=once(ps,
      'python tools/apply_local_settings_ownership.py\nif ($LASTEXITCODE -ne 0) { throw "Local settings ownership plan check failed with exit code $LASTEXITCODE" }\n\n',
      "",
      "remove obsolete PowerShell settings planner")

    HPP.write_text(hpp,encoding="utf-8")
    DESKTOP.write_text(desktop,encoding="utf-8")
    ANDROID.write_text(android,encoding="utf-8")
    TEST.write_text(test,encoding="utf-8")
    BASH.write_text(bash,encoding="utf-8")
    PS.write_text(ps,encoding="utf-8")
    if not OLD_HELPER.exists(): raise RuntimeError("obsolete settings helper missing")
    OLD_HELPER.unlink()
  except (OSError,RuntimeError) as exc:
    print(f"LOCAL_PREFERENCES_BATCH_FAIL {exc}",file=sys.stderr);return 1
  print("LOCAL_PREFERENCES_BATCH_APPLIED=PASS");return 0
if __name__=="__main__": raise SystemExit(main())
