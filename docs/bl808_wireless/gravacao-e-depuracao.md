# Gravação e depuração do BL808 (M1s Dock)

Itens marcados com **(a confirmar)** ainda não foram testados na placa.

## Portas seriais do M1s Dock

O BL702 da placa cria duas portas seriais pela USB "UART". O console de cada
núcleo sai em pinos diferentes (`bsp/board/bl808dk/board.c`):

| Núcleo | UART | Pinos | Uso |
|---|---|---|---|
| M0 | UART0 | GPIO14 (TX) / GPIO15 (RX) | console do M0 e **gravação** |
| D0 | UART3 | GPIO16 / GPIO17 | console do D0 |

Velocidade: **2.000.000 baud** nos dois. No Linux, normalmente aparecem como
`/dev/ttyUSB0` e `/dev/ttyUSB1`. Qual porta é de qual núcleo está
**(a confirmar)**: a de gravação é a que mostra o log de boot do M0.

No Linux, se der erro de permissão:
`sudo usermod -aG dialout $USER`, depois saia e entre de novo na sessão.

## Gravação sem o Dev Cube

**Modo de gravação:** segure **BOOT**, aperte e solte **RST**, solte **BOOT**.
Depois de gravar, aperte **RST** para rodar.

### Um núcleo

```sh
cd examples/wifi/sta/wifi_tcp
make CHIP=bl808 BOARD=bl808dk CPU_ID=m0
make flash CHIP=bl808 CPU_ID=m0 COMX=/dev/ttyUSB1
```

### M0 e D0 de uma vez

```sh
tools/bl808_wireless/flash.sh --port /dev/ttyUSB1 \
    --m0 examples/wifi/sta/wifi_tcp \
    --d0 examples/bl808_triplecore/helloworld_d0
```

- Compile os dois exemplos antes (`CPU_ID=m0` e `CPU_ID=d0`).
- O script junta a configuração do M0 (boot2, tabela de partições, firmware)
  com a imagem do D0 em `0x100000`, o endereço de onde o M0 inicia o D0
  (`CONFIG_D0_FLASH_ADDR` em `bsp/board/bl808dk/board.h`).
- Ele recusa gravar se o firmware do M0 invadir a área do D0. Com a tabela de
  partições padrão, o M0 começa em `0x10000`, então o limite é 960 KB.
- `--d0` ou `--m0` sozinhos gravam só um núcleo. `--erase-all` apaga a flash
  inteira antes.
- `--pack` não grava: só monta `whole_flash_data.bin`, a imagem completa da
  flash.

Testado aqui com `--pack`: boot2 em `0x0`, M0 em `0x10000` e D0 em `0x100000`,
conferidos byte a byte. A gravação pela serial em si está **(a confirmar)**.

Aviso: a tabela de partições do fork reserva `0x10000`–`0x290000` para o
firmware do M0, uma área que inclui `0x100000`. Isso funciona enquanto o M0
couber em 960 KB, mas uma atualização OTA do M0 apagaria o D0.

Alternativa de código aberto: [blisp](https://github.com/pine64/blisp)
(`blisp iot -c bl808 ...`).

## Depuração (JTAG)

### Ligar o JTAG ao núcleo certo

No BL808, os pinos de JTAG pertencem a um núcleo por vez, escolhido pela
função de GPIO (`GPIO_FUNC_JTAG_M0`, `_D0`, `_LP`). Compile o núcleo que
quer depurar com:

```sh
make CHIP=bl808 BOARD=bl808dk CPU_ID=d0 CONFIG_BL808_JTAG=y
```

Assim o `board_init()` chama `board_jtag_gpio_init()`, que configura
**GPIO4–GPIO7** para aquele núcleo, como em `examples/lvgl/lvgl_v8_with_osd`.
Para usar outros quatro pinos seguidos:
`CONFIG_BL808_JTAG_PIN_BASE=<n>`.

Ative em **um** núcleo só: se os dois configurarem os pinos, vale o último,
que normalmente é o D0, porque ele inicia depois do M0.

**No M1s Dock** (medido na bancada, 11/2026): GPIO4–7 não estão livres; o
grupo de JTAG acessível é o do **slot micro SD**, com o adaptador de JTAG da
Sipeed: GPIO0 = TCK, GPIO1 = TDI, GPIO2 = TMS, GPIO3 = TDO. Compile com
`CONFIG_BL808_JTAG=y CONFIG_BL808_JTAG_PIN_BASE=0`. O padrão continua 4 porque
este `board.c` é o do `bl808dk`. O `board_jtag_gpio_init()` também tira a
função JTAG dos GPIO6/7/12/13, que o bootrom deixa em `JTAG_M0`: com dois
grupos de pinos em JTAG, o TAP não responde.

**TAP da T-Head e OpenOCD:** o TAP do E907/C906 não volta o IR para IDCODE no
Test-Logic-Reset. Sem cuidado, o OpenOCD falha uma sessão sim, outra não
(lê o IDCODE com o IR em DMI, deixado pela sessão anterior). As cfgs
`tgt_e907.cfg` e `tgt_c906.cfg` deixam o IR em IDCODE ao sair
(`pre_shutdown_commands`). Se uma sessão morrer sem o `shutdown`, a seguinte
pode falhar uma vez: rode de novo.

**Leitura do XIP pelo depurador:** no M0, a CPU vê a imagem em `0x58000000`,
mas o GDB/OpenOCD lê `0x58000000 + X` na posição X da flash, sem o
deslocamento da imagem (no Zephyr, `0x2000`). Para conferir o código gravado,
leia a partir de `0x58002000`.

### O que o SDK já traz

| Item | Onde |
|---|---|
| OpenOCD (Linux/Windows) | `tools/openocd/` |
| Núcleo **M0** (E907) | `tools/openocd/target/tgt_e907.cfg` |
| Núcleo **D0** (C906) | `tools/openocd/target/tgt_c906.cfg` |
| Adaptadores | `tools/openocd/interface/` (CH347, FT2232D, FT2232H) |
| Firmware CKLink para BL702 | `tools/cklink_firmware/` |

Adaptador: qualquer um suportado pelo OpenOCD (CH347 ou FT2232 já têm
`.cfg`). O Sipeed RV-Debugger Plus funciona com o OpenOCD (`ft2232d.cfg`) se
tiver o firmware `usb2uartjtag` da Sipeed, que emula um FT2232D; com o
firmware CKLink ele só funciona com o T-Head DebugServer. Usar o próprio
BL702 do M1s Dock como JTAG está **(a confirmar)**.

### Linha de comando

```sh
# M0 (porta 3333) ou D0 (troque por tgt_c906.cfg e use a porta 3334)
tools/openocd/openocd -f tools/openocd/interface/ch347.cfg \
    -f tools/openocd/target/tgt_e907.cfg -c 'gdb_port 3333'

riscv64-unknown-elf-gdb examples/wifi/sta/wifi_tcp/build/build_out/wifi_tcp_bl808_m0.elf \
    -ex 'set architecture riscv:rv32' -ex 'target remote :3333'
```

O GDB do XuanTie 2.6.1 precisa da `libncursesw.so.5`. No Ubuntu/Debian:
`sudo apt install libncursesw5`.

Testado aqui: o OpenOCD lê as duas configurações e para só ao procurar o
adaptador, o esperado sem hardware.

### VS Code

```sh
cp -r tools/bl808_wireless/vscode .vscode
```

Precisa da extensão **C/C++** (`ms-vscode.cpptools`) e do compilador no
`PATH`. Ela oferece:

- **Tarefas** (`Ctrl+Shift+B` ou *Terminal → Executar tarefa*): compilar M0,
  D0 ou os dois; gravar M0 + D0, só M0 ou só D0; abrir o OpenOCD do M0 ou
  do D0.
- **Depuração** (aba *Executar e depurar*): "Depurar M0 (E907)" e "Depurar
  D0 (C906)". O OpenOCD sobe sozinho e o GDB se conecta ao programa que já
  está rodando: o código fica na flash e não é recarregado.

O VS Code pergunta o exemplo, a porta e o adaptador a cada uso. Os valores
padrão apontam para `wifi_tcp` (M0) e `bl808_triplecore/helloworld_d0` (D0).

Dois núcleos ao mesmo tempo, cada um num grupo de pinos com o seu
adaptador, **não funcionou** na bancada: com dois grupos de GPIO na função
JTAG, o TAP não responde. Para depurar o M0 e o D0 na mesma sessão, passe o
JTAG de um para o outro sem regravar (`jtag.sh passa`, abaixo).

Eclipse: já existe `tools/eclipse_gdb_launch/CKLink_BL808.launch`.

## Na bancada (M1s Dock, debugger nos GPIO0–3)

Os scripts ficam no repositório de notas (`bl809/bancada/`) e foram
testados na placa em 11/11/2026. A imagem que roda precisa ter ligado o
JTAG nos GPIO0–3 (`CONFIG_BL808_JTAG=y CONFIG_BL808_JTAG_PIN_BASE=0` no SDK,
`CONFIG_SOC_BL808_JTAG=y` no Zephyr). O bootrom só deixa o JTAG nos
GPIO6/7/12/13, então uma flash vazia, ou uma imagem sem essa opção, só se
grava pela UART.

| O que | Comando | Observação |
|---|---|---|
| Gravar a flash pelo JTAG | `jtag_flash.py [--reset] ENDERECO:ARQUIVO ...` | carrega um gravador na RAM do M0, grava em blocos de 16 KB e confere o CRC32; ~7 KB/s |
| Carregar e rodar na RAM pelo JTAG | `jtag.sh ram m0 ELF` | ELF ligado na RAM (ver `bancada/jtag_flash/bl808_ram_m0.ld`) |
| Carregar na RAM pela UART (bootrom) | `bflb-mcu-tool-uart --chipname bl808 --bootsrc UART/USB --addr 0x62030000 --firmware X.bin --port ... --baudrate 2000000` | com o chip no modo de gravação; não toca na flash |
| Passar o JTAG do M0 para o D0 e voltar | `jtag.sh passa m0 d0`, `jtag.sh passa d0 m0` | o núcleo que tem o JTAG executa um trecho que reescreve a função dos 4 GPIO; os dois continuam rodando |
| Reiniciar no modo de gravação | `jtag.sh reset` | com o cabeçalho apagado pelo serviço de bancada |

Imagem do Zephyr para o endereço 0: `bflb-mcu-tool-uart --chipname bl808
--firmware zephyr.bin --build` gera `whole_img.bin` (cabeçalho + código em
`0x2000`). No SDK, o `build_out/*_bl808_m0.bin` já vai no endereço 0.

**LP (E902):** não se depura pelo OpenOCD. Ele não tem o módulo de depuração
RISC-V nos pinos (TDO sempre em 1 com os GPIO na função `JTAG_LP`), e sim o
HAD da T-Head, de 2 fios. É preciso um CK-Link (ou o firmware CK-Link no
RV-Debugger) e o T-Head DebugServer.
