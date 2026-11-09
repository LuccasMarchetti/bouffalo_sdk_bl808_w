# BL808: rádio (Wi-Fi, BLE, Zigbee/Thread), gravação e depuração

Branch `wifi-bl808` deste fork de
[openbouffalo/bouffalo_sdk_bl808](https://github.com/openbouffalo/bouffalo_sdk_bl808).
Placa alvo: Sipeed M1s Dock. Documentação em português.

Análise de viabilidade detalhada: [analise-wifi.md](analise-wifi.md).
Gravação e depuração: [gravacao-e-depuracao.md](gravacao-e-depuracao.md).

## Situação por protocolo

| Protocolo | Binário disponível? | Situação neste fork |
|---|---|---|
| **Wi-Fi 4** | Sim (Pine64 / Zephyr) | Em andamento. Viável: a ABI bate com o M0 e faltam 3 símbolos. |
| **BLE** | Sim (`libbl606p_btblecontroller_prebuild.a`, também usado pelo Zephyr no BL808) | Próxima etapa, depois do Wi-Fi. |
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

Etapas:

1. [x] Download verificado dos binários.
2. [ ] Opção `CONFIG_WIFI4` para `CHIP=bl808 CPU_ID=m0` no CMake/Kconfig.
3. [ ] Porte de `bl808_port` (plataforma, IPC, RF) sobre FreeRTOS e LHAL.
4. [ ] Linker script do M0 com as seções do Wi-Fi.
5. [ ] Exemplo `examples/wifi/sta` para `bl808dk`/M1s Dock.
6. [ ] Teste na placa: scan, conexão WPA2, DHCP, ping.
7. [ ] BLE.
