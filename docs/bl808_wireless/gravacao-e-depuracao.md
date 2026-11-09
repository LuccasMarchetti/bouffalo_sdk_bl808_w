# Gravação e depuração do BL808 (M1s Dock)

Itens marcados com **(a confirmar)** ainda não foram testados na placa.

## Gravação sem o Dev Cube

O Dev Cube (a interface gráfica) não é necessário. O SDK já traz a versão de
linha de comando dele (`tools/bflb_tools/bouffalo_flash_cube/BLFlashCommand-*`,
para Linux, macOS e Windows), e o `make` a chama:

```sh
cd examples/helloworld
make CHIP=bl808 BOARD=bl808dk CPU_ID=m0
make flash CHIP=bl808 CPU_ID=m0 COMX=/dev/ttyUSB1   # Windows: COMX=COM5
```

- Cada núcleo tem o próprio firmware: `CPU_ID=m0` ou `CPU_ID=d0`.
- O que é gravado, e onde, fica em `flash_prog_cfg.ini`, na pasta de cada
  exemplo.
- **Modo de gravação:** segure **BOOT**, aperte e solte **RST**, solte
  **BOOT**. Depois de gravar, aperte **RST** para rodar.
- **Porta serial (a confirmar):** pela USB "UART" do M1s Dock, o BL702 da
  placa cria duas portas seriais. A gravação usa a que está ligada à UART0 do
  BL808, normalmente a segunda (`/dev/ttyUSB1`).
- No Linux, se der erro de permissão:
  `sudo usermod -aG dialout $USER`, depois saia e entre de novo na sessão.

Alternativa de código aberto: [blisp](https://github.com/pine64/blisp)
(`blisp iot -c bl808 ...`).

Próximo passo planejado: um comando `make flash-all` que grave M0 e D0 de uma
vez.

## Depuração (JTAG)

O SDK já traz o necessário:

| Item | Onde |
|---|---|
| OpenOCD (Linux/Windows) | `tools/openocd/` |
| Núcleo **M0** (E907) | `tools/openocd/target/tgt_e907.cfg` |
| Núcleo **D0** (C906) | `tools/openocd/target/tgt_c906.cfg` |
| Adaptadores | `tools/openocd/interface/` (CH347, FT2232D, FT2232H) |
| Scripts do GDB | `tools/gdbinit/bl808.init` |
| Firmware CKLink para BL702 | `tools/cklink_firmware/` |

### Depurar cada núcleo

No BL808, os pinos de JTAG são atribuídos a um núcleo por função de GPIO:
`GPIO_FUNC_JTAG_M0`, `GPIO_FUNC_JTAG_D0` e `GPIO_FUNC_JTAG_LP`
(`drivers/lhal/include/bflb_gpio.h`). Então:

- **um núcleo por vez:** o firmware configura os pinos de JTAG para o núcleo
  desejado, e o OpenOCD usa o `.cfg` correspondente;
- **dois núcleos ao mesmo tempo:** dois conjuntos de pinos e dois adaptadores
  **(a confirmar)**.

### Adaptador

- Qualquer adaptador JTAG suportado pelo OpenOCD: CH347 ou FT2232, que já
  têm `.cfg` aqui.
- Sipeed RV-Debugger Plus (BL702) com o firmware CKLink de
  `tools/cklink_firmware/`.
- **(a confirmar)** Usar o próprio BL702 do M1s Dock como adaptador JTAG.

Exemplo, M0 com CH347:

```sh
tools/openocd/openocd -f tools/openocd/interface/ch347.cfg -f tools/openocd/target/tgt_e907.cfg
riscv64-unknown-elf-gdb build/build_out/*.elf -x tools/gdbinit/bl808.init
```

O `bl808.init` conecta na porta `:1025`. Para o OpenOCD, use `:3333` ou ajuste
o script.

### Interface gráfica (planejado)

- VS Code: configurações de `launch.json` (Cortex-Debug ou cppdbg) para M0 e
  D0, usando o OpenOCD acima.
- Eclipse: já existe `tools/eclipse_gdb_launch/`.
