#ifndef _BFLB_LZ4D_H
#define _BFLB_LZ4D_H

#include "bflb_core.h"

/** @addtogroup LHAL
  * @{
  */

/** @addtogroup LZ4D
  * @{
  */

/** @defgroup LZ4D_INT lz4d interrupt and status bits
  * @{
  */
#define LZ4D_INTSTS_DONE (1 << 0)
#define LZ4D_INTSTS_ERR  (1 << 1)
/**
  * @}
  */

/**
 * @brief LZ4 frame, as parsed by bflb_lz4d_frame_parse().
 *
 * @param blocks          first block (size word) after the frame header
 * @param block_checksum  frame header asks for a checksum after each block
 * @param content_checksum frame header asks for a checksum after the end mark
 * @param content_size    decompressed size from the header, 0 if absent
 * @param block_max       maximum block size from the header, in bytes
 */
struct bflb_lz4d_frame_s {
    const void *blocks;
    bool block_checksum;
    bool content_checksum;
    uint64_t content_size;
    uint32_t block_max;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Turn on the clock of the decompressor, reset it and leave it stopped.
 *
 * @param [in] dev device handle
 */
void bflb_lz4d_init(struct bflb_device_s *dev);

/**
 * @brief Stop the decompressor.
 *
 * @param [in] dev device handle
 */
void bflb_lz4d_deinit(struct bflb_device_s *dev);

/**
 * @brief Parse the header of an LZ4 frame (magic 0x184D2204).
 *
 * @param [in] data frame
 * @param [out] frame where the blocks start and what the header says
 * @return 0 on success, -EINVAL if it is not an LZ4 frame (version 01) or
 *         it names a dictionary, which the decompressor cannot take
 */
int bflb_lz4d_frame_parse(const void *data, struct bflb_lz4d_frame_s *frame);

/**
 * @brief Start decompressing the blocks at src into dst.
 *
 * The blocks are LZ4 frame blocks: a 32-bit little endian size (bit 31 set
 * for an uncompressed block), the data and, with block_checksum, a 32-bit
 * checksum, up to the zero end mark. Independent and linked blocks both
 * work. The decompressor is a bus master: the caller writes back the source
 * and invalidates the destination in the data cache.
 *
 * Measured on the BL808 (M1s Dock):
 * - it writes 4 bytes past the end of the output: leave room for them;
 * - it does not check the block checksums (it skips them);
 * - a malformed block does not raise LZ4D_INTSTS_ERR: the decompressor goes
 *   on reading and writing past the buffers until stopped. Only feed it
 *   trusted data, stop it on a timeout (bflb_lz4d_decompress() does), and
 *   consider LZ4D_DST_FIX_OFFSET: with bits 25:16 set, writes stay inside
 *   the 64 KB aligned window of the destination.
 *
 * @param [in] dev device handle
 * @param [in] src first block size word
 * @param [in] dst destination
 * @param [in] block_checksum each block is followed by a checksum
 */
void bflb_lz4d_start(struct bflb_device_s *dev, const void *src, void *dst, bool block_checksum);

/**
 * @brief Stop a decompression (and clear the status bits).
 *
 * @param [in] dev device handle
 */
void bflb_lz4d_stop(struct bflb_device_s *dev);

/**
 * @brief Suspend or resume a running decompression.
 *
 * @param [in] dev device handle
 * @param [in] suspend true to suspend
 */
void bflb_lz4d_suspend(struct bflb_device_s *dev, bool suspend);

/**
 * @brief Status bits (LZ4D_INTSTS_*) and destination progress bits (15:10).
 *
 * @param [in] dev device handle
 * @return raw status register
 */
uint32_t bflb_lz4d_get_intstatus(struct bflb_device_s *dev);

/**
 * @brief Enable or disable interrupt sources (LZ4D_INTSTS_* bits).
 *
 * @param [in] dev device handle
 * @param [in] int_type interrupt bits
 * @param [in] enable true to enable
 */
void bflb_lz4d_int_enable(struct bflb_device_s *dev, uint32_t int_type, bool enable);

/**
 * @brief Interrupt when the destination address bits 15:10 change.
 *
 * @param [in] dev device handle
 * @param [in] mask bit n raises an interrupt when destination address bit
 *             (10 + n) changes; 0 disables
 */
void bflb_lz4d_progress_int_enable(struct bflb_device_s *dev, uint8_t mask);

/**
 * @brief Bytes written to the destination so far.
 *
 * @param [in] dev device handle
 * @return dst_end - dst_start
 */
uint32_t bflb_lz4d_get_out_size(struct bflb_device_s *dev);

/**
 * @brief Bytes read from the source so far.
 *
 * @param [in] dev device handle
 * @return src_end - src_start
 */
uint32_t bflb_lz4d_get_in_size(struct bflb_device_s *dev);

/**
 * @brief Decompress one LZ4 frame by polling.
 *
 * Parses the frame header, writes back and invalidates the cache as needed,
 * starts the decompressor and waits for the end.
 *
 * @param [in] dev device handle
 * @param [in] frame LZ4 frame (with its header)
 * @param [in] frame_len size of the frame, for the cache maintenance
 * @param [in] dst destination
 * @param [in] dst_len size of the destination
 * @param [in] timeout_ms time limit
 * @return decompressed size, -EINVAL (bad frame), -EIO (decompressor error),
 *         -ENOSPC (more than dst_len written) or -ETIMEDOUT
 */
int bflb_lz4d_decompress(struct bflb_device_s *dev, const void *frame, uint32_t frame_len,
                         void *dst, uint32_t dst_len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

/**
  * @}
  */

/**
  * @}
  */

#endif
