# hbn_rtc

Low power example for the BL808 M0 (`drivers/soc/bl808/std/src/bl808_pm.c`).

## Support CHIP

|      CHIP        | Remark |
|:----------------:|:------:|
|BL808             | M0 only; tested on the Sipeed M1s Dock |

## Compile

```
make CHIP=bl808 BOARD=m1s_dock CPU_ID=m0
```

## Flash

```
make flash CHIP=bl808 CPU_ID=m0 COMX=xxx # xxx is your com name
```
