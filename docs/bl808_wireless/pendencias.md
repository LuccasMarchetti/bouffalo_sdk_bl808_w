# Pendências e próximos passos

Situação em 10/11/2026, branch `wifi-bl808`.
O cronograma da bancada (SDK + Zephyr) está em `BANCADA.md` no repositório `bl809`.
Testado na placa (Sipeed M1s Dock) em 10/11/2026: T1–T6 e T8–T15 passam; T7 não se aplica. Logs e detalhes no `BANCADA.md` do `bl809`.

Legenda de prioridade: **P1** bloqueia o uso, **P2** importante, **P3** melhoria.

## 1. Testes na placa (P1)

Pré-requisitos, uma vez só:

```sh
tools/bl808_wireless/setup_toolchain.sh
export PATH="$HOME/.local/bouffalo-toolchain/xuantie-gcc-riscv-v2.6.1-linux-x86_64/bin:$PATH"
tools/bl808_wireless/fetch_blobs.sh
```

Serial: 2.000.000 baud. Gravação: segure BOOT, aperte e solte RST, solte BOOT (ou, na bancada, `CONFIG_BL808_BANCADA=y` e `bancada.py`).

| # | Teste | Como | Resultado esperado | Status |
|---|---|---|---|---|
| T1 | Boot básico do M0 | `examples/helloworld`, `CPU_ID=m0` | log "board init done" | ☑ |
| T2 | Portas seriais | ver qual `/dev/ttyUSBx` é M0 e qual é D0 | anotar em [gravacao-e-depuracao.md](gravacao-e-depuracao.md) | ☑ |
| T3 | Wi-Fi: boot | `examples/wifi/sta/wifi_tcp` | sem travamento; MAC diferente de `00:00:00:00:00:00` | ☑ |
| T4 | Wi-Fi: scan | `wifi_scan` | lista de redes | ☑ |
| T5 | Wi-Fi: conexão | `wifi_sta_connect <ssid> <senha>` | evento `GOT_IP` | ☑ |
| T6 | Wi-Fi: tráfego | ping / `iperf` | sem queda; anotar a vazão | ☑ |
| T7 | Wi-Fi: cristal | repetir T5 com `CONFIG_BL808_WIFI_XTAL_CAPCODE=50` | conexão mais estável ou igual | n/a |
| T8 | BLE: anúncio | `examples/btble/peripheral` | aparece no nRF Connect | ☑ |
| T9 | BLE: conexão | conectar pelo celular | conecta, troca dados | ☑ |
| T10 | BLE central | `examples/btble/central` | encontra e conecta num periférico | ☑ |
| T11 | Wi-Fi + BLE | `examples/wifi/coex/wifi_ble` | os dois funcionando ao mesmo tempo; log mostra o heap da PSRAM | ☑ |
| T12 | Dois núcleos | `tools/bl808_wireless/flash.sh --m0 ... --d0 examples/bl808_triplecore/helloworld_d0` | log do M0 e do D0 | ☑ |
| T13 | JTAG M0 / D0 | `CONFIG_BL808_JTAG=y` + OpenOCD | GDB para no programa | ☑ |
| T14 | FreeRTOS no D0 | `examples/freertos`, `CPU_ID=d0` (antes deste fork nem compilava: `portBYTE_ALIGNMENT`) | tarefas trocando mensagens no console do D0 | ☑ |
| T15 | Vetor no FreeRTOS do D0 | `examples/freertos_vector`, `CPU_ID=d0` | `VECTOR TEST PASS` após ~10 s | ☑ |

Em qualquer falha: guardar o log completo da serial desde o boot.

Resultados (10/11/2026):
- **T3** MAC `B4:0E:CF:35:D7:6B`, sem travamento. **T4** lista de redes (RSSI, canal, WPA2).
- **T5** `GOT_IP` depois de três correções: AES do supplicant pela memória sem cache
  (o desembrulho da GTK falhava e corrompia o heap), `PBUF_POOL_SIZE` maior que 0
  no BL808 (o RX copia para o pool; com 0 nenhum quadro de dados chegava) e, no
  `wifi_ble`, a criação do mutex do SEC (`bflb_sec_mutex_init`).
- **T6** TCP: placa → PC 10,8 Mbit/s, PC → placa 21,4 Mbit/s (`wifi_tcp`, 10 s).
- **T7** não se aplica: não existe `CONFIG_BL808_WIFI_XTAL_CAPCODE`, e o chip já
  usa capcode 50 (`0x32`); o AP aparece a −4 ppm.
- **T8/T9** com o PC no lugar do celular (`bluetoothctl`): anúncio, conexão, MTU 247,
  descoberta GATT, leitura, escrita e notificação. Correções: pilha do `main` de
  4 KB (estourava e corrompia o `impure_data`) e 4 buffers de TX do host (com 2, a
  primeira troca ATT travava).
- **T10** a placa conecta no PC (que anuncia como `BTBLE-DEV`), descobre os serviços
  e escreve. Com o intervalo de anúncio padrão do bluetoothd, o tempo de 3 s do
  exemplo para criar a conexão às vezes acaba antes.
- **T11** Wi-Fi com `iperf` a 8,9 Mbit/s por 40 s e, no meio, conexão BLE com
  descoberta GATT, sem queda de vazão. lwIP na PSRAM e mbedtls em software (o SEC
  não lê a PSRAM: o SHA em hardware deu PSK errada).

## 2. Riscos conhecidos, a verificar nos testes (P1/P2)

| Risco | Onde | O que observar | Se falhar |
|---|---|---|---|
| Prioridade das tarefas do BLE (escolha minha; o Zephyr ignora o valor) | `components/wireless/bluetooth/btblecontroller/bl808_port/bl808_btble_port.c`, `aos_task_new_ext` | BLE trava ou não anuncia | testar outras prioridades, ou fixar uma abaixo do timer do FreeRTOS |
| `wl_init()` em modo `ALL` com Wi-Fi + BLE (não testado nem no Zephyr) | `drivers/soc/bl808/phyrf/bl808_rf.c` | Wi-Fi ou BLE para no `wifi_ble` | testar o modo `BZ`, ou não chamar `wl_init()` quando o Wi-Fi está presente |
| PSRAM antes da RAM interna no `kmalloc` | `bsp/board/bl808dk/board.c` | Wi-Fi/BLE lentos ou com timeout no `wifi_ble` | criar heap dedicado, ou mudar a ordem de busca no `mm` |
| Offsets internos do binário nos contornos de bugs (vindos do Zephyr) | `components/wireless/wifi4/bl808_port/bl808_wifi_port.c` | assert ao conectar ou desconectar | conferir os offsets de `sta_info_tab` com o disassembly |
| Callbacks de interrupção do BLE chamados com argumentos que eles não esperam | `bl_irq_register` | crash no BLE | criar um trampolim sem argumentos |
| Pinos de JTAG GPIO4–7 no M1s Dock | `board_jtag_gpio_init()` | pinos ocupados ou sem acesso no conector | mudar `CONFIG_BL808_JTAG_PIN_BASE` |

Na placa (10/11/2026): prioridade das tarefas do BLE, `wl_init()` em `ALL`
(Wi-Fi + BLE juntos), offsets do `sta_info_tab` e callbacks de interrupção do BLE
não deram problema. A PSRAM no heap deu: o SEC (SHA/AES em hardware) não lê
buffers na PSRAM (ver P2 abaixo). No M1s Dock o JTAG fica no slot micro SD:
`CONFIG_BL808_JTAG_PIN_BASE=0` (GPIO0–3).

## 3. Desenvolvimento

### P1

- [x] **`PBUF_POOL_SIZE 0` nos exemplos de Wi-Fi.** O RX do BL808 copia cada
  quadro para o pool de pbufs (`bl_utils.c`); com o pool vazio nenhum quadro de
  dados chega. Corrigido em todos os `lwipopts_user.h` de `examples/wifi` menos
  `wifi_eth_bridge` (outra configuração). Rodados na placa: `wifi_tcp` e
  `wifi_ble`; os outros só compilados (`wifi_udp`, `wifi_http`, `wifi_mqtt_pub`).

### P2
- **LP: o IPC chega na IRQ +3, não na +35.** Medido na placa em 13/11/2026
  (`bl809/bancada/lp_clic_probe`): o CLIC do LP tem 48 linhas (`CLICINFO`),
  e com o IPC do LP desmascarado a linha pendente no LP é a 19 (16 + 3). O
  `IPC_LP_IRQn = IRQ_NUM_BASE + 35` do `bl808.h` não existe no LP
  (`CPU_LP_MAX_IRQ_NUM` = 16 + 32). Um código do LP que use `IPC_LP_IRQn`
  nunca recebe o IPC (o `IPC_LP_Init()` do `bl808_ipc.c` faz isso).
- [x] **Tabela de dispositivos (13/11/2026):** entraram I2C1, I2C2, I2C3, SPI1,
  DMA1 (4 canais) e I2S0, o clock por instância e o bit de mestre do SPI1;
  `timer0`/`timer1`/`watchdog0` apontam para o bloco de cada núcleo (antes o
  `timer1` do M0 era o bloco do MM com a IRQ do canal 1 do TIMER0). Testado
  na placa, menos o I2S0 (sem codec). **Faltam** IPC, AUADC/AUDAC, DMA2D e
  MJDEC: o SDK nem compila esses drivers para o BL808 (port de driver).
- [ ] **I2C: NACK vira timeout.** `bflb_i2c_write_bytes()`/`read_bytes()`
  esperam `busy || !end || nak` e devolvem `-ETIMEDOUT` depois de 100 ms num
  NACK; o certo é `-EIO` na hora (o Zephyr já faz). Uma varredura de 112
  endereços leva 11 s.

- [ ] **SEC e PSRAM:** com o heap na PSRAM (`CONFIG_PSRAM=y`), os ports de mbedtls
  em hardware (`components/crypto/mbedtls/port/hw_acc`) recebem buffers na PSRAM,
  que o SEC não lê (`mbedtls_sha1_once_padded` até troca o endereço para o alias
  `0x2xxxxxxx`). No `wifi_ble` a PSK saiu errada. Hoje o exemplo usa mbedtls em
  software; o certo é passar por um buffer na RAM interna, como fiz no AES do
  supplicant (`blcrypto_sec_pka.c`).
- [ ] **Partição `PSM` ausente:** a tabela de partições do BL808 não tem `PSM`, e o
  littlefs/easyflash falham (`no valid PSM partition`). O BLE não guarda as chaves
  de pareamento.
- [ ] **ACL adiado no `bl_hci_wrapper.c`:** quando falta buffer de RX, o pacote ACL
  é copiado e tratado depois, mas no BL808 o `param` aponta para a memória de
  troca do controlador, que é liberada ao fim do callback (aviso do Zephyr). Não
  vi falhar (com buffer livre o caminho é imediato), mas é um risco sob carga.
- [ ] **Upstream:** o travamento do ATT com 2 buffers de TX e o mutex do SEC que
  faltava no `bl808dk` provavelmente valem para a OpenBouffalo também.

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

- [ ] **Sysmap da EM:** o bootrom deixa `0x00000000–0x40000000` como
  *strongly ordered*, incluindo a janela da EM do BLE (`0x28000000`). O Zephyr diz
  que a ponte descarta escritas assim; nos testes do SDK não fez diferença (testado
  com e sem). Rever se aparecer corrupção no BLE.
- [ ] **Heap do `wifi_tcp`:** sobram ~24 KB de heap interno com o pool de pbufs na
  RAM. Para mais conexões, usar a PSRAM como no `wifi_ble`.
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
- [x] FreeRTOS do D0: salva a unidade vetorial na troca de contexto e volta a
  compilar (`portBYTE_ALIGNMENT`). Teste: `examples/freertos_vector`.
- [x] Testes T1–T15 na placa (10/11/2026), com as correções: AES do supplicant sem
  cache, pool de pbufs, pilha de 4 KB no M0, 4 buffers de TX do BLE, mutex do SEC.
