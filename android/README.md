# Skrumi для Android

См. [руководство Android](../docs/ANDROID.md): AAR для Java/Kotlin, SkrumiView,
сборка APK из `.skm`, ввод, жизненный цикл, ограничения Android и тестирование.

```bash
./gradlew :skrumi:assembleRelease :app:assembleDebug
./gradlew :skrumi:connectedDebugAndroidTest -PskrumiAbis=x86_64
```
