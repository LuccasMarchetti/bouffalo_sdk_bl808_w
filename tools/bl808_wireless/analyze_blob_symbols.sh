#!/usr/bin/env bash
# Baixa os binários de Wi-Fi do BL808 (os mesmos usados pelo Zephyr) e lista
# os símbolos externos que eles exigem, marcando quais NÃO existem no SDK base.
#
# Uso: tools/bl808_wireless/analyze_blob_symbols.sh [caminho/do/sdk] [dir_trabalho]
# Requer: curl, sha256sum, llvm-nm (ou nm com suporte a RISC-V), grep.
set -euo pipefail

SDK="$(cd "${1:-$(dirname "$0")/../..}" && pwd)"
WORK="${2:-$(mktemp -d)}"
NM="${NM:-llvm-nm}"
BASE=https://raw.githubusercontent.com/pine64/pinevoice_smartspeaker_sdk/305dd9a535ce7751de8d77bd2134873469072df0/components/bl606p_wireless

mkdir -p "$WORK" && cd "$WORK"
curl -sSLf -o fw.a  "$BASE/bl606p_firmware/libs/e907fp/libbl606p_firmware_prebuild.a"
curl -sSLf -o phy.a "$BASE/bl606p_phy/bl606p_phyrf/lib-gcc_10.2.0-toolchain_V2.6.1/libbl606p_phyrf.a"

# Mesmos checksums registrados em zephyrproject-rtos/hal_bouffalolab (zephyr/module.yml)
sha256sum -c - <<'SUMS'
004c4636b3409585294b2219312c7bf99b361450b2b33798a7672535c34ae98b  fw.a
36b4241e868ff96378b61c890570f761d3cb2f56b1af98601bd2a4ac0dd3f05d  phy.a
SUMS

"$NM" --defined-only fw.a phy.a 2>/dev/null | awk 'NF==3{print $3}' | sort -u > definidos.txt
"$NM" -u fw.a phy.a 2>/dev/null | awk 'NF==2{print $2}' | sort -u | comm -23 - definidos.txt > exigidos.txt

echo "Símbolos externos exigidos pelos binários: $(wc -l < exigidos.txt)"
echo
echo "Exigidos e NÃO encontrados no SDK base (excluindo libc/libgcc e símbolos de linker):"
grep -vE '^(__|mem|str|_ld_)|printf$|^ato|^put' exigidos.txt | while read -r s; do
  grep -rqE "[ *]$s *(\(|=|\[)" "$SDK/components" "$SDK/drivers" "$SDK/bsp" --include=*.c || echo "  $s"
done
