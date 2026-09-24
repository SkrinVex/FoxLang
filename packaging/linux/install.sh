#!/usr/bin/env bash
set -euo pipefail

PREFIX="${FOXLANG_PREFIX:-$HOME/.local}"
LIBDIR="$PREFIX/share/foxlang"
BINDIR="$PREFIX/bin"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="$(tr -d '[:space:]' < "$HERE/VERSION")"

mkdir -p "$LIBDIR" "$BINDIR"
install -m755 "$HERE/foxlang.bin" "$LIBDIR/foxlang"
if [ -f "$HERE/foxlang-lsp.bin" ]; then
  install -m755 "$HERE/foxlang-lsp.bin" "$LIBDIR/foxlang-lsp"
fi
rm -rf "$LIBDIR/std"
cp -R "$HERE/std" "$LIBDIR/std"
cp "$HERE/VERSION" "$LIBDIR/VERSION"

# Сохраняем каталог editors и пакет vsix в share/foxlang
if [ -d "$HERE/editors" ]; then
  rm -rf "$LIBDIR/editors"
  cp -R "$HERE/editors" "$LIBDIR/editors"
fi
for vsix in "$HERE"/foxlang-*.vsix; do
  if [ -f "$vsix" ]; then
    cp "$vsix" "$LIBDIR/"
  fi
done

# Создаем исполняемые обёртки в BINDIR
cat > "$BINDIR/foxlang" <<EOF
#!/usr/bin/env bash
export FOXLANG_HOME="$LIBDIR"
exec "$LIBDIR/foxlang" "\$@"
EOF
chmod +x "$BINDIR/foxlang"

if [ -f "$LIBDIR/foxlang-lsp" ]; then
  cat > "$BINDIR/foxlang-lsp" <<EOF
#!/usr/bin/env bash
export FOXLANG_HOME="$LIBDIR"
exec "$LIBDIR/foxlang-lsp" "\$@"
EOF
  chmod +x "$BINDIR/foxlang-lsp"
fi

# 1. Автоматическая настройка редактора Kate (KDE)
KATE_SYNTAX_DIR="$HOME/.local/share/org.kde.syntax-highlighting/syntax"
KATE_LSP_DIR="$HOME/.config/kate/lspclient"
mkdir -p "$KATE_SYNTAX_DIR" "$KATE_LSP_DIR"

if [ -f "$HERE/editors/kate/foxlang.xml" ]; then
  cp "$HERE/editors/kate/foxlang.xml" "$KATE_SYNTAX_DIR/foxlang.xml"
  echo "✔ Подсветка синтаксиса для Kate установлена в $KATE_SYNTAX_DIR/foxlang.xml"
fi

# Внешние инструменты Kate: запуск, проверка и сборка текущего файла (меню Сервис).
KATE_TOOLS_DIR="$HOME/.config/kate/externaltools"
if [ -d "$HERE/editors/kate/externaltools" ]; then
  mkdir -p "$KATE_TOOLS_DIR"
  for tool in "$HERE"/editors/kate/externaltools/*.ini; do
    target="$KATE_TOOLS_DIR/$(basename "$tool")"
    if [ ! -f "$target" ]; then
      sed "s|^executable=foxlang$|executable=$BINDIR/foxlang|" "$tool" > "$target"
    fi
  done
  echo "✔ Инструменты Kate (Сервис → Внешние инструменты → FoxLang) установлены в $KATE_TOOLS_DIR"
fi

# Отладчик Kate (плагин «Отладчик»): адаптер foxlang debug-adapter.
KATE_DAP_DIR="$HOME/.config/kate/debugger"
if [ -f "$HERE/editors/kate/dap.json" ]; then
  mkdir -p "$KATE_DAP_DIR"
  if [ ! -f "$KATE_DAP_DIR/dap.json" ]; then
    cp "$HERE/editors/kate/dap.json" "$KATE_DAP_DIR/dap.json"
    echo "✔ Отладчик FoxLang для Kate настроен в $KATE_DAP_DIR/dap.json"
  elif ! grep -q '"foxlang"' "$KATE_DAP_DIR/dap.json" 2>/dev/null; then
    cp "$HERE/editors/kate/dap.json" "$LIBDIR/kate-dap.json"
    echo "ℹ Настройки отладчика Kate сохранены в $LIBDIR/kate-dap.json (добавьте секцию 'foxlang' в $KATE_DAP_DIR/dap.json)"
  else
    echo "✔ Отладчик FoxLang для Kate уже настроен в $KATE_DAP_DIR/dap.json"
  fi
fi

if [ -f "$HERE/editors/kate/settings.json" ]; then
  if [ ! -f "$KATE_LSP_DIR/settings.json" ]; then
    cp "$HERE/editors/kate/settings.json" "$KATE_LSP_DIR/settings.json"
    echo "✔ Конфигурация LSP для Kate создана в $KATE_LSP_DIR/settings.json"
  else
    if ! grep -q '"foxlang"' "$KATE_LSP_DIR/settings.json" 2>/dev/null; then
      cp "$HERE/editors/kate/settings.json" "$LIBDIR/kate-lsp-settings.json"
      echo "ℹ Конфигурация Kate LSP сохранена в $LIBDIR/kate-lsp-settings.json (добавьте секцию 'foxlang' в $KATE_LSP_DIR/settings.json)"
    else
      echo "✔ Конфигурация Kate LSP уже настроена в $KATE_LSP_DIR/settings.json"
    fi
  fi
fi

# 2. Автоматическая интеграция с Visual Studio Code / VSCodium / Flatpak
VSIX_FILE=""
for f in "$HERE"/foxlang-*.vsix; do
  if [ -f "$f" ]; then
    VSIX_FILE="$f"
    break
  fi
done

# Расширение ставится одним способом на каждый редактор: две копии запускали бы
# два языковых сервера и дважды регистрировали команды. Если есть утилита code,
# VS Code получает .vsix, а каталог копируется только редакторам без неё.
VSCODE_BY_CLI=0
if command -v code >/dev/null 2>&1 && [ -n "$VSIX_FILE" ]; then
  # Копия каталога от прежних установок мешала бы пакету из .vsix.
  rm -rf "$HOME/.vscode/extensions/SkrinVex.foxlang-language"* "$HOME/.vscode/extensions/foxlang"* 2>/dev/null || true
  if code --install-extension "$VSIX_FILE" --force >/dev/null 2>&1; then
    VSCODE_BY_CLI=1
    echo "✔ Расширение FoxLang для VS Code установлено через команду 'code'"
  fi
fi

# Прямая установка в каталоги расширений (VS Code, VSCodium, Flatpak)
EXTENSION_TARGET_BASES=(
  "$HOME/.vscode/extensions"
  "$HOME/.vscode-oss/extensions"
  "$HOME/.var/app/com.visualstudio.code/data/vscode/extensions"
  "$HOME/.var/app/com.vscodium.codium/data/vscode/extensions"
)

for ext_base in "${EXTENSION_TARGET_BASES[@]}"; do
  if [ "$VSCODE_BY_CLI" = 1 ] && [ "$ext_base" = "$HOME/.vscode/extensions" ]; then
    continue
  fi
  parent_dir="$(dirname "$ext_base")"
  if [ -d "$parent_dir" ] || [ -d "$ext_base" ]; then
    mkdir -p "$ext_base"
    TARGET_EXT_DIR="$ext_base/SkrinVex.foxlang-language-$VERSION"

    # Удаляем старые или некорректно названные версии
    rm -rf "$ext_base/foxlang"* "$ext_base/SkrinVex.foxlang"* "$ext_base/skrinvex.foxlang"* 2>/dev/null || true

    # Снимаем пометку obsolete, если она была установлена VS Code
    if [ -f "$ext_base/.obsolete" ]; then
      sed -i -E '/(foxlang|SkrinVex)/d' "$ext_base/.obsolete" 2>/dev/null || true
    fi

    if [ -d "$HERE/editors/vscode" ]; then
      cp -R "$HERE/editors/vscode" "$TARGET_EXT_DIR"
      echo "✔ Каталог расширения VS Code установлен в $TARGET_EXT_DIR"
    fi
  fi
done

# 3. Поддержка Zed IDE
if [ -d "$HERE/editors/zed" ]; then
  mkdir -p "$LIBDIR/editors/zed"
  cp -R "$HERE/editors/zed/"* "$LIBDIR/editors/zed/"
  echo "✔ Расширение Zed IDE скопировано в $LIBDIR/editors/zed"
fi

if [ -d "$HOME/.config/zed" ]; then
  ZED_SETTINGS="$HOME/.config/zed/settings.json"
  if [ ! -f "$ZED_SETTINGS" ]; then
    cat > "$ZED_SETTINGS" <<'ZED_EOF'
{
  "lsp": {
    "foxlang-lsp": {
      "binary": {
        "path": "foxlang-lsp",
        "arguments": ["--stdio"]
      }
    }
  },
  "languages": {
    "C": {
      "language_servers": ["foxlang-lsp", "..."]
    }
  },
  "file_types": {
    "C": ["fox"]
  }
}
ZED_EOF
    echo "✔ Настройки FoxLang для Zed IDE записаны в $ZED_SETTINGS"
  fi
fi

# Скрипт полного удаления
cat > "$LIBDIR/uninstall.sh" <<EOF
#!/usr/bin/env bash
set -e
if command -v code >/dev/null 2>&1; then
  code --uninstall-extension SkrinVex.foxlang-language 2>/dev/null || true
  code --uninstall-extension SkrinVex.foxlang 2>/dev/null || true
  code --uninstall-extension foxlang.foxlang 2>/dev/null || true
fi
rm -f "$BINDIR/foxlang" "$BINDIR/foxlang-lsp"
rm -f "$HOME/.local/share/org.kde.syntax-highlighting/syntax/foxlang.xml"
rm -f "$HOME/.config/kate/externaltools/foxlang-run.ini" "$HOME/.config/kate/externaltools/foxlang-check.ini" "$HOME/.config/kate/externaltools/foxlang-build.ini"
rm -rf "$HOME/.vscode/extensions/foxlang"* "$HOME/.vscode/extensions/SkrinVex.foxlang"* 2>/dev/null || true
rm -rf "$HOME/.vscode-oss/extensions/foxlang"* "$HOME/.vscode-oss/extensions/SkrinVex.foxlang"* 2>/dev/null || true
rm -rf "$HOME/.var/app/com.visualstudio.code/data/vscode/extensions/foxlang"* "$HOME/.var/app/com.visualstudio.code/data/vscode/extensions/SkrinVex.foxlang"* 2>/dev/null || true
rm -rf "$HOME/.var/app/com.vscodium.codium/data/vscode/extensions/foxlang"* "$HOME/.var/app/com.vscodium.codium/data/vscode/extensions/SkrinVex.foxlang"* 2>/dev/null || true
rm -rf "$LIBDIR"
echo "FoxLang и связанные расширения редакторов успешно удалены."
EOF
chmod +x "$LIBDIR/uninstall.sh"

echo ""
echo "FoxLang $VERSION успешно установлен."
echo "Команда CLI:  $BINDIR/foxlang"
if [ -f "$BINDIR/foxlang-lsp" ]; then
  echo "Сервер LSP:   $BINDIR/foxlang-lsp"
fi
echo "Удаление:     $LIBDIR/uninstall.sh"
echo ""
echo "Редакторы:"
echo "  • Kate:    подсветка активна; включите плагины 'Клиент LSP' и 'Отладчик' в настройках Kate."
echo "  • VS Code: расширение установлено (подсветка, LSP, отладка по F5)."
echo "  • Zed IDE: dev-расширение в $LIBDIR/editors/zed (zed: install dev extension)."
case ":$PATH:" in
  *":$BINDIR:"*) ;;
  *) echo "" && echo "Внимание: добавьте $BINDIR в переменную PATH, если команды foxlang и foxlang-lsp пока не находятся." ;;
esac
