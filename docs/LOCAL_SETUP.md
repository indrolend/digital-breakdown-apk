# Local Setup

## Windows

1. Clone or pull the repo.
2. Run npm ci.
3. Create android/local.properties with:

    sdk.dir=C:/Users/indro/AppData/Local/Android/Sdk

4. Build/install/run:

    .\scripts\build-install-run.ps1

## Mac

1. Clone or pull the repo.
2. Run npm ci.
3. Create android/local.properties with:

    sdk.dir=/Users/joelgutierrez/Library/Android/sdk

4. Use Java 21:

    export JAVA_HOME=$(/usr/libexec/java_home -v 21)
    export PATH="$JAVA_HOME/bin:$PATH"

5. Build APK:

    ./scripts/build-android.sh

## Required tools

- Git
- Node.js / npm
- Android SDK
- JDK 21
- ADB/platform-tools for device install
- scrcpy optional

## Notes

- Use npm ci on fresh clones.
- android/local.properties is machine-specific and should not be committed.
- www/android-bundle.js is generated and should not be committed.
- Java 21 is required by the current Android/Capacitor build.
