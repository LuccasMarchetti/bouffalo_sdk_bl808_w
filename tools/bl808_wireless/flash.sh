#!/usr/bin/env bash
# Grava o firmware do M0 e/ou do D0 do BL808 de uma vez, pela UART, usando a
# ferramenta de linha de comando do SDK (BLFlashCommand), sem o Dev Cube.
#
# Uso:
#   tools/bl808_wireless/flash.sh --port /dev/ttyUSB1 \
#       --m0 examples/wifi/sta/wifi_tcp --d0 examples/bl808_triplecore/helloworld_d0
#
# Opções:
#   --m0 DIR      exemplo do M0 já compilado (make CHIP=bl808 BOARD=bl808dk CPU_ID=m0)
#   --d0 DIR      exemplo do D0 já compilado (make CHIP=bl808 BOARD=bl808dk CPU_ID=d0)
#   --port PORTA  porta serial (Linux: /dev/ttyUSBx ou /dev/ttyACMx; Windows: COMx)
#   --baud N      velocidade (padrão 2000000)
#   --erase-all   apaga a flash inteira antes de gravar
#   --pack        não grava: só monta a imagem completa da flash
#                 (whole_flash_data.bin), útil para conferir ou gravar depois
#
# Antes de gravar: segure BOOT, aperte e solte RST, solte BOOT.
#
# O M0 inicia o D0 a partir de CONFIG_D0_FLASH_ADDR (bsp/board/bl808dk/board.h,
# 0x100000). O script grava o D0 nesse endereço e confere se o firmware do M0
# cabe antes dele.
set -euo pipefail

SDK_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TOOL_DIR="$SDK_ROOT/tools/bflb_tools/bouffalo_flash_cube"
D0_ADDR=0x100000

M0_DIR="" D0_DIR="" PORT="" BAUD=2000000 ERASE=1 PACK=0
while [ $# -gt 0 ]; do
  case "$1" in
    --m0) M0_DIR="$2"; shift 2 ;;
    --d0) D0_DIR="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --baud) BAUD="$2"; shift 2 ;;
    --erase-all) ERASE=2; shift ;;
    --pack) PACK=1; shift ;;
    -h|--help) sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "opção desconhecida: $1" >&2; exit 2 ;;
  esac
done

[ -n "$M0_DIR$D0_DIR" ] || { echo "informe --m0 e/ou --d0" >&2; exit 2; }
[ "$PACK" = 1 ] || [ -n "$PORT" ] || { echo "informe --port (ou use --pack)" >&2; exit 2; }

case "$(uname -s)-$(uname -m)" in
  Linux-x86_64) TOOL="$TOOL_DIR/BLFlashCommand-ubuntu" ;;
  Linux-aarch64) TOOL="$TOOL_DIR/BLFlashCommand-ubuntu-aarch64" ;;
  Darwin-*) TOOL="$TOOL_DIR/BLFlashCommand-macos" ;;
  MINGW*|MSYS*|CYGWIN*) TOOL="$TOOL_DIR/BLFlashCommand.exe" ;;
  *) echo "sistema não suportado: $(uname -s)-$(uname -m)" >&2; exit 1 ;;
esac

abs() { (cd "$1" && pwd); }
one_file() { # padrão glob -> exatamente um arquivo
  local matches=( $1 )
  if [ ${#matches[@]} -ne 1 ] || [ ! -f "${matches[0]}" ]; then
    echo "esperava exatamente um arquivo para '$1', achei: ${matches[*]}" >&2
    exit 1
  fi
  echo "${matches[0]}"
}

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
INI="$WORK/flash_prog_cfg.ini"

cat > "$INI" <<EOF
[cfg]
# 0: no erase, 1: programmed section erase, 2: chip erase
erase = $ERASE
skip_mode = 0x0, 0x0
boot2_isp_mode = 0
EOF

if [ -n "$M0_DIR" ]; then
  M0_DIR="$(abs "$M0_DIR")"
  [ -f "$M0_DIR/flash_prog_cfg.ini" ] || { echo "sem flash_prog_cfg.ini em $M0_DIR" >&2; exit 1; }
  [ -d "$M0_DIR/build/build_out" ] || { echo "compile o M0 antes: $M0_DIR" >&2; exit 1; }
  # Copia as seções do M0 (boot2, partition, FW...), com caminhos absolutos.
  # Os nomes ficam como estão: "address = @partition" procura a partição
  # pelo nome da seção (FW, mfg...).
  awk -v dir="$M0_DIR" '
    /^\[cfg\]/ { skip = 1; next }
    /^\[/      { skip = 0; print; next }
    !skip      { gsub(/\.\/build\//, dir "/build/"); print }
  ' "$M0_DIR/flash_prog_cfg.ini" >> "$INI"

  m0_bin="$(one_file "$M0_DIR/build/build_out/*_bl808_m0.bin")"
  m0_addr="$(awk '/^\[FW\]/{f=1} f && /^address/{print $3; exit}' "$M0_DIR/flash_prog_cfg.ini")"
  [ "$m0_addr" = "@partition" ] && m0_addr=0x10000   # FW em partition_cfg_8M.toml
  m0_end=$(( m0_addr + $(stat -c %s "$m0_bin" 2>/dev/null || stat -f %z "$m0_bin") ))
  if [ -n "$D0_DIR" ] && [ "$m0_end" -gt $((D0_ADDR)) ]; then
    printf 'ERRO: o firmware do M0 (até 0x%x) invade a área do D0 (0x%x).\n' "$m0_end" "$D0_ADDR" >&2
    exit 1
  fi
fi

if [ -n "$D0_DIR" ]; then
  D0_DIR="$(abs "$D0_DIR")"
  d0_bin="$(one_file "$D0_DIR/build/build_out/*_bl808_d0.bin")"
  cat >> "$INI" <<EOF

[D0_IMG]
filedir = $d0_bin
address = $D0_ADDR
EOF
fi

if [ -n "$M0_DIR" ]; then CPU_ID=m0; else CPU_ID=d0; fi

echo "== configuração gerada =="
cat "$INI"
echo

cd "$TOOL_DIR"
if [ "$PACK" = 1 ]; then
  "$TOOL" --interface=uart --chipname=bl808 --cpu_id="$CPU_ID" --config="$INI" --build
  out="$TOOL_DIR/chips/bl808/img_create/whole_flash_data.bin"
  cp "$out" "$OLDPWD/whole_flash_data.bin"
  echo "Imagem completa: $OLDPWD/whole_flash_data.bin"
else
  "$TOOL" --interface=uart --baudrate="$BAUD" --port="$PORT" \
    --chipname=bl808 --cpu_id="$CPU_ID" --config="$INI"
  echo "Pronto. Aperte RST para rodar."
fi
