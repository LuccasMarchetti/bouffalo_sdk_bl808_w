# wifi_ota_bl808

OTA do M0 do BL808 pelo Wi-Fi, com o boot2 e a tabela de partições do
`bl808dk` (FW A em `0x10000`, FW B em `0x290000`). Parte do `wifi_tcp`, que é
o exemplo de Wi-Fi que funciona no BL808, com o componente `wifi_ota`
(`CONFIG_BFLB_OTA`, `CONFIG_PARTITION`, `CONFIG_BFLB_MTD`).

```sh
make CHIP=bl808 BOARD=bl808dk CPU_ID=m0 OTA_VERSION=B   # a imagem nova: build_out/*.bin.ota
make CHIP=bl808 BOARD=bl808dk CPU_ID=m0 OTA_VERSION=A   # a que vai ser gravada
make flash ...
```
No PC: `cat wifi_ota_bl808_bl808_m0.bin.ota | nc -l 3365` (ou o
`bl809/bancada/ota_servidor.py`). No shell da placa:
```
wifi_sta_connect SSID SENHA
wifi_ota_start IP_DO_PC 3365      # baixa, confere o SHA-256, troca a partição e reinicia
wifi_ota_info                     # partição ativa e age
ota_versao                        # rótulo da imagem (OTA_VERSION)
```
Na bancada sem botões: compile com `CONFIG_BL808_BANCADA=y
CONFIG_BL808_BANCADA_KEEP_BOOT=y` (o watchdog é armado, mas o boot2 em 0 não
é apagado) e termine com `bancada_isp`, que apaga o cabeçalho e reinicia no
modo de gravação.

Testado no M1s Dock em 11/11/2026: 472 KB em 5,3 s, SHA-256 conferido, o
boot2 subiu a imagem B (relatório 7.6 em `bl809/BANCADA.md`).
