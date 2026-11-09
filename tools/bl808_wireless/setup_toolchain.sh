#!/usr/bin/env bash
# Baixa o compilador usado por este SDK no BL808: XuanTie GCC 2.6.1 (T-Head),
# publicado pela Bouffalo Lab em github.com/bouffalolab/toolchain (release
# "compact-old"). É a mesma versão com que os binários de Wi-Fi foram gerados.
#
# O GCC 14.3 do release "default" não serve para este fork: ele não aceita as
# extensões T-Head usadas no -march do BL808 e usa picolibc em vez de newlib.
#
# Uso: tools/bl808_wireless/setup_toolchain.sh [diretório_destino]
# Depois: export PATH="<destino>/xuantie-gcc-riscv-v2.6.1-linux-x86_64/bin:$PATH"
set -euo pipefail

DEST="${1:-$HOME/.local/bouffalo-toolchain}"
NAME=xuantie-gcc-riscv-v2.6.1-linux-x86_64
URL=https://github.com/bouffalolab/toolchain/releases/download/compact-old/$NAME.tar.xz
SHA256=83817bd7cab693fc249bb98869de5e475c1ede577f4a188869fed96c8dbb8e1a

if [ "$(uname -s)-$(uname -m)" != "Linux-x86_64" ]; then
  echo "Este script cobre só Linux x86_64. Para Windows, veja $URL (troque por $NAME/windows .7z)." >&2
  exit 1
fi

if [ -x "$DEST/$NAME/bin/riscv64-unknown-elf-gcc" ]; then
  echo "Já instalado em $DEST/$NAME"
else
  mkdir -p "$DEST"
  tmp="$DEST/$NAME.tar.xz"
  curl -fL -o "$tmp" "$URL"
  echo "$SHA256  $tmp" | sha256sum -c -
  tar -xJf "$tmp" -C "$DEST"
  rm -f "$tmp"
fi

"$DEST/$NAME/bin/riscv64-unknown-elf-gcc" --version | head -1
echo
echo "Adicione ao PATH:"
echo "  export PATH=\"$DEST/$NAME/bin:\$PATH\""
