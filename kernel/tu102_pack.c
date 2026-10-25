// SPDX-License-Identifier: GPL-2.0-only
#ifndef TU102_DMA_HOST_TEST
#include <linux/errno.h>
#include <linux/string.h>
#endif
#include "tu102_pack.h"

/* Logical offsets map to separate 64 KiB blocks. */
static unsigned char *cpu_at(struct tu102_dma *s, unsigned int off)
{
    return (unsigned char *)s->blocks[off / TU102_DMA_BLOCK_SIZE].cpu + off % TU102_DMA_BLOCK_SIZE;
}
static u64 dma_at(struct tu102_dma *s, unsigned int off)
{
    return s->blocks[off / TU102_DMA_BLOCK_SIZE].address + off % TU102_DMA_BLOCK_SIZE;
}
static void put64(unsigned char *p, u64 v)
{
    unsigned int i;
    for (i = 0; i < 8; i++) p[i] = (unsigned char)(v >> (8 * i));
}
static u64 get64(const unsigned char *p)
{
    u64 v = 0;
    unsigned int i;
    for (i = 0; i < 8; i++) v |= (u64)p[i] << (8 * i);
    return v;
}
static int validate_blocks(struct tu102_dma *s)
{
    unsigned int i, j;
    if (!s || !s->blocks || s->allocated != TU102_DMA_BLOCKS ||
        s->size != TU102_DMA_BLOCKS * TU102_DMA_BLOCK_SIZE)
        return -EINVAL;
    for (i = 0; i < s->allocated; i++) {
        u64 a = s->blocks[i].address;
        if (!s->blocks[i].cpu || (a & 4095) || a < (1ULL << 32) ||
            a > (1ULL << 40) - TU102_DMA_BLOCK_SIZE)
            return -ERANGE;
        for (j = 0; j < i; j++) {
            u64 b = s->blocks[j].address;
            if (a < b + TU102_DMA_BLOCK_SIZE && b < a + TU102_DMA_BLOCK_SIZE)
                return -EINVAL;
            if (s->blocks[i].cpu == s->blocks[j].cpu)
                return -EINVAL;
        }
    }
    return 0;
}

int tu102_pack(struct tu102_dma *s, u64 fb, unsigned int workspace,
               struct tu102_pack_result *out)
{
    const u64 mb = 1ULL << 20;
    const unsigned int pages = (TU102_IMAGE_SIZE + 4095u) / 4096u;
    const unsigned int leaves = (pages + 511u) / 512u;
    u64 m[32] = {0}, base, heap;
    unsigned int i;
    unsigned char *meta, *radix;
    int ret = validate_blocks(s);
    if (ret) return ret;
    if (!out || fb != (22ULL << 30) || workspace == ~0u ||
        (workspace & 0xffff0000u) == 0xbadf0000u)
        return -EINVAL;
    if ((2u + leaves) * 4096u > TU102_DMA_BLOCK_SIZE ||
        TU102_DATA_OFF + pages * 4096u > s->size)
        return -EOVERFLOW;
    base = (workspace & 8) ? (u64)(workspace >> 8) << 16 : fb - mb;
    if (base >= fb || base < fb - mb) return -ERANGE;
    heap = (8 + 96 + (((fb >> 30) * 96 * 1024 + mb - 1) / mb)) * mb;
    m[0] = 0xdc3aae21371a60b3ULL;
    m[1] = 1;
    m[2] = dma_at(s, TU102_RADIX_OFF);
    m[3] = TU102_IMAGE_SIZE;
    m[4] = dma_at(s, 0x30000);
    m[5] = 0x6000;
    m[6] = 0x1800;
    m[7] = 0x800;
    m[9] = dma_at(s, 0x20000);
    m[10] = 0xfa00;
    m[22] = fb;
    m[23] = base;
    m[24] = fb - base;
    m[21] = base & ~0x1ffffULL;
    m[20] = mb;
    m[19] = m[21] - mb;
    m[18] = (m[19] - m[5]) & ~0xfffULL;
    m[17] = (m[18] - m[3]) & ~0xffffULL;
    m[15] = (m[17] - heap) & ~(mb - 1);
    m[16] = (m[17] - m[15]) & ~(mb - 1);
    m[14] = m[15] - mb;
    m[13] = mb;
    m[12] = m[14] - mb;
    m[11] = m[12];
    if (m[11] < fb - 256 * mb || m[15] + m[16] > m[17] ||
        m[17] + m[3] > m[18] || m[18] + m[5] > m[19])
        return -ERANGE;
    meta = cpu_at(s, TU102_META_OFF);
    radix = cpu_at(s, TU102_RADIX_OFF);
    memset(meta, 0, 256);
    memset(radix, 0, TU102_DMA_BLOCK_SIZE);
    for (i = 0; i < 32; i++) put64(meta + i * 8, m[i]);
    /* Flags remain zero for this non-executing preparation profile. */
    put64(radix, dma_at(s, TU102_RADIX_OFF + 4096));
    for (i = 0; i < leaves; i++)
        put64(radix + 4096 + i * 8, dma_at(s, TU102_RADIX_OFF + (2 + i) * 4096));
    for (i = 0; i < pages; i++)
        put64(radix + 8192 + i * 8, dma_at(s, TU102_DATA_OFF + i * 4096));
    *out = (struct tu102_pack_result) {
        .metadata_dma = dma_at(s, TU102_META_OFF), .radix_dma = m[2],
        .frts = m[19], .wpr_start = m[14], .wpr_end = m[21], .image_pages = pages
    };
    for (i = 0; i < 32; i++) out->metadata_words[i] = m[i];
    return tu102_pack_verify(s, out);
}

int tu102_pack_verify(struct tu102_dma *s, const struct tu102_pack_result *out)
{
    unsigned int i, j;
    const unsigned int pages = (TU102_IMAGE_SIZE + 4095u) / 4096u;
    const unsigned int leaves = (pages + 511u) / 512u;
    const unsigned char *meta, *radix;
    int ret = validate_blocks(s);
    if (ret) return ret;
    if (!out || out->image_pages != pages || out->metadata_dma != dma_at(s, TU102_META_OFF) || out->radix_dma != dma_at(s, TU102_RADIX_OFF))
        return -EINVAL;
    meta = cpu_at(s, TU102_META_OFF);
    radix = cpu_at(s, TU102_RADIX_OFF);
    for (i = 0; i < 32; i++)
        if (get64(meta + i*8) != out->metadata_words[i]) return -EBADMSG;
    if (get64(meta) != 0xdc3aae21371a60b3ULL || get64(meta+8) != 1 ||
        get64(meta+16) != out->radix_dma || get64(meta+24) != TU102_IMAGE_SIZE ||
        get64(meta+19*8) != out->frts || get64(meta+14*8) != out->wpr_start ||
        get64(meta+21*8) != out->wpr_end ||
        get64(radix) != dma_at(s, TU102_RADIX_OFF+4096)) return -EBADMSG;
    for (i = 1; i < 512; i++) if (get64(radix+i*8)) return -EBADMSG;
    for (i = 0; i < 512; i++) {
        u64 expected = i < leaves ? dma_at(s, TU102_RADIX_OFF+(2+i)*4096) : 0;
        if (get64(radix+4096+i*8) != expected) return -EBADMSG;
    }
    for (i = 0; i < leaves; i++) for (j = 0; j < 512; j++) {
        unsigned int page = i*512+j;
        u64 expected = page < pages ? dma_at(s, TU102_DATA_OFF+page*4096) : 0;
        if (get64(radix+(2+i)*4096+j*8) != expected) return -EBADMSG;
    }
    return 0;
}
