# FoxLang для Android

См. [руководство Android](../docs/ANDROID.md): AAR для Java/Kotlin, FoxView,
сборка APK из `.fox`, ввод, жизненный цикл, ограничения Android и тестирование.

```bash
./gradlew :foxlang:assembleRelease :app:assembleDebug
./gradlew :foxlang:connectedDebugAndroidTest -PfoxlangAbis=x86_64
```
