#ifndef __HARDWARE_LZ4D_H__
#define __HARDWARE_LZ4D_H__

/* Registers of the LZ4 decompressor (BL808 reference manual, LZ4D chapter) */

#define LZ4D_CONFIG_OFFSET    (0x00)
#define LZ4D_SRC_FIX_OFFSET   (0x04)
#define LZ4D_DST_FIX_OFFSET   (0x08)
#define LZ4D_SRC_START_OFFSET (0x10)
#define LZ4D_SRC_END_OFFSET   (0x14)
#define LZ4D_DST_START_OFFSET (0x18)
#define LZ4D_DST_END_OFFSET   (0x1C)
#define LZ4D_INT_EN_OFFSET    (0x20)
#define LZ4D_INT_STA_OFFSET   (0x24)
#define LZ4D_MONITOR_OFFSET   (0x28)

/* 0x00 : lz4_config */
#define LZ4D_EN       (1 << 0U)
#define LZ4D_SUSPEND  (1 << 1U)
#define LZ4D_CHECKSUM (1 << 4U)

/* 0x10/0x14/0x18/0x1C : address registers, 26 bits of offset (64 MB window) */
#define LZ4D_ADDR_MASK (0x03ffffffU)

/* 0x20 : lz4_int_en, 0x24 : lz4_int_sta */
#define LZ4D_DONE            (1 << 0U)
#define LZ4D_ERR             (1 << 1U)
#define LZ4D_DST_INT_SHIFT   (10U)
#define LZ4D_DST_INT_MASK    (0x3fU << LZ4D_DST_INT_SHIFT)
#define LZ4D_DST_INT_INV_SHIFT (26U)

/* 0x28 : lz4_monitor */
#define LZ4D_CS_MASK (0xfU)

#endif /* __HARDWARE_LZ4D_H__ */
