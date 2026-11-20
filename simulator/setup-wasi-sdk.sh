#!/bin/sh
# Downloads wasi-sdk (clang + wasm-ld for WebAssembly) into .wasi/, for building
# the Wokwi display chip. Run by `make wasi-sdk`. Safe to re-run: it does
# nothing if this version is already installed.
#
# .wasi/wasi-sdk is a symlink to the installed version, so the Makefile
# doesn't need to know the version or platform.
set -eu

VERSION=34.0
TAG=wasi-sdk-34

case "$(uname -s)-$(uname -m)" in
  Darwin-arm64)  PLATFORM=arm64-macos  SHA256=9c59398106b417f8f14913380fdf0097a8cc0ff4af9eb3ce0065a859e88d49e9 ;;
  Darwin-x86_64) PLATFORM=x86_64-macos SHA256=87d27fa8adc68dee59bfbf2e22a6d34ef717c34d6bf1d8af2a56fc929d9ce0eb ;;
  Linux-x86_64)  PLATFORM=x86_64-linux SHA256=b761e3a0721dbae9c09a0059e5fdb2bf917d1b4a8a7b430fb3b5aafb0984b2c4 ;;
  Linux-aarch64 | Linux-arm64)
                 PLATFORM=arm64-linux  SHA256=f7e243dff54d60bcc576e94d6166b69f410f2500ae4a9ceef34315be10e77971 ;;
  *) echo "No wasi-sdk $VERSION build for $(uname -s) $(uname -m)" >&2; exit 1 ;;
esac

ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST="$ROOT/.wasi"
NAME="wasi-sdk-$VERSION-$PLATFORM"

if [ -x "$DEST/$NAME/bin/clang" ]; then
  ln -sfn "$NAME" "$DEST/wasi-sdk"
  echo "wasi-sdk $VERSION already installed: .wasi/$NAME"
  exit 0
fi

mkdir -p "$DEST"
ARCHIVE="$DEST/$NAME.tar.gz.tmp"
URL="https://github.com/WebAssembly/wasi-sdk/releases/download/$TAG/$NAME.tar.gz"
echo "Downloading $URL (~180 MB)"
# Progress bar only in a terminal; it floods logs otherwise.
if [ -t 2 ]; then progress=--progress-bar; else progress=-sS; fi
curl -fL "$progress" -o "$ARCHIVE" "$URL"

if command -v sha256sum >/dev/null 2>&1; then
  actual=$(sha256sum "$ARCHIVE" | cut -d' ' -f1)
else
  actual=$(shasum -a 256 "$ARCHIVE" | cut -d' ' -f1)
fi
if [ "$actual" != "$SHA256" ]; then
  rm -f "$ARCHIVE"
  echo "Checksum mismatch for $NAME.tar.gz (got $actual, expected $SHA256)" >&2
  exit 1
fi

# Unpack next to the final folder and rename at the end, so an interrupted
# run never leaves a half-installed toolchain that looks complete.
rm -rf "$DEST/$NAME" "$DEST/$NAME.partial"
mkdir "$DEST/$NAME.partial"
tar -xzf "$ARCHIVE" -C "$DEST/$NAME.partial" --strip-components=1
rm -f "$ARCHIVE"
mv "$DEST/$NAME.partial" "$DEST/$NAME"
ln -sfn "$NAME" "$DEST/wasi-sdk"

echo "Installed .wasi/$NAME"
"$DEST/wasi-sdk/bin/clang" --version | head -n 1
