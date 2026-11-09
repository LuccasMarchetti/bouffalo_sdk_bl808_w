/*
 * XuanTie C906 vector unit (RVV 0.7.1) state in the task context.
 *
 * When the code is built with the vector extension, every context switch
 * saves v0-v31, vl, vtype, vstart, vxsat and vxrm on the task stack, below
 * the integer registers, so any number of tasks can use the vector unit.
 * Interrupt handlers must not use it.
 *
 * Included by port.c and portASM.S: preprocessor definitions only.
 */
#ifndef PORT_VECTOR_H
#define PORT_VECTOR_H

#if defined(__riscv_xtheadvector)
/* GCC 14 and later: -march=..._xtheadvector, "th." mnemonics */
#define portHAS_VECTOR 1
#define portVSETVLI    th.vsetvli
#define portVSETVL     th.vsetvl
#define portVSE        th.vse.v
#define portVLE        th.vle.v
#elif defined(__riscv_vector) && defined(__riscv_v) && (__riscv_v == 7000)
/* XuanTie GCC: -march=...v0p7..., plain mnemonics */
#define portHAS_VECTOR 1
#define portVSETVLI    vsetvli
#define portVSETVL     vsetvl
#define portVSE        vse.v
#define portVLE        vle.v
#else
#define portHAS_VECTOR 0
#endif

#if portHAS_VECTOR
/* C906: VLEN = 128 bits */
#define portVECTOR_REG_BYTES    16
#define portVECTOR_GROUP_BYTES  (8 * portVECTOR_REG_BYTES)
#define portVECTOR_VL_OFFSET     (32 * portVECTOR_REG_BYTES)
#define portVECTOR_VTYPE_OFFSET  (portVECTOR_VL_OFFSET + 8)
#define portVECTOR_VSTART_OFFSET (portVECTOR_VL_OFFSET + 16)
#define portVECTOR_VXSAT_OFFSET  (portVECTOR_VL_OFFSET + 24)
#define portVECTOR_VXRM_OFFSET   (portVECTOR_VL_OFFSET + 32)
/* Rounded up to keep the stack 16-byte aligned */
#define portVECTOR_FRAME_SIZE    (portVECTOR_VL_OFFSET + 48)
#else
#define portVECTOR_FRAME_SIZE    0
#endif

#endif /* PORT_VECTOR_H */
