# Análise: Wi-Fi do BL808 no SDK oficial

Data da análise: 09/11/2026.

Objetivo: ter um SDK para o BL808 (placa Sipeed M1s Dock) com os periféricos do
SDK oficial da Bouffalo **e** o Wi-Fi que hoje só funciona no SDK da Sipeed.

## 1. O que já existe (para não refazer trabalho)

| Projeto | O que faz | Situação |
|---|---|---|
| [bouffalolab/bouffalo_sdk](https://github.com/bouffalolab/bouffalo_sdk) (oficial) | SDK principal | Na versão atual (commit `eba87edc`, nov/2026) **não há mais BL808**: só BL602, BL702, BL616, BL618. Wi-Fi 4 (`components/wireless/wifi4`) existe, mas só para BL602. |
| [openbouffalo/bouffalo_sdk_bl808](https://github.com/openbouffalo/bouffalo_sdk_bl808) | Fork do SDK oficial que **mantém o BL808** (M0 e D0) | Ativo (último commit em set/2026), base v2.3.16. Periféricos do BL808 presentes, mas a tabela marca WIFI4 como **não suportado** no BL808. |
| [Zephyr PR #121169](https://github.com/zephyrproject-rtos/zephyr/pull/121169) | Wi-Fi do BL808 no driver `wifi4` do Zephyr | **Mesclado** (out/2026). Testado no **M1s Dock**: scan, WPA2-PSK, DHCP, TCP, reconexão. BLE também funciona. |
| [flyingcys/wlan_bl808_rtthread](https://github.com/flyingcys/wlan_bl808_rtthread) | Wi-Fi do BL808 no RT-Thread | Porte do Wi-Fi do SDK da Sipeed. Parado desde nov/2023. Só Wi-Fi. |
| [sipeed/M1s_BL808_SDK](https://github.com/sipeed/M1s_BL808_SDK) | SDK da Sipeed | Parado desde fev/2023. Tem Wi-Fi, mas com binário antigo. |
| [bouffalolab/bl_iot_sdk](https://github.com/bouffalolab/bl_iot_sdk), branch `bl808` | SDK IoT da própria Bouffalo, base do SDK da Sipeed | Parado desde abr/2023 (commit `ccc9f70`). Tem Wi-Fi com um binário **diferente e mais novo** que o da Sipeed. Era usado pelo [blwnet_xram](https://github.com/bouffalolab/blwnet_xram) para dar Wi-Fi ao Linux no D0. |
| [apache/nuttx](https://github.com/apache/nuttx) | RTOS | O BL808 (placa Ox64, núcleo D0) tem GPIO, UART, I2C, SPI, ADC, timers e watchdog. **Não tem Wi-Fi.** O Wi-Fi do NuttX (`bl602_netdev.c`) existe só para o BL602. Conferido no commit `6c91f95` (nov/2026). |

Conclusão: ninguém trouxe o Wi-Fi do BL808 para o SDK oficial, mas o **Zephyr
já resolveu o problema difícil**: mostrou quais binários usar e como inicializar
o hardware no M1s Dock. Isso serve de referência direta.

## 2. Qual binário de Wi-Fi usar

Existem três versões do binário fechado do Wi-Fi do BL808 (checksums diferentes):

| Versão | Origem | Compatível com o código Wi-Fi do SDK oficial? |
|---|---|---|
| Sipeed (antiga) | `M1s_BL808_SDK/components/network/wifi/lib/libwifi.a` | **Não.** A estrutura de memória compartilhada (`ipc_shared.h`) é diferente: faltam campos como `vif_type` e `postpone_time`, e a fila de TX é outra. |
| BL IoT SDK | `bl_iot_sdk@bl808`: `libwifi.a` + `libbl606p_phyrf.a` | **Provavelmente sim.** O código aberto que acompanha esse binário já tem `vif_type` e `postpone_time`. Mas ninguém o usou fora do próprio SDK. |
| Pine64 | SDK da Pine64 [`pinevoice_smartspeaker_sdk@305dd9a`](https://github.com/pine64/pinevoice_smartspeaker_sdk/tree/305dd9a535ce7751de8d77bd2134873469072df0/components/bl606p_wireless): `libbl606p_firmware_prebuild.a` + `libbl606p_phyrf.a` | **Sim.** É a que o Zephyr usa no M1s Dock, com o mesmo layout de IPC do `wifi4` atual. |

**Decisão: usar os binários da Pine64**, com os mesmos checksums SHA-256 que o
Zephyr registra em `hal_bouffalolab/zephyr/module.yml`, porque já foram
validados na mesma placa. O do BL IoT SDK fica como plano B.

Comparação entre os dois candidatos: compartilham cerca de 6.950 símbolos
internos. Diferenças no que exigem de fora:

- só o do BL IoT SDK exige: `hal_get_temperature`, `hal_set_temperature`
  (que o SDK base não tem) e funções de libc para ler parâmetros de RF
  (`atof`, `atoi`, `strtol`, `strcmp`);
- só o da Pine64 exige: `bl_gpio_enable_output`, `bl_gpio_output_set`,
  `coex_timing_control_ctx` (que o SDK base não tem), `bl_os_clock_gettime_ms`,
  `bl_printf`, `puts`, `putchar`.

Ou seja, os dois exigem o mesmo tamanho de adaptação.

## 3. Análise de símbolos

Comando: `tools/bl808_wireless/analyze_blob_symbols.sh`.

- Os dois binários exigem **47 símbolos externos**. Cerca de metade é libc/libgcc
  (`memcpy`, `printf`, `__udivdi3`...).
- Quase todo o resto **já existe no SDK base**, no código aberto do `wifi4` feito
  para o BL602: `bl_main_event_handle`, `bl_rx_e2a_handler`,
  `ipc_host_disable_irq_e2a`, `tcpip_stack_input`, `g_bl_ops_funcs` (camada de
  sistema operacional), `wifi_hosal_*` (energia/RF), `bl_supplicant_init`,
  utilitários de CRC/TLV/listas.
- **Faltam só 3:**
  - `bl_gpio_enable_output`, `bl_gpio_output_set`: wrappers simples sobre o
    driver de GPIO (LHAL) do SDK.
  - `coex_timing_control_ctx`: variável global de coexistência Wi-Fi/BLE.
- Também é preciso definir no linker script do M0 a seção
  `_ld_bl_static_cfg_entry_start/_end` (o BL602 já faz isso em `bl602_flash.ld`).

**ABI:** os binários foram compilados para
`rv32imafc + p + zpsfoperand + xtheade`, ABI `ilp32f`. É exatamente o
`MARCH`/`MABI` do M0 no SDK base (`drivers/soc/bl808/std/CMakeLists.txt`).

## 4. O que realmente dá trabalho

A camada de símbolos é pequena. O trabalho de verdade está na inicialização do
hardware, que não aparece na lista de símbolos:

1. **Build:** fazer o `components/wireless/wifi4` compilar com `CHIP=bl808
   CPU_ID=m0`, trocando `libfirmware.a` (BL602) pelos binários do BL808 e
   ajustando os `CFG_*` (ex.: `CFG_CHIP_BL808`).
2. **Endereços e interrupções:** base dos registradores de IPC do MAC Wi-Fi e
   números de IRQ do BL808, que são diferentes dos do BL602.
3. **Memória:** a memória compartilhada do Wi-Fi ocupa boa parte da RAM do M0
   (o Zephyr relata sobrar cerca de 108 KB), então heap e buffers de rede devem
   ir para outras regiões (MM_DRAM/PSRAM).
4. **RF:** clocks do Wi-Fi, calibração de RF na inicialização (feita com
   interrupções desligadas) e ajuste do capacitor do cristal (no M1s Dock o
   Zephyr usou o código 50 para ficar dentro de 1 ppm).
5. **`wifi_hosal` para BL808:** MAC address via eFuse, parâmetros de RF,
   temperatura, energia.

Referência para os itens 2 a 5: o driver do Zephyr (PR #121169) e, para
comparar, o código de inicialização do SDK da Sipeed.

## 5. Riscos

- **Binário fechado:** se travar dentro dele, a depuração é às cegas. Mitigação:
  seguir de perto o que o Zephyr faz, já que lá funciona.
- **Teste só no hardware:** cada etapa precisa ser validada na placa.
- **Licença:** os binários têm licença própria da Bouffalo. Melhor baixá-los com
  verificação de checksum (como o Zephyr faz) do que versioná-los no repositório.
- **Base do SDK:** o oficial removeu o BL808, então a base precisa ser o fork da
  OpenBouffalo (v2.3.16). Correções novas do oficial terão que ser trazidas
  manualmente, se interessarem.

## 6. Plano proposto

1. Usar `openbouffalo/bouffalo_sdk_bl808` como base (feito: este fork).
2. Habilitar o `wifi4` para `bl808`/M0, com download verificado dos binários.
3. Implementar os 3 símbolos que faltam e a seção do linker script.
4. Portar a inicialização de hardware (itens 2 a 5 da seção 4).
5. Criar o exemplo `examples/wifi/sta` para o M1s Dock e validar na placa: scan,
   conexão, DHCP, ping.
6. Depois: BLE (o binário `libbl606p_btblecontroller_prebuild.a` também está
   mapeado no Zephyr) e integração com o D0 (câmera, display, NPU).
