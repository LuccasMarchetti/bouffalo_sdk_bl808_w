# Pendências e próximos passos

Situação em 09/11/2026, branch `wifi-bl808` (commit `a18836d`).
Tudo abaixo **compila**, mas **nada foi testado no hardware**.

Legenda de prioridade: **P1** bloqueia o uso, **P2** importante, **P3** melhoria.

## 1. Testes na placa (P1)

Pré-requisitos, uma vez só:

```sh
tools/bl808_wireless/setup_toolchain.sh
export PATH="$HOME/.local/bouffalo-toolchain/xuantie-gcc-riscv-v2.6.1-linux-x86_64/bin:$PATH"
tools/bl808_wireless/fetch_blobs.sh
```

Serial: 2.000.000 baud. Gravação: segure BOOT, aperte e solte RST, solte BOOT.

| # | Teste | Como | Resultado esperado | Status |
|---|---|---|---|---|
| T1 | Boot básico do M0 | `examples/helloworld`, `CPU_ID=m0` | log "board init done" | ☐ |
| T2 | Portas seriais | ver qual `/dev/ttyUSBx` é M0 e qual é D0 | anotar em [gravacao-e-depuracao.md](gravacao-e-depuracao.md) | ☐ |
| T3 | Wi-Fi: boot | `examples/wifi/sta/wifi_tcp` | sem travamento; MAC diferente de `00:00:00:00:00:00` | ☐ |
| T4 | Wi-Fi: scan | `wifi_scan` | lista de redes | ☐ |
| T5 | Wi-Fi: conexão | `wifi_sta_connect <ssid> <senha>` | evento `GOT_IP` | ☐ |
| T6 | Wi-Fi: tráfego | ping / `iperf` | sem queda; anotar a vazão | ☐ |
| T7 | Wi-Fi: cristal | repetir T5 com `CONFIG_BL808_WIFI_XTAL_CAPCODE=50` | conexão mais estável ou igual | ☐ |
| T8 | BLE: anúncio | `examples/btble/peripheral` | aparece no nRF Connect | ☐ |
| T9 | BLE: conexão | conectar pelo celular | conecta, troca dados | ☐ |
| T10 | BLE central | `examples/btble/central` | encontra e conecta num periférico | ☐ |
| T11 | Wi-Fi + BLE | `examples/wifi/coex/wifi_ble` | os dois funcionando ao mesmo tempo; log mostra o heap da PSRAM | ☐ |
| T12 | Dois núcleos | `tools/bl808_wireless/flash.sh --m0 ... --d0 examples/bl808_triplecore/helloworld_d0` | log do M0 e do D0 | ☐ |
| T13 | JTAG M0 / D0 | `CONFIG_BL808_JTAG=y` + OpenOCD | GDB para no programa | ☐ |

Em qualquer falha: guardar o log completo da serial desde o boot.

## 2. Riscos conhecidos, a verificar nos testes (P1/P2)

| Risco | Onde | O que observar | Se falhar |
|---|---|---|---|
| Prioridade das tarefas do BLE (escolha minha; o Zephyr ignora o valor) | `components/wireless/bluetooth/btblecontroller/bl808_port/bl808_btble_port.c`, `aos_task_new_ext` | BLE trava ou não anuncia | testar outras prioridades, ou fixar uma abaixo do timer do FreeRTOS |
| `wl_init()` em modo `ALL` com Wi-Fi + BLE (não testado nem no Zephyr) | `drivers/soc/bl808/phyrf/bl808_rf.c` | Wi-Fi ou BLE para no `wifi_ble` | testar o modo `BZ`, ou não chamar `wl_init()` quando o Wi-Fi está presente |
| PSRAM antes da RAM interna no `kmalloc` | `bsp/board/bl808dk/board.c` | Wi-Fi/BLE lentos ou com timeout no `wifi_ble` | criar heap dedicado, ou mudar a ordem de busca no `mm` |
| Offsets internos do binário nos contornos de bugs (vindos do Zephyr) | `components/wireless/wifi4/bl808_port/bl808_wifi_port.c` | assert ao conectar ou desconectar | conferir os offsets de `sta_info_tab` com o disassembly |
| Callbacks de interrupção do BLE chamados com argumentos que eles não esperam | `bl_irq_register` | crash no BLE | criar um trampolim sem argumentos |
| Pinos de JTAG GPIO4–7 no M1s Dock | `board_jtag_gpio_init()` | pinos ocupados ou sem acesso no conector | mudar `CONFIG_BL808_JTAG_PIN_BASE` |

## 3. Desenvolvimento

### P2

- [ ] **Layout da flash:** o M0 inicia o D0 em `0x100000`, mas a tabela de
  partições põe o firmware do M0 em `0x10000`–`0x290000` e reserva `D0FW` em
  `0x4F0000`. Uma OTA do M0 apagaria o D0. Opções: ler o endereço da partição
  `D0FW` no `board_init()`, ou mudar `CONFIG_D0_FLASH_ADDR` e o `flash.sh`.
- [ ] **Placa `m1s_dock`:** um `BOARD=m1s_dock` com o ajuste do cristal (50),
  os pinos da placa e o JTAG, em vez de reaproveitar o `bl808dk`.
- [ ] **`flash.sh` pela serial:** validar no hardware (até agora só `--pack`).
- [ ] **Exemplo próprio para o M1s:** Wi-Fi + BLE simples, com instruções no
  README do exemplo.
- [ ] **Exemplo de Wi-Fi do BL602 quebrado** neste fork
  (`drivers/lhal/src/bflb_common.c:357`). Problema anterior a este trabalho;
  corrigir se for afetar o envio para a OpenBouffalo.

### P3

- [ ] **Bluetooth Clássico (BR/EDR):** o binário do BLE tem o código
  (`lm_*`, `ld_*`). Avaliar A2DP/SPP/HFP.
- [ ] **Modo AP do Wi-Fi** (`wifi_ap_start`) e **economia de energia**
  (`wifi_sta_ps_on`): nada foi verificado.
- [ ] **Logs do binário:** `bl_printf` é descartado. Criar uma opção de
  depuração que mostre esses logs (fora da calibração de RF, como no Zephyr).
- [ ] **GCC 14.3:** o compilador XuanTie 2.6.1 já não está no release
  principal da Bouffalo. Migrar o fork para GCC 14 + picolibc, como fez o SDK
  oficial (C library, `-march` sem extensões T-Head).
- [ ] **Depurar os dois núcleos ao mesmo tempo:** dois adaptadores em dois
  conjuntos de pinos.
- [ ] **Enviar para a OpenBouffalo** (pull request para
  `openbouffalo/bouffalo_sdk_bl808`), depois dos testes.

### Bloqueado

- [ ] **Zigbee / Thread (802.15.4):** não existe binário público do `lmac154`
  para o BL808. Caminhos: pedir à Bouffalo, acompanhar Zephyr/OpenBouffalo, ou
  usar um rádio externo (BL702/BL616 como RCP). Detalhes no
  [README](README.md#zigbee-e-thread).

## 4. Feito

- [x] Pesquisa do que já existia: Zephyr, RT-Thread, NuttX, BL IoT SDK,
  Sipeed, Pine64 ([analise-wifi.md](analise-wifi.md)).
- [x] Fork do SDK da OpenBouffalo com `upstream` configurado.
- [x] Compilador XuanTie 2.6.1 com checksum (`setup_toolchain.sh`).
- [x] Download dos binários com checksum (`fetch_blobs.sh`).
- [x] Wi-Fi 4 no M0: compila e linka.
- [x] BLE no M0, sozinho e com Wi-Fi: compila e linka.
- [x] EM do BLE selecionada conforme o linker (antes, sempre 0 KB).
- [x] PSRAM como heap do M0 (`CONFIG_PSRAM=y`).
- [x] Gravação de M0 + D0 numa vez (`flash.sh`).
- [x] JTAG por núcleo (`CONFIG_BL808_JTAG=y`) e configurações do VS Code.
