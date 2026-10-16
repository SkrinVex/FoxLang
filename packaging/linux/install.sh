#!/usr/bin/env bash
set -euo pipefail

PREFIX="${SKRUMI_PREFIX:-${FOXLANG_PREFIX:-$HOME/.local}}"
LIBDIR="$PREFIX/share/skrumi"
BINDIR="$PREFIX/bin"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="$(tr -d '[:space:]' < "$HERE/VERSION")"

# Refuse unrelated destinations before writing any payload.
if [ -e "$LIBDIR" ] && [ ! -f "$LIBDIR/.skrumi-install" ]; then
  echo "Refusing unmanaged installation directory: $LIBDIR" >&2; exit 1
fi
if [ -d "$LIBDIR" ]; then
  if [ ! -f "$LIBDIR/.skrumi-files.sha256" ] ||
     [ -n "$(find "$LIBDIR" -type l -print -quit)" ] ||
     ! (cd "$LIBDIR" && sha256sum --quiet -c .skrumi-files.sha256) ||
     [ "$(find "$LIBDIR" -type f ! -name .skrumi-files.sha256 | wc -l)" -ne "$(wc -l < "$LIBDIR/.skrumi-files.sha256")" ]; then
    echo "Installation contains modified or additional files; preserve it and choose a new SKRUMI_PREFIX." >&2; exit 1
  fi
fi
for command in skrumi skrumi-lsp foxlang foxlang-lsp; do
  target="$BINDIR/$command"
  if [ -e "$target" ] || [ -L "$target" ]; then
    # Only replace launchers owned by this installer or the previous FoxLang installer.
    if ! cmp -s "$target" "$LIBDIR/launchers/$command" &&
       ! grep -Fq "$PREFIX/share/skrumi/" "$target" 2>/dev/null &&
       ! grep -Fq "$PREFIX/share/foxlang/" "$target" 2>/dev/null; then
      echo "Refusing to overwrite another command: $target" >&2; exit 1
    fi
  fi
done
mkdir -p "$LIBDIR" "$BINDIR"
printf '%s\n' "$VERSION" > "$LIBDIR/.skrumi-install"
install -m755 "$HERE/skrumi.bin" "$LIBDIR/skrumi"
if [ -f "$HERE/skrumi-lsp.bin" ]; then
  install -m755 "$HERE/skrumi-lsp.bin" "$LIBDIR/skrumi-lsp"
fi
rm -rf "$LIBDIR/std"
cp -R "$HERE/std" "$LIBDIR/std"
cp "$HERE/VERSION" "$LIBDIR/VERSION"

# Сохраняем каталог editors и пакет vsix в share/skrumi
if [ -d "$HERE/editors" ]; then
  rm -rf "$LIBDIR/editors"
  cp -R "$HERE/editors" "$LIBDIR/editors"
fi
for vsix in "$HERE"/skrumi-*.vsix; do
  if [ -f "$vsix" ]; then
    cp "$vsix" "$LIBDIR/"
  fi
done

# Quote paths as shell data, including spaces, quotes and dollar signs.
write_launcher() {
  printf '#!/usr/bin/env bash\nexport SKRUMI_HOME=%q\nexec %q "$@"\n' "$LIBDIR" "$LIBDIR/$2" > "$BINDIR/$1"
  chmod +x "$BINDIR/$1"
}
write_launcher skrumi skrumi
write_launcher foxlang skrumi
if [ -f "$LIBDIR/skrumi-lsp" ]; then
  write_launcher skrumi-lsp skrumi-lsp
  write_launcher foxlang-lsp skrumi-lsp
fi

# 1. Автоматическая настройка редактора Kate (KDE)
KATE_SYNTAX_DIR="$HOME/.local/share/org.kde.syntax-highlighting/syntax"
KATE_LSP_DIR="$HOME/.config/kate/lspclient"
mkdir -p "$KATE_SYNTAX_DIR" "$KATE_LSP_DIR"

if [ -f "$HERE/editors/kate/foxlang.xml" ] && {
  [ ! -e "$KATE_SYNTAX_DIR/foxlang.xml" ] ||
  grep -q 'author="SkrinVex"' "$KATE_SYNTAX_DIR/foxlang.xml";
}; then
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
      sed "s|^executable=skrumi$|executable=$BINDIR/skrumi|" "$tool" > "$target"
    fi
  done
  echo "✔ Инструменты Kate (Сервис → Внешние инструменты → Skrumi) установлены в $KATE_TOOLS_DIR"
fi

# Отладчик Kate (плагин «Отладчик»): адаптер foxlang debug-adapter.
KATE_DAP_DIR="$HOME/.config/kate/debugger"
if [ -f "$HERE/editors/kate/dap.json" ]; then
  mkdir -p "$KATE_DAP_DIR"
  if [ ! -f "$KATE_DAP_DIR/dap.json" ]; then
    cp "$HERE/editors/kate/dap.json" "$KATE_DAP_DIR/dap.json"
    echo "✔ Отладчик Skrumi для Kate настроен в $KATE_DAP_DIR/dap.json"
  elif ! grep -q '"foxlang"' "$KATE_DAP_DIR/dap.json" 2>/dev/null; then
    cp "$HERE/editors/kate/dap.json" "$LIBDIR/kate-dap.json"
    echo "ℹ Настройки отладчика Kate сохранены в $LIBDIR/kate-dap.json (добавьте секцию 'foxlang' в $KATE_DAP_DIR/dap.json)"
  else
    echo "✔ Отладчик Skrumi для Kate уже настроен в $KATE_DAP_DIR/dap.json"
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
for f in "$HERE"/skrumi-*.vsix; do
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
  if code --install-extension "$VSIX_FILE" --force >/dev/null 2>&1; then
    VSCODE_BY_CLI=1
    echo "✔ Расширение Skrumi для VS Code установлено через команду 'code'"
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

    if [ -d "$HERE/editors/vscode" ]; then
      mkdir -p "$TARGET_EXT_DIR"
      cp -R "$HERE/editors/vscode/." "$TARGET_EXT_DIR/"
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
        "path": "skrumi-lsp",
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
    "C": ["skm", "fox"]
  }
}
ZED_EOF
    echo "✔ Настройки Skrumi для Zed IDE записаны в $ZED_SETTINGS"
  fi
fi

# Скрипт полного удаления
{
  printf '#!/usr/bin/env bash\nset -e\nLIBDIR=%q\nBINDIR=%q\n' "$LIBDIR" "$BINDIR"
  cat <<'EOF'
test -f "$LIBDIR/.skrumi-install" || exit 1
if [ ! -f "$LIBDIR/.skrumi-files.sha256" ] ||
   [ -n "$(find "$LIBDIR" -type l -print -quit)" ] ||
   ! (cd "$LIBDIR" && sha256sum --quiet -c .skrumi-files.sha256) ||
   [ "$(find "$LIBDIR" -type f ! -name .skrumi-files.sha256 | wc -l)" -ne "$(wc -l < "$LIBDIR/.skrumi-files.sha256")" ]; then
  echo "Installation contains modified or additional files; nothing was removed." >&2; exit 1
fi
for command in skrumi skrumi-lsp foxlang foxlang-lsp; do
  if grep -Fq "SKRUMI_HOME=" "$BINDIR/$command" 2>/dev/null &&
     cmp -s "$BINDIR/$command" "$LIBDIR/launchers/$command"; then
    rm -f "$BINDIR/$command"
  fi
done
rm -rf "$LIBDIR"
echo "Skrumi removed. Editor settings and extensions were preserved."
EOF
} > "$LIBDIR/uninstall.sh"
mkdir -p "$LIBDIR/launchers"
for command in skrumi skrumi-lsp foxlang foxlang-lsp; do
  if [ -f "$BINDIR/$command" ]; then cp "$BINDIR/$command" "$LIBDIR/launchers/$command"; fi
done
chmod +x "$LIBDIR/uninstall.sh"

echo ""
echo "Skrumi $VERSION успешно установлен."
echo "Команда CLI:  $BINDIR/skrumi"
if [ -f "$BINDIR/skrumi-lsp" ]; then
  echo "Сервер LSP:   $BINDIR/skrumi-lsp"
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

# Refuse future upgrades/uninstalls if a user adds or modifies installation files.
(cd "$LIBDIR" && find . -type f ! -name .skrumi-files.sha256 -print0 | sort -z | xargs -0 sha256sum > .skrumi-files.sha256)
