// SPDX-License-Identifier: GPL-2.0-only
/*
 * QzRAM - Quantized Memory Compression for the Linux Kernel
 * Fully consolidated single-file design. Pure 100% kernel safe code.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/crypto.h>
#include <linux/compiler.h>
#include <linux/string.h>

/* =====================[ Core Quantization Algorithms ]===================== */

static void qz_compress_group(const u8 *src, u8 *dst, u8 *meta)
{
    u8 min_val = src[0], max_val = src[0];
    for (int i = 1; i < 64; ++i) {
        if (src[i] < min_val) min_val = src[i];
        if (src[i] > max_val) max_val = src[i];
    }

    u8 range = max_val - min_val;
    meta[0] = min_val;
    meta[1] = range;

    if (range == 0) {
        memset(dst, 0, 32);
        return;
    }

    u32 range_u32 = range;
    u32 add = range_u32 >> 1;  // Dynamic rounding optimization

    memset(dst, 0, 32);
    for (int i = 0; i < 64; ++i) {
        u32 tmp = (u32)(src[i] - min_val) * 15 + add;
        u32 q   = tmp / range_u32;
        if (q > 15) q = 15;
        dst[i / 2] |= q << ((i & 1) ? 4 : 0);
    }
}

static void qz_decompress_group(const u8 *src, const u8 *meta, u8 *dst)
{
    u8 min_val = meta[0];
    u8 range   = meta[1];

    if (range == 0) {
        memset(dst, min_val, 64);
        return;
    }

    u32 range_u32 = range;
    u32 add15 = range_u32 >> 1; // Exactly matches dynamic rounding in compression

    for (int i = 0; i < 64; ++i) {
        u8 q = (src[i / 2] >> ((i & 1) ? 4 : 0)) & 0x0F;
        u32 rec = min_val + ((u32)q * range_u32 + add15) / 15;
        dst[i] = (u8)(rec);
    }
}

/* =====================[ comp_alg Interface ]===================== */

static int qzram_compress(struct crypto_tfm *tfm,
                          const u8 *src, unsigned int slen,
                          u8 *dst, unsigned int *dlen)
{
    if (slen != 4096)
        return -EINVAL;

    if (*dlen < 128 + 2048)
        return -EINVAL;
    *dlen = 128 + 2048;

    u8 *meta = dst;
    u8 *body = dst + 128;

    for (int g = 0; g < 64; ++g)
        qz_compress_group(src + g * 64, body + g * 32, meta + g * 2);

    return 0;
}

static int qzram_decompress(struct crypto_tfm *tfm,
                            const u8 *src, unsigned int slen,
                            u8 *dst, unsigned int *dlen)
{
    if (slen < 128 + 2048)
        return -EINVAL;

    if (*dlen < 4096)
        return -EINVAL;
    *dlen = 4096;

    const u8 *meta = src;
    const u8 *body = src + 128;

    for (int g = 0; g < 64; ++g)
        qz_decompress_group(body + g * 32, meta + g * 2, dst + g * 64);

    return 0;
}

/* =====================[ Modern Kernel Registration ]===================== */

static struct comp_alg qzram_comp = {
    .compress   = qzram_compress,
    .decompress = qzram_decompress,
    .base       = {
        .cra_name        = "qzram",
        .cra_driver_name = "qzram-generic",
        .cra_priority    = 150,
        .cra_flags       = CRYPTO_ALG_TYPE_COMPRESS,
        .cra_ctxsize     = 0,
        .cra_module      = THIS_MODULE,
    },
};

static int __init qzram_init(void)
{
    return crypto_register_comp(&qzram_comp);
}

static void __exit qzram_exit(void)
{
    crypto_unregister_comp(&qzram_comp);
}

module_init(qzram_init);
module_exit(qzram_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("QzRAM Project");
MODULE_DESCRIPTION("Quantized memory compression for Linux (qzram)");
