#!/usr/bin/env bash
# Baixa os binários fechados de Wi-Fi/BLE/PHY do BL808 e confere o SHA-256.
# São os mesmos binários (e checksums) que o Zephyr usa no Sipeed M1s Dock
# (zephyrproject-rtos/hal_bouffalolab, zephyr/module.yml).
#
# Os binários têm licença própria da Bouffalo Lab e NÃO são versionados aqui:
# ficam em components/wireless/bl808_blobs/lib/ (ignorado pelo git).
#
# Uso: tools/bl808_wireless/fetch_blobs.sh
set -euo pipefail

SDK_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEST="$SDK_ROOT/components/wireless/bl808_blobs/lib"
BASE=https://raw.githubusercontent.com/pine64/pinevoice_smartspeaker_sdk/305dd9a535ce7751de8d77bd2134873469072df0/components/bl606p_wireless

# arquivo local | sha256 | caminho no SDK da Pine64
BLOBS="
libbl606p_firmware_prebuild.a|004c4636b3409585294b2219312c7bf99b361450b2b33798a7672535c34ae98b|bl606p_firmware/libs/e907fp/libbl606p_firmware_prebuild.a
libbl606p_phyrf.a|36b4241e868ff96378b61c890570f761d3cb2f56b1af98601bd2a4ac0dd3f05d|bl606p_phy/bl606p_phyrf/lib-gcc_10.2.0-toolchain_V2.6.1/libbl606p_phyrf.a
libbl606p_btblecontroller_prebuild.a|68ca8868cbd6be9a4198ab21de6cfc57a9a92e2bf431d779a6dea001f7a255df|btblecontroller/btblecontroller/libs/e907fp/libbl606p_btblecontroller_prebuild.a
"

mkdir -p "$DEST"
for entry in $BLOBS; do
  IFS='|' read -r name sum path <<<"$entry"
  out="$DEST/$name"
  if [ -f "$out" ] && echo "$sum  $out" | sha256sum -c --status -; then
    echo "ok (já existe): $name"
    continue
  fi
  echo "baixando: $name"
  curl -sSLf -o "$out.tmp" "$BASE/$path"
  if ! echo "$sum  $out.tmp" | sha256sum -c --status -; then
    rm -f "$out.tmp"
    echo "ERRO: checksum não confere para $name" >&2
    exit 1
  fi
  mv "$out.tmp" "$out"
  echo "ok: $name"
done
