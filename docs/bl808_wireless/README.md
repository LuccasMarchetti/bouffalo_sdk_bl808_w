# BL808: rádio (Wi-Fi, BLE, Zigbee/Thread), gravação e depuração

Branch `wifi-bl808` deste fork de
[openbouffalo/bouffalo_sdk_bl808](https://github.com/openbouffalo/bouffalo_sdk_bl808).
Placa alvo: Sipeed M1s Dock. Documentação em português.

Análise de viabilidade detalhada: [analise-wifi.md](analise-wifi.md).
Gravação e depuração: [gravacao-e-depuracao.md](gravacao-e-depuracao.md).
Pendências e próximos passos: [pendencias.md](pendencias.md).
Vault do Obsidian e grafo do código (graphify): repositório
[luccasmarchetti/bl809](https://github.com/LuccasMarchetti/bl809/tree/ccr-2d4283a6-izjhvt).

## Situação por protocolo

| Protocolo | Binário disponível? | Situação neste fork |
|---|---|---|
| **Wi-Fi 4** | Sim (Pine64 / Zephyr) | Compila e linka no M0. Falta testar na placa. |
| **BLE** | Sim (`libbl606p_btblecontroller_prebuild.a`, também usado pelo Zephyr no BL808) | Compila e linka no M0, sozinho ou junto com o Wi-Fi. Falta testar na placa. |
| **Bluetooth Clássico (BR/EDR)** | O binário do BLE tem o código | Não ativado. |
| **Zigbee / Thread (802.15.4)** | **Não** | Bloqueado. Ver abaixo. |

### Zigbee e Thread

O BL808 tem rádio 802.15.4 no hardware (está no datasheet), mas **não há binário
público da camada MAC 802.15.4 (`lmac154`) para o BL808** em nenhum dos SDKs
verificados:

- o SDK base tem `liblmac154_bl616.a`, só para BL616/BL618, e o OpenThread daqui
  depende dele;
- não há nada de 802.15.4 no `bl_iot_sdk` (branch `bl808`), no SDK da Sipeed nem
  no SDK da Pine64;
- os três binários do BL808 (Wi-Fi, PHY e BLE) não têm nenhum símbolo de
  802.15.4 (verificado com `llvm-nm`);
- um projeto da comunidade relata que o `lmac154` do BL808 só sairia sob NDA.

Usar o `liblmac154_bl616.a` no BL808 não é uma opção realista: ele foi feito
para outro chip, com outros registradores e outro PHY.

Caminhos possíveis:

1. pedir o `lmac154` do BL808 à Bouffalo Lab (suporte ou fórum);
2. acompanhar o Zephyr e a OpenBouffalo, caso alguém publique;
3. usar um rádio 802.15.4 externo (por exemplo, um BL702 ou BL616 como
   coprocessador, via UART/SPI), com o OpenThread RCP.

A parte de alto nível (OpenThread, Zigbee) não é o problema: o SDK já tem.
O que falta é só o driver do rádio para este chip.

## Binários fechados

Não são versionados (licença da Bouffalo). Para baixá-los com verificação de
SHA-256:

```sh
tools/bl808_wireless/fetch_blobs.sh
```

Eles ficam em `components/wireless/bl808_blobs/lib/`, ignorado pelo git.

## Plano do Wi-Fi

Referência principal: o porte do Zephyr (Apache-2.0, mesma licença do SDK),
funcionando no M1s Dock.

| Zephyr | O que faz | Destino neste SDK |
|---|---|---|
| `soc/bflb/bl808/wifi/bflb_wifi_platform.c` | clock do MAC, potência de TX, tarefa do firmware, símbolos que faltam, contornos de bugs do binário | `components/wireless/wifi4/bl808_port/` |
| `soc/bflb/bl808/wifi/bflb_wifi_ipc.c` | caminho de TX/RX adaptado à memória compartilhada do BL808 | idem, integrado ao `bl60x_wifi_driver` |
| `soc/bflb/bl808/wifi/bflb_wifi_sections.ld` | seções `.bl_static_cfg_entry`, `.fw.SHRAM`, `.fw.SHRAMIPC` | linker script do M0 do BL808 |
| `soc/bflb/bl808/bflb_rf.c` | calibração de RF e cristal | idem |
| `dts/riscv/bflb/bl808_em_32kb_wifi.dtsi` | mapa de memória: `WIFI_RAM` em `0x2203b000` (84 KB) | linker script do M0 |

Pontos de atenção levantados no Zephyr:

- **Contornos de bugs do binário:** o Zephyr intercepta (`-Wl,--wrap`) as
  funções `rc_update_counters`, `sta_mgmt_unregister`, `rfc_init`,
  `ipc_emb_wait`/`ipc_emb_notify` e `bl_sleep_schedule`. Os dois primeiros
  evitam *asserts* fatais dentro do binário.
- **Memória:** a `WIFI_RAM` não é zerada na inicialização, então precisa de
  `memset` antes de iniciar o firmware.
- **Interrupção E2A:** o handler do binário não confirma (ACK) a interrupção;
  o host precisa fazer isso, ou ela dispara sem parar.
- **TX:** o binário exige `status_addr` válido em todo descritor de TX.
- **Compilação:** o binário precisa de `-fcommon`.

## Como compilar (Wi-Fi no M0)

```sh
tools/bl808_wireless/setup_toolchain.sh            # XuanTie GCC 2.6.1 (uma vez)
export PATH="$HOME/.local/bouffalo-toolchain/xuantie-gcc-riscv-v2.6.1-linux-x86_64/bin:$PATH"
tools/bl808_wireless/fetch_blobs.sh                # binários de Wi-Fi/BLE/PHY (uma vez)

cd examples/wifi/sta/wifi_tcp
make CHIP=bl808 BOARD=bl808dk CPU_ID=m0
```

O GCC 14.3 (release `default` da Bouffalo) **não** serve para este fork: ele
não aceita as extensões T-Head no `-march` do BL808 e usa picolibc em vez de
newlib. O `setup_toolchain.sh` baixa o XuanTie 2.6.1 do release `compact-old`,
com verificação de SHA-256.

Cristal do M1s Dock: o Zephyr usa o código de capacitor 50 (erro abaixo de
1 ppm, contra cerca de 57 ppm sem ajuste). Para aplicar aqui:
`make CHIP=bl808 BOARD=bl808dk CPU_ID=m0 CONFIG_BL808_WIFI_XTAL_CAPCODE=50`.
O padrão, 0, mantém o valor da eFuse.

## O que foi feito

| Arquivo | Mudança |
|---|---|
| `examples/wifi/defconfig` | BL808 usa `CONFIG_WIFI4`. |
| `components/wireless/wifi4/firmware/CMakeLists.txt` | No BL808, liga o binário do BL808 (exige `CPU_ID=m0`), `-fcommon`, `CFG_CHIP_BL808`. |
| `components/wireless/wifi4/manager/CMakeLists.txt` | `CFG_TXDESC=4`, que precisa bater com o layout de memória do binário. |
| `components/wireless/wifi4/bl808_port/` (novo) | Símbolos que o binário de Wi-Fi pede e contornos de bugs dele (`--wrap` de `rc_update_counters` e `sta_mgmt_unregister`). Adaptado do Zephyr. |
| `components/wireless/wifi4/wifi4_adapter/` | Headers do BL808. IRQs `WIFI_IRQn` (70) e `WIFI_IPC_PUBLIC_IRQn` (79), iguais às do Zephyr. MAC lido da eFuse (antes devolvia zeros). |
| `drivers/soc/bl808/phyrf/` (novo) | Liga o binário de PHY/RF. `rfparam_init()` comum ao Wi-Fi e ao BLE: clock do PHY, ajuste do cristal e, conforme o que está linkado, RAM/clock/potência do Wi-Fi e `wl_init()` do BLE. `--wrap` de `rfc_init` (primeira calibração sem interrupções). `wl_api.h` do BL808 (Bouffalo, Apache-2.0, via HAL do Zephyr). |
| `bsp/board/bl808dk/bl808_flash_m0.ld` | Tabela `.wifi.cfg.entry`. Região `ram_wifi` de 84 KB no topo da WRAM (`0x22043000`), só quando o Wi-Fi é linkado. Correção da origem, que vinha do BL616 (`0x23038000`). Heap do lwIP na RAM normal. |

O driver de Wi-Fi do SDK (`bl60x_wifi_driver`) **já tinha** os caminhos do
BL808 (TX com cópia para buffers compartilhados, endereço dos registradores
em `0x24000000`). Eles só nunca tinham sido ativados.

Conferido no ELF gerado: a RAM do Wi-Fi fica em `0x22043000`–`0x22057e10`, e o
heap termina em `0x62043000` (alias com cache do mesmo endereço), sem
sobreposição. As chamadas do binário passam pelos `__wrap_*`.

Sem Wi-Fi, o M0 continua igual: região do Wi-Fi vazia e heap com os 160 KB de
WRAM. O `helloworld` compila no M0 e no D0.

## BLE

| Arquivo | Mudança |
|---|---|
| `components/wireless/bluetooth/CMakeLists.txt` | No BL808, liga o binário do controlador BLE do BL808 (só `CPU_ID=m0`). |
| `components/wireless/bluetooth/btblecontroller/bl808_port/` (novo) | API do AliOS (`aos_*`, filas e tarefas) sobre FreeRTOS, API antiga de interrupções (`bl_irq_*`), MAC da eFuse, e os nomes que diferem do host do SDK: `btble_controller_deinit` e `bt_onchiphci_handle_rx_acl` (no binário, `bt_onchiphci_hanlde_rx_acl`, com erro de grafia). Adaptado do Zephyr. |
| `drivers/soc/bl808/std/startup/m0/system_bl808.c` | Seleciona a Exchange Memory (EM) do tamanho que o linker reservou: 32 KB com BLE. Antes ficava sempre em 0 KB, e o controlador corromperia o próprio heap. |
| `bsp/board/bl808dk/board.c` | Com `CONFIG_PSRAM=y`, registra a PSRAM como heap do M0, limitada ao tamanho detectado na eFuse (32 ou 64 MB). |
| `examples/btble/{peripheral,central,btble_cli}/main.c` | Chamam `rfparam_init()` também no BL808. |
| `examples/wifi/coex/wifi_ble/` | Funciona no BL808: Wi-Fi + BLE, sem BR/EDR, com PSRAM. |

Mapa de memória do M0, conferido nos ELFs gerados:

| Imagem | RAM do Wi-Fi | EM do BLE | Fim do heap interno |
|---|---|---|---|
| Só Wi-Fi (`wifi_tcp`) | `0x22043000`, 84 KB | — | `0x62043000` |
| Só BLE (`peripheral`) | — | `0x22050000`, 32 KB | `0x62050000` |
| Wi-Fi + BLE (`wifi_ble`) | `0x2203b000`, 84 KB (o mesmo endereço do Zephyr) | `0x22050000`, 32 KB | `0x6203b000` (cerca de 14 KB livres, por isso a PSRAM) |

Com a PSRAM registrada, o `kmalloc()` tenta a PSRAM **antes** da RAM interna,
como no BL616. Isso dá mais memória, mas é mais lento.

`CONFIG_BTBLECONTROLLER_LIB`, no BL808, só escolhe os papéis BLE ativados no
host (central, periférico, observador...). O controlador é sempre o binário
do BL808.

Para testar:

```sh
cd examples/btble/peripheral       # ou central, btble_cli
make CHIP=bl808 BOARD=bl808dk CPU_ID=m0
make flash CHIP=bl808 CPU_ID=m0 COMX=/dev/ttyUSB1
```

O `peripheral` anuncia o dispositivo. Use um app como o nRF Connect no
celular para ver o anúncio e conectar.

## Etapas

1. [x] Download verificado dos binários.
2. [x] `CONFIG_WIFI4` para `CHIP=bl808 CPU_ID=m0`.
3. [x] Porte da plataforma (`bl808_port`, hosal, IRQs, RF).
4. [x] Linker script do M0 com as seções do Wi-Fi.
5. [x] `examples/wifi/sta/wifi_tcp` compila e linka para o BL808 M0.
6. [ ] **Teste na placa:** boot, `wifi_scan`, conexão WPA2, DHCP, ping. Ainda
   **não testado no hardware.**
7. [x] BLE compila e linka (sozinho e com Wi-Fi). [ ] Teste na placa.
8. [x] Gravar M0 e D0 de uma vez (`tools/bl808_wireless/flash.sh`). Depuração
   pelo VS Code (`tools/bl808_wireless/vscode/`). Ver
   [gravacao-e-depuracao.md](gravacao-e-depuracao.md).

Problema já existente no fork, sem relação com este trabalho: o exemplo de
Wi-Fi do **BL602** não compila (`drivers/lhal/src/bflb_common.c:357`).
