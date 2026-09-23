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

# Если доступна консольная утилита code, устанавливаем .vsix напрямую
if command -v code >/dev/null 2>&1 && [ -n "$VSIX_FILE" ]; then
  if code --install-extension "$VSIX_FILE" --force >/dev/null 2>&1; then
    echo "✔ Расширение FoxLang для VS Code успешно установлено через команду 'code'"
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
echo "  • Kate:    подсветка активна; включите плагин 'Клиент LSP' в настройках Kate."
echo "  • VS Code: расширение установлено (распознавание языка .fox, подсветка и LSP)."
case ":$PATH:" in
  *":$BINDIR:"*) ;;
  *) echo "" && echo "Внимание: добавьте $BINDIR в переменную PATH, если команды foxlang и foxlang-lsp пока не находятся." ;;
esac
