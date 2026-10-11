# FoxLang на Android

FoxLang поддерживает два способа работы на **Android 9 (API 28) и новее**:

- библиотека **AAR** с Java API, доступным из Kotlin, и `FoxView` для графики;
- упаковка проекта `.fox` в **APK** с готовой Activity, экранными кнопками,
  клавиатурой, выводом и остановкой программы.

Это тот же C++ runtime, что на ПК: модули, VM, `graphics`, `ui`, PNG/BMP,
WAV/тоны, файлы, HTTP/HTTPS, серверы и TCP. Браузер или сервер для исполнения
не требуются. `os_platform()` и `using os; platform()` возвращают `android`.
Стандартная библиотека и корневые сертификаты встроены в нативную библиотеку.

Поддерживаются `arm64-v8a`, `armeabi-v7a`, `x86_64`, `x86`. По умолчанию собираются
ARM64 и x86_64; все четыре ABI проверяются при сборке CI. 64-битные библиотеки
совместимы со страницами памяти 16 КБ. Версия AAR и APK берётся из `VERSION`.

## Подготовка и сборка

Нужны JDK 17 или новее, Python 3 (для упаковщика), Android SDK и NDK.
Закреплённые версии: Gradle 8.11.1 (wrapper включён), Android Gradle Plugin 8.9.2,
SDK / build-tools 35, NDK 27.3.13750724, CMake 3.22.1.
Установите через SDK Manager Android Studio или командой:

```bash
sdkmanager "platform-tools" "platforms;android-35" "build-tools;35.0.0" \
  "ndk;27.3.13750724" "cmake;3.22.1"
export ANDROID_HOME=/путь/к/Android/Sdk
cd android
./gradlew :foxlang:assembleRelease :app:assembleDebug
```

На Windows используйте `gradlew.bat` и задайте `ANDROID_HOME` в окружении.
Gradle также принимает `sdk.dir` в локальном `android/local.properties`.
`FOXLANG_NDK_DIR` позволяет указать отдельно установленный NDK.

- AAR: `android/foxlang/build/outputs/aar/foxlang-release.aar`.
- Пример APK со списком дел: `android/app/build/outputs/apk/debug/app-debug.apk`.
- Для всех ABI добавьте `-PfoxlangAbis=arm64-v8a,armeabi-v7a,x86_64,x86`.
- Релиз GitHub содержит `foxlang-android.aar` и `foxlang-android-demo.apk`.

Первый запуск сборки скачивает Gradle, зависимости Maven, libcurl и Mbed TLS.
Последние закреплены хешами в CMake, Gradle wrapper проверяет SHA-256 дистрибутива.

## Собрать APK из своей программы

Из корня репозитория:

```bash
python3 packaging/build_android.py /projects/game/main.fox \
  --project /projects/game --asset images --asset sounds \
  --application-id com.example.mygame --name "Моя игра" \
  --abis arm64-v8a,x86_64 -o game.apk
adb install -r game.apk
```

Упаковщик сохраняет относительные пути всех `.fox` внутри проекта, включая
`layout.fox` и `main.fox` из [конструктора](BUILDER.md). Входной файл может быть
во вложенном каталоге. Остальные ресурсы добавляются только через повторяемый
`--asset`. Скрытые файлы, `.env`, каталоги сборки и символьные ссылки исключены
либо отклоняются; посторонний файл с ключом не попадёт в APK автоматически.

Без `--project` корнем проекта служит каталог входного файла. Ресурсы читаются
относительно корня проекта, а `include("helper.fox")` — относительно файла с
этим вызовом. После установки файлы копируются во внутренний каталог приложения.
Записанные программой данные сохраняются между запусками; при обновлении APK
поставляемые файлы обновляются. Удаление приложения удаляет его данные.

Обычная сборка подписана **отладочным ключом**. Для распространения:

```bash
python3 packaging/build_android.py main.fox --release -o app-unsigned.apk
apksigner sign --ks my-release.jks --out app.apk app-unsigned.apk
apksigner verify --verbose app.apk
```

Свой ключ храните вне проекта. `--release` не подписывает APK чужим или
автоматически созданным ключом. Для AAB/Google Play используйте проект `android/app`
как основу, задайте собственную `signingConfig` и запустите `:app:bundleRelease`.
Команда настольного CLI `foxlang build` по-прежнему создаёт executable текущей ОС;
для Android используется этот упаковщик с SDK/NDK.

## Встраивание в Kotlin / Java

Положите AAR в `app/libs` и подключите его в Gradle:

```kotlin
dependencies {
    implementation(files("libs/foxlang-android.aar"))
}
```

Минимальный Kotlin-пример для Activity:

```kotlin
import org.foxlang.android.FoxLang
import org.foxlang.android.FoxView
import java.io.File

val view = FoxView(this)
setContentView(view)
val engine = FoxLang(File(filesDir, "program"), view, "--level", "2")
view.setEngine(engine)
view.setOutputListener { text, _ -> runOnUiThread { /* добавить text в свой вывод */ } }
engine.runSource("print(40 + 2);").thenAccept { result ->
    runOnUiThread { title = if (result.success) result.output else result.error }
}
```

Объект `engine` сохраняйте как поле Activity. В `onStop()` вызывайте
`engine.cancel()`, а в `onDestroy()` — `engine.close()`. Возврат после остановки
не возобновляет прерванную VM: запустите программу снова, при необходимости
сначала вызвав `reset()`. Пример этого жизненного цикла —
[`MainActivity.java`](../android/app/src/main/java/org/foxlang/runner/MainActivity.java).

Методы исполнения, переменных и сброса возвращают `CompletableFuture`:

| Метод | Назначение |
|---|---|
| `runSource(source)` | Выполнить текст с именем `main.fox` |
| `runSource(source, filename, stdin)` | Задать имя для ошибок/импортов и строки для `input()` |
| `runFile(filename, stdin)` | Выполнить файл; вариант без `stdin` передаёт пустой ввод |
| `getGlobal(name)` | Получить `Value.type` и текстовое представление `Value.text` |
| `setGlobal(name, type, text)` | Определить скалярную переменную из Java/Kotlin |
| `reset()` | Очистить переменные, модули, окно и сервер |
| `cancel()` | Остановить текущее исполнение и уже поставленные в очередь запуски |
| `close()` | Отменить исполнение и освободить интерпретатор после его остановки |
| `FoxLang.version()` | Версия runtime |
| `FoxLang.thirdPartyNotices()` | Тексты лицензий встроенных зависимостей |

`RunResult` содержит `success`, `exitCode`, `error`, `output`; `cancelled()`
проверяет код 130. Синтаксические ошибки возвращаются в результате, ошибки
использования закрытого SDK — в exceptionally-завершённом future.
Захваченный `output` ограничен первым миллионом UTF-8-байт; `onOutput` получает
весь вывод. `input()` читает заранее переданный `stdin` построчно; после конца —
пустую строку. Для интерактивного текста используйте `ui_text_field`.

Интерпретаторы имеют отдельные переменные и модули, но выполняются **последовательно
на общем рабочем потоке**, поскольку нативный аллокатор общий для процесса.
Долгая программа занимает очередь; `cancel()` и события ввода доставляются сразу.
Нельзя вызывать `.get()` / `.join()` из Host callbacks или из UI-потока.
Отмена проверяется в VM и `wait()`. Блокирующий системный ввод-вывод завершает свой
вызов/тайм-аут прежде, чем VM увидит отмену; это не принудительное завершение потока.

Без графики вместо `FoxView` передайте `object : FoxLang.Host {}`. Свой Host может
перехватить вывод, кадры ARGB, окно, буфер обмена, звук и URI. По умолчанию
графика требует подключённый Host, а звук/URI возвращают `false`.
Callbacks выполняются на рабочем потоке; изменения Android UI отправляйте через
`post()` / `runOnUiThread()`. `FoxView` делает это самостоятельно.

## Графика, клавиатура, звук

`open_window`, `graphics` и `ui` рисуют в `FoxView` внутри Activity.
Окно по умолчанию подстраивается под доступное место в **dp**, включая поворот,
многооконный режим и появление клавиатуры. Размер читается через `window_width()` /
`window_height()`, событие — `window_resized()` после `window_poll()`.
Расположение элементов программа рассчитывает сама.

`set_window_resizable(false)` оставляет логическое разрешение фиксированным:
кадр масштабируется с сохранением пропорций, касания пересчитываются обратно.
`set_window_size` меняет логический холст, а не размеры Activity в системе.
`set_window_fullscreen` скрывает системные панели; свайп от края временно их
возвращает. `FoxView.setFullscreenListener` позволяет Activity скрыть свою панель
управления; готовый runner скрывает также консоль и поле `stdin`.

Касание соответствует левой кнопке мыши; поддерживаются отпускание/отмена жеста,
аппаратная клавиатура, мышь и колесо. Кнопка клавиатуры вызывает
`view.showKeyboard()`. Сначала выберите нужное `ui_text_field` касанием.
IME передаёт Unicode, составной ввод и удаление; потеря фокуса освобождает клавиши.
`FoxControls(context, engine, view)` добавляет стрелки, пробел и клавиатуру;
четвёртый аргумент `true` заменяет стрелки на WASD. Кнопки можно удерживать
одновременно, текст на них не выделяется. Свои кнопки могут вызывать `key()`,
`text()`, `pointer()`, `wheel()` и `closeWindow()` у SDK.

WAV и `play_tone` воспроизводятся через Android MediaPlayer без внешних утилит.
`stop_sound()` и завершение запуска освобождают проигрыватели.

## Особенности Android

- `INTERNET` включён в манифест AAR. libcurl и Mbed TLS используют встроенный
  Mozilla CA bundle с проверкой сертификата и имени; это не Java HTTP-клиент,
  поэтому Android Network Security Config к нему не применяется.
- Относительные файловые пути привязаны к каталогу интерпретатора без глобального
  `chdir()`. Доступ к файлам определяется разрешениями Android-приложения.
  SDK не является отдельной песочницей для недоверенного кода и не выдаёт
  разрешение на произвольное внешнее хранилище.
- `os_open` через `FoxView` передаёт `http`, `https`, `mailto`, `content` URI в
  Android Intent. Для обычного файла используйте FileProvider в приложении и
  передайте `content://`; desktop-пути не открываются автоматически.
- Терминала у Activity нет: `getch()` возвращает пустую строку, `kbhit()` — `false`.
  ANSI-коды не превращают TextView в терминал. Используйте графический ввод.
- Системные процессы, окружение, права/владельцы файлов ограничены Android и UID
  приложения. `.env` не загружается автоматически. Настольные флаги консоли
  неприменимы; пользовательские аргументы передаются конструктору SDK либо
  строковым массивом Intent extra `foxlang.arguments` готового runner.

## C++ / NDK без Java

```bash
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
  -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-android -j4
```

Цели: `foxlang_core` (статическая библиотека) и `foxlang_android` (JNI `.so`).
`-DFOXLANG_BUILD_ANDROID_JNI=OFF` отключает JNI. Для встраивания через CMake
используйте `add_subdirectory` и `target_link_libraries(... foxlang_core)`, чтобы
получить также транзитивные зависимости curl/TLS.
API интерпретатора находится в `FoxLang.h`, платформенный контракт — в
[`Android.h`](../include/foxlang/Android.h). Для окна, звука и перенаправления
ввода создайте `android::Scope` со своим Host; уничтожьте окно/интерпретатор до
Scope. C++-клиент сам сериализует доступ к ядру и доставку событий.

## Проверки

```bash
python3 tests/test_android_packaging.py
cd android
./gradlew :foxlang:connectedDebugAndroidTest -PfoxlangAbis=x86_64
```

Вторая команда требует запущенный эмулятор или устройство. CI собирает четыре
ABI, проверяет содержимое AAR/APK и выравнивание библиотек, выполняет instrumentation
на Android 9 и Android 15. Проверяются импорты, Unicode, переменные, файловый ввод,
аргументы, сеть, графика, UI-события, размер окна, буфер обмена, WAV и отмена VM.
Android instrumentation и браузерные тесты считаются отдельно от Linux CTest.
