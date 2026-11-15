#include "bflb_lz4d.h"
#include "bflb_l1c.h"
#include "bflb_mtimer.h"
#include "hardware/lz4d_reg.h"

/* GLB: clock gate (cgen_cfg1 bit 29) and software reset (swrst_cfg1 bit 29) */
#define LZ4D_GLB_CGEN_CFG1  (0x20000000 + 0x584)
#define LZ4D_GLB_SWRST_CFG1 (0x20000000 + 0x544)
#define LZ4D_GLB_BIT        (1 << 29)

#define LZ4F_MAGIC 0x184D2204U

static uint32_t lz4d_rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void bflb_lz4d_init(struct bflb_device_s *dev)
{
    uint32_t regval;

    regval = getreg32(LZ4D_GLB_CGEN_CFG1);
    putreg32(regval | LZ4D_GLB_BIT, LZ4D_GLB_CGEN_CFG1);

    regval = getreg32(LZ4D_GLB_SWRST_CFG1);
    putreg32(regval & ~LZ4D_GLB_BIT, LZ4D_GLB_SWRST_CFG1);
    putreg32(regval | LZ4D_GLB_BIT, LZ4D_GLB_SWRST_CFG1);
    putreg32(regval & ~LZ4D_GLB_BIT, LZ4D_GLB_SWRST_CFG1);

    putreg32(0, dev->reg_base + LZ4D_CONFIG_OFFSET);
    putreg32(0, dev->reg_base + LZ4D_SRC_FIX_OFFSET);
    putreg32(0, dev->reg_base + LZ4D_DST_FIX_OFFSET);
    /* Interrupts are enabled at reset: leave them to bflb_lz4d_int_enable() */
    putreg32(0, dev->reg_base + LZ4D_INT_EN_OFFSET);
}

void bflb_lz4d_deinit(struct bflb_device_s *dev)
{
    uint32_t regval;

    putreg32(0, dev->reg_base + LZ4D_CONFIG_OFFSET);
    regval = getreg32(LZ4D_GLB_CGEN_CFG1);
    putreg32(regval & ~LZ4D_GLB_BIT, LZ4D_GLB_CGEN_CFG1);
}

int bflb_lz4d_frame_parse(const void *data, struct bflb_lz4d_frame_s *frame)
{
    const uint8_t *p = data;
    uint8_t flg, bd;

    if (lz4d_rd_le32(p) != LZ4F_MAGIC) {
        return -EINVAL;
    }

    flg = p[4];
    bd = p[5];
    if ((flg >> 6) != 1 || (flg & 0x01) != 0 || ((bd >> 4) & 0x7) < 4) {
        return -EINVAL;
    }

    frame->block_checksum = (flg & 0x10) != 0;
    frame->content_checksum = (flg & 0x04) != 0;
    frame->block_max = 1U << (8 + 2 * ((bd >> 4) & 0x7));
    p += 6;

    frame->content_size = 0;
    if (flg & 0x08) {
        frame->content_size = (uint64_t)lz4d_rd_le32(p) | ((uint64_t)lz4d_rd_le32(p + 4) << 32);
        p += 8;
    }

    /* header checksum */
    p += 1;
    frame->blocks = p;

    return 0;
}

void bflb_lz4d_start(struct bflb_device_s *dev, const void *src, void *dst, bool block_checksum)
{
    uint32_t reg_base = dev->reg_base;

    putreg32(0, reg_base + LZ4D_CONFIG_OFFSET);
    putreg32((uint32_t)(uintptr_t)src, reg_base + LZ4D_SRC_START_OFFSET);
    putreg32((uint32_t)(uintptr_t)dst, reg_base + LZ4D_DST_START_OFFSET);
    putreg32(block_checksum ? (LZ4D_CHECKSUM | LZ4D_EN) : LZ4D_EN, reg_base + LZ4D_CONFIG_OFFSET);
}

void bflb_lz4d_stop(struct bflb_device_s *dev)
{
    putreg32(0, dev->reg_base + LZ4D_CONFIG_OFFSET);
}

void bflb_lz4d_suspend(struct bflb_device_s *dev, bool suspend)
{
    uint32_t regval;

    regval = getreg32(dev->reg_base + LZ4D_CONFIG_OFFSET);
    if (suspend) {
        regval |= LZ4D_SUSPEND;
    } else {
        regval &= ~LZ4D_SUSPEND;
    }
    putreg32(regval, dev->reg_base + LZ4D_CONFIG_OFFSET);
}

uint32_t bflb_lz4d_get_intstatus(struct bflb_device_s *dev)
{
    return getreg32(dev->reg_base + LZ4D_INT_STA_OFFSET);
}

void bflb_lz4d_int_enable(struct bflb_device_s *dev, uint32_t int_type, bool enable)
{
    uint32_t regval;

    int_type &= LZ4D_INTSTS_DONE | LZ4D_INTSTS_ERR;
    regval = getreg32(dev->reg_base + LZ4D_INT_EN_OFFSET);
    if (enable) {
        regval |= int_type;
    } else {
        regval &= ~int_type;
    }
    putreg32(regval, dev->reg_base + LZ4D_INT_EN_OFFSET);
}

void bflb_lz4d_progress_int_enable(struct bflb_device_s *dev, uint8_t mask)
{
    uint32_t regval;

    regval = getreg32(dev->reg_base + LZ4D_INT_EN_OFFSET);
    regval &= ~LZ4D_DST_INT_MASK;
    regval |= ((uint32_t)mask << LZ4D_DST_INT_SHIFT) & LZ4D_DST_INT_MASK;
    putreg32(regval, dev->reg_base + LZ4D_INT_EN_OFFSET);
}

uint32_t bflb_lz4d_get_out_size(struct bflb_device_s *dev)
{
    return (getreg32(dev->reg_base + LZ4D_DST_END_OFFSET) - getreg32(dev->reg_base + LZ4D_DST_START_OFFSET)) &
           LZ4D_ADDR_MASK;
}

uint32_t bflb_lz4d_get_in_size(struct bflb_device_s *dev)
{
    return (getreg32(dev->reg_base + LZ4D_SRC_END_OFFSET) - getreg32(dev->reg_base + LZ4D_SRC_START_OFFSET)) &
           LZ4D_ADDR_MASK;
}

int bflb_lz4d_decompress(struct bflb_device_s *dev, const void *frame, uint32_t frame_len,
                         void *dst, uint32_t dst_len, uint32_t timeout_ms)
{
    struct bflb_lz4d_frame_s f;
    uint64_t start;
    uint32_t sts, out;
    int ret;

    ret = bflb_lz4d_frame_parse(frame, &f);
    if (ret != 0) {
        return ret;
    }

    bflb_l1c_dcache_clean_range((void *)frame, frame_len);
    bflb_l1c_dcache_clean_invalidate_range(dst, dst_len);

    bflb_lz4d_start(dev, f.blocks, dst, f.block_checksum);
    start = bflb_mtimer_get_time_ms();
    do {
        sts = bflb_lz4d_get_intstatus(dev);
        if (sts & (LZ4D_INTSTS_DONE | LZ4D_INTSTS_ERR)) {
            break;
        }
    } while ((bflb_mtimer_get_time_ms() - start) < timeout_ms);

    out = bflb_lz4d_get_out_size(dev);
    bflb_lz4d_stop(dev);
    bflb_l1c_dcache_invalidate_range(dst, dst_len);

    if (sts & LZ4D_INTSTS_ERR) {
        return -EIO;
    }
    if ((sts & LZ4D_INTSTS_DONE) == 0) {
        return -ETIMEDOUT;
    }
    if (out > dst_len) {
        return -ENOSPC;
    }

    return (int)out;
}
