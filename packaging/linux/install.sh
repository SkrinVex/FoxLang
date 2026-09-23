#!/usr/bin/env bash
set -euo pipefail

PREFIX="${FOXLANG_PREFIX:-$HOME/.local}"
LIBDIR="$PREFIX/share/foxlang"
BINDIR="$PREFIX/bin"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mkdir -p "$LIBDIR" "$BINDIR"
install -m755 "$HERE/foxlang.bin" "$LIBDIR/foxlang"
if [ -f "$HERE/foxlang-lsp.bin" ]; then
  install -m755 "$HERE/foxlang-lsp.bin" "$LIBDIR/foxlang-lsp"
fi
rm -rf "$LIBDIR/std"
cp -R "$HERE/std" "$LIBDIR/std"
cp "$HERE/VERSION" "$LIBDIR/VERSION"

# Сохраняем каталог editors в share/foxlang
if [ -d "$HERE/editors" ]; then
  rm -rf "$LIBDIR/editors"
  cp -R "$HERE/editors" "$LIBDIR/editors"
fi

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

# 2. Автоматическая настройка Visual Studio Code
VSCODE_EXT_DIR="$HOME/.vscode/extensions/foxlang"
if [ -d "$HOME/.vscode" ] || [ -d "$HOME/.vscode/extensions" ]; then
  mkdir -p "$HOME/.vscode/extensions"
  if [ -d "$HERE/editors/vscode" ]; then
    rm -rf "$VSCODE_EXT_DIR"
    cp -R "$HERE/editors/vscode" "$VSCODE_EXT_DIR"
    echo "✔ Расширение для VS Code установлено в $VSCODE_EXT_DIR"
  fi
fi

cat > "$LIBDIR/uninstall.sh" <<EOF
#!/usr/bin/env bash
set -e
rm -f "$BINDIR/foxlang" "$BINDIR/foxlang-lsp"
rm -f "$HOME/.local/share/org.kde.syntax-highlighting/syntax/foxlang.xml"
rm -rf "$HOME/.vscode/extensions/foxlang"
rm -rf "$LIBDIR"
echo "FoxLang и связанные конфигурации редакторов удалены."
EOF
chmod +x "$LIBDIR/uninstall.sh"

echo ""
echo "FoxLang $(cat "$HERE/VERSION") успешно установлен."
echo "Команда CLI:  $BINDIR/foxlang"
if [ -f "$BINDIR/foxlang-lsp" ]; then
  echo "Сервер LSP:   $BINDIR/foxlang-lsp"
fi
echo "Удаление:     $LIBDIR/uninstall.sh"
echo ""
echo "Редакторы:"
echo "  • Kate:    подсветка активна; включите плагин 'Клиент LSP' в настройках Kate."
echo "  • VS Code: файлы расширения доступны в $LIBDIR/editors/vscode"
echo "             (установка: cp -R $LIBDIR/editors/vscode ~/.vscode/extensions/foxlang)"
case ":$PATH:" in
  *":$BINDIR:"*) ;;
  *) echo "" && echo "Добавьте $BINDIR в PATH, если команды foxlang и foxlang-lsp пока не находятся в терминале." ;;
esac
