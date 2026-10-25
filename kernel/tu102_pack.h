/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef TU102_PACK_H
#define TU102_PACK_H
#include "tu102_dma.h"
#define TU102_META_OFF 0x40000u
#define TU102_RADIX_OFF 0x50000u
#define TU102_DATA_OFF 0x60000u
#define TU102_IMAGE_SIZE 0x1bfadc8u
struct tu102_pack_result {
    u64 metadata_dma, radix_dma, frts, wpr_start, wpr_end;
    unsigned int image_pages;
    u64 metadata_words[32];
};
int tu102_pack(struct tu102_dma *state, u64 fb, unsigned int workspace,
               struct tu102_pack_result *out);
int tu102_pack_verify(struct tu102_dma *state, const struct tu102_pack_result *out);
#endif
