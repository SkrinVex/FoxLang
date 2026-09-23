#!/usr/bin/env bash
set -euo pipefail

PREFIX="${FOXLANG_PREFIX:-$HOME/.local}"
LIBDIR="$PREFIX/share/foxlang"
BINDIR="$PREFIX/bin"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mkdir -p "$LIBDIR" "$BINDIR"
install -m755 "$HERE/foxlang.bin" "$LIBDIR/foxlang"
rm -rf "$LIBDIR/std"
cp -R "$HERE/std" "$LIBDIR/std"
cp "$HERE/VERSION" "$LIBDIR/VERSION"

cat > "$BINDIR/foxlang" <<EOF
#!/usr/bin/env bash
export FOXLANG_HOME="$LIBDIR"
exec "$LIBDIR/foxlang" "\$@"
EOF
chmod +x "$BINDIR/foxlang"

cat > "$LIBDIR/uninstall.sh" <<EOF
#!/usr/bin/env bash
set -e
rm -f "$BINDIR/foxlang"
rm -rf "$LIBDIR"
echo "FoxLang удалён."
EOF
chmod +x "$LIBDIR/uninstall.sh"

echo "FoxLang $(cat "$HERE/VERSION") установлен."
echo "Команда: $BINDIR/foxlang"
case ":$PATH:" in
  *":$BINDIR:"*) ;;
  *) echo "Добавь $BINDIR в PATH, если команда foxlang пока не находится." ;;
esac
