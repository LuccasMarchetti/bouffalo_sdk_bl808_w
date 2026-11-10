# freertos_vector

Tests that the FreeRTOS port of the BL808 D0 (XuanTie C906) saves the vector
unit (RVV 0.7.1) on context switch: three tasks keep their own values in
v0-v31 while the tick preempts them.

## Support CHIP

|      CHIP        | Remark |
|:----------------:|:------:|
|BL808             | Only for D0 CPU |

## Compile

```
make CHIP=bl808 BOARD=bl808dk CPU_ID=d0
```

## Expected output

After about 10 s on the D0 console: `VECTOR TEST PASS`.
