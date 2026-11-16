#include "rkos/rkos.h"

/*
 * Reconstructed Apple corecrypto AES-128/192/256 (ECB/CBC/CTR/CCM), multi-precision
 * BigNum (`ccn_*`), NIST SP 800-90A HMAC-DRBG (`init`, `reseed`, `generate`,
 * `update`), and AAPCS utility routines.
 */

static inline uint32_t rkos_rotr32_aes(uint32_t val, unsigned int shift)
{
    return (val >> shift) | (val << (32u - shift));
}

static inline uint32_t rkos_bswap32(uint32_t val)
{
    return ((val & 0x000000FFu) << 24) |
           ((val & 0x0000FF00u) << 8)  |
           ((val & 0x00FF0000u) >> 8)  |
           ((val & 0xFF000000u) >> 24);
}

static inline uint32_t rkos_aes_subword(const uint8_t *sbox, uint32_t word)
{
    return ((uint32_t)sbox[word & 0xFFu]) |
           ((uint32_t)sbox[(word >> 8) & 0xFFu] << 8) |
           ((uint32_t)sbox[(word >> 16) & 0xFFu] << 16) |
           ((uint32_t)sbox[(word >> 24) & 0xFFu] << 24);
}

/* @provenance RVA 0x1988 sub_1988 */
void rkos_aes_key_expand_enc(const uint32_t *key, uint32_t *ctx, uint32_t nk, uint32_t nr)
{
    uint32_t rcon_va = *RKOS_VMA_PTR(const uint32_t *, 0x20000B8Cu);
    uint32_t sbox_va = *RKOS_VMA_PTR(const uint32_t *, 0x20000B90u);
    const uint8_t *rcon = (const uint8_t *)RKOS_VMA_RAW(rcon_va, 16u);
    const uint8_t *sbox = (const uint8_t *)RKOS_VMA_RAW(sbox_va, 256u);

    ctx[0x3Cu] = 16u * nr;

    if (nk == 6u) {
        uint32_t w0 = key[0];
        uint32_t w1 = key[1];
        uint32_t w2 = key[2];
        uint32_t w3 = key[3];
        uint32_t w4 = key[4];
        uint32_t w5 = key[5];
        ctx[0] = w0;
        ctx[1] = w1;
        ctx[2] = w2;
        ctx[3] = w3;
        ctx[4] = w4;
        ctx[5] = w5;
        uint32_t *dst = ctx + 6;
        int32_t rem = (int32_t)(4u * nr) - 2;
        do {
            uint32_t rc_byte = (uint32_t)(*rcon++);
            uint32_t next0 = dst[-6] ^ rkos_aes_subword(sbox, rkos_rotr32_aes(w5, 8u)) ^ rc_byte;
            w1 ^= next0;
            w2 ^= w1;
            w3 ^= w2;
            w4 ^= w3;
            w5 ^= w4;
            dst[0] = next0;
            dst[1] = w1;
            dst[2] = w2;
            dst[3] = w3;
            dst[4] = w4;
            dst[5] = w5;
            dst += 6;
            rem -= 6;
        } while (rem > 0);
    } else if (nk == 8u) {
        for (uint32_t idx = 0u; idx < 8u; idx++) {
            ctx[idx] = key[idx];
        }
        uint32_t *dst = ctx + 8;
        int32_t rem = (int32_t)(4u * nr) - 4;
        uint32_t prev = key[7];
        for (;;) {
            uint32_t rc_byte = (uint32_t)(*rcon++);
            uint32_t k0 = dst[-8] ^ rkos_aes_subword(sbox, rkos_rotr32_aes(prev, 8u)) ^ rc_byte;
            uint32_t k1 = k0 ^ dst[-7];
            uint32_t k2 = k1 ^ dst[-6];
            uint32_t k3 = k2 ^ dst[-5];
            dst[0] = k0;
            dst[1] = k1;
            dst[2] = k2;
            dst[3] = k3;
            dst += 4;
            rem -= 4;
            if (rem == 0) {
                break;
            }
            uint32_t k4 = dst[-8] ^ rkos_aes_subword(sbox, k3);
            uint32_t k5 = k4 ^ dst[-7];
            uint32_t k6 = k5 ^ dst[-6];
            prev = k6 ^ dst[-5];
            dst[0] = k4;
            dst[1] = k5;
            dst[2] = k6;
            dst[3] = prev;
            dst += 4;
            rem -= 4;
            if (rem <= 0) {
                break;
            }
        }
    } else {
        uint32_t w0 = key[0];
        uint32_t w1 = key[1];
        uint32_t w2 = key[2];
        uint32_t w3 = key[3];
        ctx[0] = w0;
        ctx[1] = w1;
        ctx[2] = w2;
        ctx[3] = w3;
        uint32_t *dst = ctx + 4;
        int32_t rem = (int32_t)(4u * nr);
        do {
            uint32_t rc_byte = (uint32_t)(*rcon++);
            w0 ^= rkos_aes_subword(sbox, rkos_rotr32_aes(w3, 8u)) ^ rc_byte;
            w1 ^= w0;
            w2 ^= w1;
            w3 ^= w2;
            dst[0] = w0;
            dst[1] = w1;
            dst[2] = w2;
            dst[3] = w3;
            dst += 4;
            rem -= 4;
        } while (rem > 0);
    }
}

/* @provenance RVA 0x1b80 sub_1B80 */
void rkos_aes_key_expand_enc128(const uint32_t *key, uint32_t *ctx)
{
    rkos_aes_key_expand_enc(key, ctx, 4u, 10u);
}

/* @provenance RVA 0x1b8c sub_1B8C */
void rkos_aes_key_expand_enc192(const uint32_t *key, uint32_t *ctx)
{
    rkos_aes_key_expand_enc(key, ctx, 6u, 12u);
}

/* @provenance RVA 0x1b98 sub_1B98 */
void rkos_aes_key_expand_enc256(const uint32_t *key, uint32_t *ctx)
{
    rkos_aes_key_expand_enc(key, ctx, 8u, 14u);
}

/* @provenance RVA 0x1ba4 sub_1BA4 */
void rkos_aes_key_expand_dec(const uint32_t *key, uint32_t *ctx, uint32_t nk, uint32_t nr)
{
    rkos_aes_key_expand_enc(key, ctx, nk, nr);

    uint32_t imc_va = *RKOS_VMA_PTR(const uint32_t *, 0x20000B94u);
    const uint32_t *imc = (const uint32_t *)RKOS_VMA_RAW(imc_va, 1024u);
    uint32_t *rk = ctx + 4;
    uint32_t count = 4u * (nr - 1u);

    for (uint32_t idx = 0u; idx < count; idx++) {
        uint32_t w = rk[idx];
        rk[idx] = imc[w & 0xFFu] ^
                  rkos_rotr32_aes(imc[(w >> 8) & 0xFFu], 24u) ^
                  rkos_rotr32_aes(imc[(w >> 16) & 0xFFu], 16u) ^
                  rkos_rotr32_aes(imc[(w >> 24) & 0xFFu], 8u);
    }
}

/* @provenance RVA 0x1c08 sub_1C08 */
uint32_t *rkos_aes_key_expand_dec128(uint32_t *key, uint32_t *ctx)
{
    rkos_aes_key_expand_dec(key, ctx, 4u, 10u);
    return key;
}

/* @provenance RVA 0x1c20 sub_1C20 */
uint32_t *rkos_aes_key_expand_dec192(uint32_t *key, uint32_t *ctx)
{
    rkos_aes_key_expand_dec(key, ctx, 6u, 12u);
    return key;
}

/* @provenance RVA 0x1c38 sub_1C38 */
uint32_t *rkos_aes_key_expand_dec256(uint32_t *key, uint32_t *ctx)
{
    rkos_aes_key_expand_dec(key, ctx, 8u, 14u);
    return key;
}

/* @provenance RVA 0x1c50 sub_1C50 */
int rkos_aes_setup_enc(const uint32_t *key, int key_len_or_bits, uint32_t *ctx)
{
    uint32_t klen = (key_len_or_bits > 32) ? ((uint32_t)key_len_or_bits >> 3)
                                           : (uint32_t)key_len_or_bits;
    if (klen == 16u) {
        rkos_aes_key_expand_enc128(key, ctx);
        return 0;
    }
    if (klen == 24u) {
        rkos_aes_key_expand_enc192(key, ctx);
        return 0;
    }
    if (klen == 32u) {
        rkos_aes_key_expand_enc256(key, ctx);
        return 0;
    }
    return -1;
}

/* @provenance RVA 0x1c8c sub_1C8C */
int rkos_aes_setup_dec(uint32_t *key, int key_len_or_bits, uint32_t *ctx)
{
    uint32_t klen = (key_len_or_bits > 32) ? ((uint32_t)key_len_or_bits >> 3)
                                           : (uint32_t)key_len_or_bits;
    if (klen == 16u) {
        (void)rkos_aes_key_expand_dec128(key, ctx);
        return 0;
    }
    if (klen == 24u) {
        (void)rkos_aes_key_expand_dec192(key, ctx);
        return 0;
    }
    if (klen == 32u) {
        (void)rkos_aes_key_expand_dec256(key, ctx);
        return 0;
    }
    return -1;
}

/* @provenance RVA 0x256c sub_256C */
int rkos_aes_encrypt_block(const uint32_t in_blk[4], uint32_t out_blk[4], const uint32_t *ctx)
{
    uint32_t rk_bytes = ctx[0x3Cu];
    if (rk_bytes != 0xA0u && rk_bytes != 0xC0u && rk_bytes != 0xE0u) {
        return -1;
    }

    uint32_t te_va = *RKOS_VMA_PTR(const uint32_t *, 0x20000BA0u);
    uint32_t sb_va = *RKOS_VMA_PTR(const uint32_t *, 0x20000B90u);
    const uint32_t *te0 = (const uint32_t *)RKOS_VMA_RAW(te_va, 1024u);
    const uint8_t *sbox = (const uint8_t *)RKOS_VMA_RAW(sb_va, 256u);

    uint32_t s0 = in_blk[0] ^ ctx[0];
    uint32_t s1 = in_blk[1] ^ ctx[1];
    uint32_t s2 = in_blk[2] ^ ctx[2];
    uint32_t s3 = in_blk[3] ^ ctx[3];

    const uint32_t *rk = ctx + 4;
    const uint32_t *rk_end = ctx + (rk_bytes / 4u);

    uint32_t k0 = rk[0];
    uint32_t k1 = rk[1];
    uint32_t k2 = rk[2];
    uint32_t k3 = rk[3];
    rk += 4;

    for (;;) {
        uint32_t t3 = k3 ^
                      rkos_rotr32_aes(te0[(s0 >> 8) & 0xFFu], 24u) ^
                      rkos_rotr32_aes(te0[(s1 >> 16) & 0xFFu], 16u) ^
                      rkos_rotr32_aes(te0[(s2 >> 24) & 0xFFu], 8u) ^
                      te0[s3 & 0xFFu];
        uint32_t t2 = k2 ^
                      rkos_rotr32_aes(te0[(s0 >> 16) & 0xFFu], 16u) ^
                      rkos_rotr32_aes(te0[(s1 >> 24) & 0xFFu], 8u) ^
                      te0[s2 & 0xFFu] ^
                      rkos_rotr32_aes(te0[(s3 >> 8) & 0xFFu], 24u);
        uint32_t t1 = k1 ^
                      rkos_rotr32_aes(te0[(s0 >> 24) & 0xFFu], 8u) ^
                      te0[s1 & 0xFFu] ^
                      rkos_rotr32_aes(te0[(s2 >> 8) & 0xFFu], 24u) ^
                      rkos_rotr32_aes(te0[(s3 >> 16) & 0xFFu], 16u);
        s0 = k0 ^
             te0[s0 & 0xFFu] ^
             rkos_rotr32_aes(te0[(s1 >> 8) & 0xFFu], 24u) ^
             rkos_rotr32_aes(te0[(s2 >> 16) & 0xFFu], 16u) ^
             rkos_rotr32_aes(te0[(s3 >> 24) & 0xFFu], 8u);
        s1 = t1;
        s2 = t2;
        s3 = t3;

        k0 = rk[0];
        k1 = rk[1];
        k2 = rk[2];
        k3 = rk[3];
        if (rk == rk_end) {
            break;
        }
        rk += 4;
    }

    out_blk[0] = k0 ^
                 ((uint32_t)sbox[s0 & 0xFFu]) ^
                 ((uint32_t)sbox[(s1 >> 8) & 0xFFu] << 8) ^
                 ((uint32_t)sbox[(s2 >> 16) & 0xFFu] << 16) ^
                 ((uint32_t)sbox[(s3 >> 24) & 0xFFu] << 24);
    out_blk[1] = k1 ^
                 ((uint32_t)sbox[(s0 >> 24) & 0xFFu] << 24) ^
                 ((uint32_t)sbox[s1 & 0xFFu]) ^
                 ((uint32_t)sbox[(s2 >> 8) & 0xFFu] << 8) ^
                 ((uint32_t)sbox[(s3 >> 16) & 0xFFu] << 16);
    out_blk[2] = k2 ^
                 ((uint32_t)sbox[(s0 >> 16) & 0xFFu] << 16) ^
                 ((uint32_t)sbox[(s1 >> 24) & 0xFFu] << 24) ^
                 ((uint32_t)sbox[s2 & 0xFFu]) ^
                 ((uint32_t)sbox[(s3 >> 8) & 0xFFu] << 8);
    out_blk[3] = k3 ^
                 ((uint32_t)sbox[(s0 >> 8) & 0xFFu] << 8) ^
                 ((uint32_t)sbox[(s1 >> 16) & 0xFFu] << 16) ^
                 ((uint32_t)sbox[(s2 >> 24) & 0xFFu] << 24) ^
                 ((uint32_t)sbox[s3 & 0xFFu]);
    return 0;
}

/* @provenance RVA 0x1e50 sub_1E50 */
int rkos_aes_decrypt_block(const uint32_t in_blk[4], uint32_t out_blk[4], const uint32_t *ctx)
{
    uint32_t rk_bytes = ctx[0x3Cu];
    if (rk_bytes != 0xA0u && rk_bytes != 0xC0u && rk_bytes != 0xE0u) {
        return -1;
    }

    uint32_t td_va = *RKOS_VMA_PTR(const uint32_t *, 0x20000B98u);
    uint32_t isb_va = *RKOS_VMA_PTR(const uint32_t *, 0x20000B9Cu);
    const uint32_t *td0 = (const uint32_t *)RKOS_VMA_RAW(td_va, 1024u);
    const uint8_t *isbox = (const uint8_t *)RKOS_VMA_RAW(isb_va, 256u);

    const uint32_t *rk_top = ctx + (rk_bytes / 4u);
    uint32_t s0 = in_blk[0] ^ rk_top[0];
    uint32_t s1 = in_blk[1] ^ rk_top[1];
    uint32_t s2 = in_blk[2] ^ rk_top[2];
    uint32_t s3 = in_blk[3] ^ rk_top[3];

    const uint32_t *rk = rk_top - 4;
    uint32_t k0 = rk[0];
    uint32_t k1 = rk[1];
    uint32_t k2 = rk[2];
    uint32_t k3 = rk[3];
    rk -= 4;

    for (;;) {
        uint32_t t3 = k3 ^
                      rkos_rotr32_aes(td0[(s0 >> 24) & 0xFFu], 8u) ^
                      rkos_rotr32_aes(td0[(s1 >> 16) & 0xFFu], 16u) ^
                      rkos_rotr32_aes(td0[(s2 >> 8) & 0xFFu], 24u) ^
                      td0[s3 & 0xFFu];
        uint32_t t1 = k1 ^
                      rkos_rotr32_aes(td0[(s0 >> 8) & 0xFFu], 24u) ^
                      td0[s1 & 0xFFu] ^
                      rkos_rotr32_aes(td0[(s2 >> 24) & 0xFFu], 8u) ^
                      rkos_rotr32_aes(td0[(s3 >> 16) & 0xFFu], 16u);
        uint32_t t2 = k2 ^
                      rkos_rotr32_aes(td0[(s0 >> 16) & 0xFFu], 16u) ^
                      rkos_rotr32_aes(td0[(s1 >> 8) & 0xFFu], 24u) ^
                      td0[s2 & 0xFFu] ^
                      rkos_rotr32_aes(td0[(s3 >> 24) & 0xFFu], 8u);
        s0 = k0 ^
             td0[s0 & 0xFFu] ^
             rkos_rotr32_aes(td0[(s1 >> 24) & 0xFFu], 8u) ^
             rkos_rotr32_aes(td0[(s2 >> 16) & 0xFFu], 16u) ^
             rkos_rotr32_aes(td0[(s3 >> 8) & 0xFFu], 24u);
        s1 = t1;
        s2 = t2;
        s3 = t3;

        k0 = rk[0];
        k1 = rk[1];
        k2 = rk[2];
        k3 = rk[3];
        if (rk == ctx) {
            break;
        }
        rk -= 4;
    }

    out_blk[0] = k0 ^
                 ((uint32_t)isbox[s0 & 0xFFu]) ^
                 ((uint32_t)isbox[(s1 >> 24) & 0xFFu] << 24) ^
                 ((uint32_t)isbox[(s2 >> 16) & 0xFFu] << 16) ^
                 ((uint32_t)isbox[(s3 >> 8) & 0xFFu] << 8);
    out_blk[1] = k1 ^
                 ((uint32_t)isbox[(s0 >> 8) & 0xFFu] << 8) ^
                 ((uint32_t)isbox[s1 & 0xFFu]) ^
                 ((uint32_t)isbox[(s2 >> 24) & 0xFFu] << 24) ^
                 ((uint32_t)isbox[(s3 >> 16) & 0xFFu] << 16);
    out_blk[2] = k2 ^
                 ((uint32_t)isbox[(s0 >> 16) & 0xFFu] << 16) ^
                 ((uint32_t)isbox[(s1 >> 8) & 0xFFu] << 8) ^
                 ((uint32_t)isbox[s2 & 0xFFu]) ^
                 ((uint32_t)isbox[(s3 >> 24) & 0xFFu] << 24);
    out_blk[3] = k3 ^
                 ((uint32_t)isbox[(s0 >> 24) & 0xFFu] << 24) ^
                 ((uint32_t)isbox[(s1 >> 16) & 0xFFu] << 16) ^
                 ((uint32_t)isbox[(s2 >> 8) & 0xFFu] << 8) ^
                 ((uint32_t)isbox[s3 & 0xFFu]);
    return 0;
}

/* @provenance RVA 0x1880 sub_1880 */
void rkos_aes_cbc_decrypt(const uint32_t *in_blks, const uint32_t iv[4],
                          int nblocks, uint32_t *out_blks, const uint32_t *ctx)
{
    uint32_t chain0 = iv[0];
    uint32_t chain1 = iv[1];
    uint32_t chain2 = iv[2];
    uint32_t chain3 = iv[3];

    for (int idx = 0; idx < nblocks; idx++) {
        uint32_t ct0 = in_blks[0];
        uint32_t ct1 = in_blks[1];
        uint32_t ct2 = in_blks[2];
        uint32_t ct3 = in_blks[3];
        uint32_t dec[4];
        (void)rkos_aes_decrypt_block(in_blks, dec, ctx);
        out_blks[0] = dec[0] ^ chain0;
        out_blks[1] = dec[1] ^ chain1;
        out_blks[2] = dec[2] ^ chain2;
        out_blks[3] = dec[3] ^ chain3;
        chain0 = ct0;
        chain1 = ct1;
        chain2 = ct2;
        chain3 = ct3;
        in_blks += 4;
        out_blks += 4;
    }
}

/* @provenance RVA 0x190c sub_190C */
void rkos_aes_cbc_encrypt(const uint32_t *in_blks, const uint32_t iv[4],
                          int nblocks, uint32_t *out_blks, const uint32_t *ctx)
{
    uint32_t state[4] = { iv[0], iv[1], iv[2], iv[3] };
    for (int idx = 0; idx < nblocks; idx++) {
        state[0] ^= in_blks[0];
        state[1] ^= in_blks[1];
        state[2] ^= in_blks[2];
        state[3] ^= in_blks[3];
        (void)rkos_aes_encrypt_block(state, state, ctx);
        out_blks[0] = state[0];
        out_blks[1] = state[1];
        out_blks[2] = state[2];
        out_blks[3] = state[3];
        in_blks += 4;
        out_blks += 4;
    }
}

/* @provenance RVA 0x1dd4 sub_1DD4 */
void rkos_aes_ctr_crypt(const uint32_t *in_blks, uint32_t *out_blks,
                        uint32_t byte_len, uint32_t ctr_blk[4], const uint32_t *ctx)
{
    int32_t nblocks = (int32_t)(byte_len >> 4);
    do {
        uint32_t ks[4];
        (void)rkos_aes_encrypt_block(ctr_blk, ks, ctx);
        uint32_t p0 = in_blks[0];
        uint32_t p1 = in_blks[1];
        uint32_t p2 = in_blks[2];
        uint32_t p3 = in_blks[3];
        out_blks[0] = p0 ^ ks[0];
        out_blks[1] = p1 ^ ks[1];
        out_blks[2] = p2 ^ ks[2];
        out_blks[3] = p3 ^ ks[3];

        uint64_t ctr_be = (((uint64_t)rkos_bswap32(ctr_blk[2])) << 32) |
                          (uint64_t)rkos_bswap32(ctr_blk[3]);
        ctr_be++;
        ctr_blk[2] = rkos_bswap32((uint32_t)(ctr_be >> 32));
        ctr_blk[3] = rkos_bswap32((uint32_t)ctr_be);

        in_blks += 4;
        out_blks += 4;
        nblocks--;
    } while (nblocks > 0);
}

static inline uint64_t rkos_ccm_len_mask(int len_field_size)
{
    if (len_field_size <= 0) {
        return 0ULL;
    }
    if (len_field_size >= 8) {
        return ~0ULL;
    }
    return (1ULL << (8u * (uint32_t)len_field_size)) - 1ULL;
}

/* @provenance RVA 0x1cc8 sub_1CC8 */
void rkos_aes_ccm_decrypt(const uint32_t *in_blks, uint32_t *out_blks,
                          uint32_t cbc_mac[4], int nblocks,
                          const uint32_t *ctx, uint32_t ctr_blk[4], int len_field_size)
{
    uint64_t mask = rkos_ccm_len_mask(len_field_size);
    uint64_t ctr_val = (((uint64_t)rkos_bswap32(ctr_blk[2])) << 32) |
                       (uint64_t)rkos_bswap32(ctr_blk[3]);

    do {
        ctr_val = (ctr_val & ~mask) | ((ctr_val + 1ULL) & mask);
        ctr_blk[2] = rkos_bswap32((uint32_t)(ctr_val >> 32));
        ctr_blk[3] = rkos_bswap32((uint32_t)ctr_val);

        uint32_t ks[4];
        (void)rkos_aes_encrypt_block(ctr_blk, ks, ctx);

        uint32_t pt0 = ks[0] ^ in_blks[0];
        uint32_t pt1 = ks[1] ^ in_blks[1];
        uint32_t pt2 = ks[2] ^ in_blks[2];
        uint32_t pt3 = ks[3] ^ in_blks[3];
        out_blks[0] = pt0;
        out_blks[1] = pt1;
        out_blks[2] = pt2;
        out_blks[3] = pt3;

        uint32_t mac_in[4] = {
            pt0 ^ cbc_mac[0],
            pt1 ^ cbc_mac[1],
            pt2 ^ cbc_mac[2],
            pt3 ^ cbc_mac[3],
        };
        (void)rkos_aes_encrypt_block(mac_in, cbc_mac, ctx);

        in_blks += 4;
        out_blks += 4;
        nblocks--;
    } while (nblocks > 0);
}

/* @provenance RVA 0x2468 sub_2468 */
void rkos_aes_ccm_encrypt(const uint32_t *in_blks, uint32_t *out_blks,
                          uint32_t cbc_mac[4], int nblocks,
                          const uint32_t *ctx, uint32_t ctr_blk[4], int len_field_size)
{
    uint64_t mask = rkos_ccm_len_mask(len_field_size);
    uint64_t ctr_val = (((uint64_t)rkos_bswap32(ctr_blk[2])) << 32) |
                       (uint64_t)rkos_bswap32(ctr_blk[3]);

    do {
        ctr_val = (ctr_val & ~mask) | ((ctr_val + 1ULL) & mask);
        ctr_blk[2] = rkos_bswap32((uint32_t)(ctr_val >> 32));
        ctr_blk[3] = rkos_bswap32((uint32_t)ctr_val);

        uint32_t ks[4];
        (void)rkos_aes_encrypt_block(ctr_blk, ks, ctx);

        uint32_t pt0 = in_blks[0];
        uint32_t pt1 = in_blks[1];
        uint32_t pt2 = in_blks[2];
        uint32_t pt3 = in_blks[3];
        out_blks[0] = ks[0] ^ pt0;
        out_blks[1] = ks[1] ^ pt1;
        out_blks[2] = ks[2] ^ pt2;
        out_blks[3] = ks[3] ^ pt3;

        uint32_t mac_in[4] = {
            cbc_mac[0] ^ pt0,
            cbc_mac[1] ^ pt1,
            cbc_mac[2] ^ pt2,
            cbc_mac[3] ^ pt3,
        };
        (void)rkos_aes_encrypt_block(mac_in, cbc_mac, ctx);

        in_blks += 4;
        out_blks += 4;
        nblocks--;
    } while (nblocks > 0);
}

/* --- Multi-Precision BigNum (`ccn_*` at 0x2080..0x2466) --- */

/* @provenance RVA 0x2080 sub_2080 */
uint32_t rkos_ccn_add(uint32_t count, uint32_t *dst, const uint32_t *lhs, const uint32_t *rhs)
{
    uint64_t carry = 0ULL;
    for (uint32_t idx = 0u; idx < count; idx++) {
        uint64_t sum = (uint64_t)lhs[idx] + (uint64_t)rhs[idx] + carry;
        dst[idx] = (uint32_t)sum;
        carry = sum >> 32;
    }
    return (uint32_t)carry;
}

/* @provenance RVA 0x20ec sub_20EC */
uint32_t rkos_ccn_add1(uint32_t count, uint32_t *dst, const uint32_t *src, uint32_t val)
{
    if (count == 0u) {
        return val;
    }
    uint64_t carry = (uint64_t)val;
    for (uint32_t idx = 0u; idx < count; idx++) {
        uint64_t sum = (uint64_t)src[idx] + carry;
        dst[idx] = (uint32_t)sum;
        carry = sum >> 32;
    }
    return (uint32_t)carry;
}

/* @provenance RVA 0x2118 sub_2118 */
uint32_t rkos_ccn_addmul1(uint32_t count, uint32_t *acc, const uint32_t *src, uint32_t mul)
{
    uint64_t carry = 0ULL;
    for (uint32_t idx = 0u; idx < count; idx++) {
        uint64_t prod = (uint64_t)src[idx] * (uint64_t)mul + (uint64_t)acc[idx] + carry;
        acc[idx] = (uint32_t)prod;
        carry = prod >> 32;
    }
    return (uint32_t)carry;
}

/* @provenance RVA 0x2204 sub_2204 */
int rkos_ccn_cmp(uint32_t count, const uint32_t *lhs, const uint32_t *rhs)
{
    int res = 0;
    for (uint32_t idx = 0u; idx < count; idx++) {
        if (lhs[idx] > rhs[idx]) {
            res = 1;
        } else if (lhs[idx] < rhs[idx]) {
            res = -1;
        }
    }
    return res;
}

/* @provenance RVA 0x2234 sub_2234 */
uint32_t rkos_ccn_sub1(uint32_t count, uint32_t *dst, const uint32_t *src, uint32_t val)
{
    if (count == 0u) {
        return val;
    }
    uint64_t borrow = (uint64_t)val;
    for (uint32_t idx = 0u; idx < count; idx++) {
        uint64_t cur = (uint64_t)src[idx];
        uint64_t diff = cur - borrow;
        dst[idx] = (uint32_t)diff;
        borrow = (cur < borrow) ? 1ULL : 0ULL;
    }
    return (uint32_t)borrow;
}

/* @provenance RVA 0x2264 sub_2264 */
uint32_t rkos_ccn_mul1(uint32_t count, uint32_t *dst, const uint32_t *src, uint32_t mul)
{
    uint64_t carry = 0ULL;
    for (uint32_t idx = 0u; idx < count; idx++) {
        uint64_t prod = (uint64_t)src[idx] * (uint64_t)mul + carry;
        dst[idx] = (uint32_t)prod;
        carry = prod >> 32;
    }
    return (uint32_t)carry;
}

/* @provenance RVA 0x2304 sub_2304 */
uint32_t rkos_ccn_n(uint32_t count, const uint32_t *src)
{
    uint32_t active = 0u;
    for (uint32_t idx = 0u; idx < count; idx++) {
        if (src[idx] != 0u) {
            active = idx + 1u;
        }
    }
    return active;
}

/* @provenance RVA 0x2328 sub_2328 */
void rkos_ccn_set(uint32_t count, uint32_t *dst, const uint32_t *src)
{
    for (uint32_t idx = 0u; idx < count; idx++) {
        dst[idx] = src[idx];
    }
}

/* @provenance RVA 0x235c sub_235C */
uint32_t rkos_ccn_sub(uint32_t count, uint32_t *dst, const uint32_t *lhs, const uint32_t *rhs)
{
    uint64_t borrow = 0ULL;
    for (uint32_t idx = 0u; idx < count; idx++) {
        uint64_t lval = (uint64_t)lhs[idx];
        uint64_t subtrahend = (uint64_t)rhs[idx] + borrow;
        dst[idx] = (uint32_t)(lval - subtrahend);
        borrow = (lval < subtrahend) ? 1ULL : 0ULL;
    }
    return (uint32_t)borrow;
}

/* @provenance RVA 0x23d0 sub_23D0 */
void rkos_ccn_shift_right(uint32_t count, uint32_t *dst, const uint32_t *src, uint32_t shift_bits)
{
    if (count == 0u) {
        return;
    }
    uint32_t sh = shift_bits & 31u;
    if (sh == 0u) {
        for (uint32_t idx = 0u; idx < count; idx++) {
            dst[idx] = src[idx];
        }
        return;
    }
    uint32_t lsh = 32u - sh;
    for (uint32_t idx = 0u; idx + 1u < count; idx++) {
        uint32_t lo = src[idx];
        uint32_t hi = src[idx + 1u];
        dst[idx] = (lo >> sh) | (hi << lsh);
    }
    dst[count - 1u] = src[count - 1u] >> sh;
}

/* --- NIST SP 800-90A HMAC-DRBG & CoreCrypto / AAPCS Helpers --- */

static inline void rkos_secure_zero_buf(void *buf, size_t len)
{
#ifdef RKOS_HOST
    (void)rkos_memset(buf, 0, len);
#else
    uint32_t fn_va = *RKOS_VMA_PTR(const uint32_t *, 0x000160C0u);
    (void)rkos_call_fn3(0x000160C0u, fn_va, (uint32_t)(uintptr_t)buf, 0u, (uint32_t)len);
#endif
}

/* @provenance RVA 0x118b4 sub_118B4 */
uint32_t rkos_ref_mutex_dec_unlock(uint32_t obj_va)
{
    uint32_t *cnt_ptr = RKOS_VMA_PTR(uint32_t *, obj_va + 8u);
    uint32_t next_cnt = *cnt_ptr - 1u;
    *cnt_ptr = next_cnt;
    if (next_cnt != 0u) {
        return obj_va;
    }
    return (uint32_t)mutex_unlock(obj_va + 4u);
}

/* @provenance RVA 0x118c4 sub_118C4 */
void rkos_crypto_fips_abort(void)
{
    rkos_crypto_panic_str(NULL);
}

/* @provenance RVA 0x125cc sub_125CC */
void rkos_drbg_hmac_update(uint32_t drbg_va, uint32_t num_inputs, ...)
{
    uint8_t *drbg = (uint8_t *)RKOS_VMA_RAW(drbg_va, 0x90u);
    uint32_t custom_va = *(const uint32_t *)(const void *)drbg;
    uint32_t di_va = *RKOS_VMA_PTR(const uint32_t *, custom_va);
    const rkos_ccdigest_info_t *di = RKOS_VMA_PTR(const rkos_ccdigest_info_t *, di_va);
    uint32_t out_sz = di->output_size;

    uint8_t *key_k = drbg + 4u;
    uint8_t *val_v = drbg + 0x44u;
    uint8_t hctx[384];
    (void)rkos_memset(hctx, 0, sizeof(hctx));

    size_t total_input = 0u;
    for (uint32_t pass = 0u; pass <= 1u; pass++) {
        uint8_t sep = (uint8_t)pass;
        (void)rkos_cchmac_init(di_va, hctx, out_sz, key_k);
        rkos_ccdigest_update(di_va, hctx, out_sz, val_v);
        rkos_ccdigest_update(di_va, hctx, 1u, &sep);

        va_list ap;
        va_start(ap, num_inputs);
        for (uint32_t idx = 0u; idx < num_inputs; idx++) {
            size_t in_len = va_arg(ap, size_t);
            const void *in_ptr = va_arg(ap, const void *);
            rkos_ccdigest_update(di_va, hctx, in_len, in_ptr);
            total_input += in_len;
        }
        va_end(ap);

        rkos_cchmac_final(di_va, hctx, key_k);
        rkos_cchmac_oneshot(di_va, out_sz, key_k, out_sz, val_v, val_v);
        if (total_input == 0u) {
            break;
        }
    }

    size_t scrub_sz = ((di->state_size + ((di->block_size + di->state_size + 0x13u) & ~7u) + 7u) & ~7u);
    rkos_secure_zero_buf(hctx, scrub_sz);
}

/* @provenance RVA 0x12374 sub_12374 */
int rkos_drbg_hmac_init(uint32_t drbg_info_va, uint32_t drbg_va,
                        uint32_t entropy_len, const void *entropy,
                        uint32_t nonce_len, const void *nonce,
                        uint32_t ps_len, const void *ps)
{
    uint32_t custom_va = *RKOS_VMA_PTR(const uint32_t *, drbg_info_va + 0x14u);
    uint8_t *drbg = (uint8_t *)RKOS_VMA_RAW(drbg_va, 0x90u);
    *(uint32_t *)(void *)drbg = custom_va;

    if (entropy_len > 0x10000u) {
        return -63;
    }
    uint32_t di_va = *RKOS_VMA_PTR(const uint32_t *, custom_va);
    uint32_t out_sz = *RKOS_VMA_PTR(const uint32_t *, di_va);
    if (out_sz > 0x40u || entropy_len < (out_sz >> 1) || ps_len > 0x10000u) {
        return -63;
    }

    (void)rkos_memset_chk(drbg + 4u, 0, out_sz, 0x40u);
    (void)rkos_memset_chk(drbg + 0x44u, 1, out_sz, 0x40u);
    rkos_drbg_hmac_update(drbg_va, 3u,
                          (size_t)entropy_len, entropy,
                          (size_t)nonce_len, nonce,
                          (size_t)ps_len, ps);
    *(uint32_t *)(void *)(drbg + 0x8Cu) = 0u;
    *(uint32_t *)(void *)(drbg + 0x88u) = 1u;
    return 0;
}

/* @provenance RVA 0x12526 sub_12526 */
int rkos_drbg_hmac_reseed_body(uint32_t drbg_va,
                               uint32_t entropy_len, const void *entropy,
                               uint32_t add_len, const void *add_in)
{
    if (add_len > 0x10000u) {
        return -63;
    }
    uint8_t *drbg = (uint8_t *)RKOS_VMA_RAW(drbg_va, 0x90u);
    uint32_t custom_va = *(const uint32_t *)(const void *)drbg;
    uint32_t di_va = *RKOS_VMA_PTR(const uint32_t *, custom_va);
    uint32_t out_sz = *RKOS_VMA_PTR(const uint32_t *, di_va);
    if (entropy_len < (out_sz >> 1)) {
        return -63;
    }

    rkos_drbg_hmac_update(drbg_va, 2u,
                          (size_t)entropy_len, entropy,
                          (size_t)add_len, add_in);
    *(uint32_t *)(void *)(drbg + 0x8Cu) = 0u;
    *(uint32_t *)(void *)(drbg + 0x88u) = 1u;
    return 0;
}

/* @provenance RVA 0x12520 sub_12520 */
int rkos_drbg_hmac_reseed(uint32_t drbg_va,
                          uint32_t entropy_len, const void *entropy,
                          uint32_t add_len, const void *add_in)
{
    if (entropy_len > 0x10000u) {
        return -63;
    }
    return rkos_drbg_hmac_reseed_body(drbg_va, entropy_len, entropy, add_len, add_in);
}

/* @provenance RVA 0x123f4 __memcpy_chk */
int rkos_drbg_hmac_generate(uint32_t drbg_va,
                            uint32_t data_len, void *data_out,
                            uint32_t add_len, const void *add_in)
{
    uint8_t *drbg = (uint8_t *)RKOS_VMA_RAW(drbg_va, 0x90u);
    uint32_t custom_va = *(const uint32_t *)(const void *)drbg;
    uint32_t di_va = *RKOS_VMA_PTR(const uint32_t *, custom_va);
    uint32_t out_sz = *RKOS_VMA_PTR(const uint32_t *, di_va);
    uint8_t prev_v[64];
    (void)rkos_memset(prev_v, 0, sizeof(prev_v));

    int status = -63;
    if (data_len > 0x10000u || add_len > 0x10000u) {
        rkos_secure_zero_buf(prev_v, out_sz);
        return status;
    }

    uint32_t strict_reseed = *RKOS_VMA_PTR(const uint32_t *, custom_va + 4u);
    uint64_t counter = (((uint64_t)*(const uint32_t *)(const void *)(drbg + 0x8Cu)) << 32) |
                       (uint64_t)*(const uint32_t *)(const void *)(drbg + 0x88u);
    if (strict_reseed != 0u && counter > 0x0001000000000000ULL) {
        rkos_secure_zero_buf(prev_v, out_sz);
        return -62;
    }

    if (add_len != 0u) {
        rkos_drbg_hmac_update(drbg_va, 1u, (size_t)add_len, add_in);
    }

    uint8_t *key_k = drbg + 4u;
    uint8_t *val_v = drbg + 0x44u;
    uint8_t *dst = (uint8_t *)data_out;
    uint32_t rem = data_len;

    while (rem != 0u) {
        if (out_sz >= 0x41u) {
            rtkit_panic("memcpy_chk");
        }
        (void)memcpy_thunk(prev_v, val_v, out_sz);
        rkos_cchmac_oneshot(di_va, out_sz, key_k, out_sz, val_v, val_v);
        if (rkos_cc_cmp_safe(out_sz, prev_v, val_v) == 0) {
            (void)rkos_drbg_clear(drbg_va);
            rkos_crypto_fips_abort();
        }
        uint32_t chunk = (rem < out_sz) ? rem : out_sz;
        (void)memcpy_thunk(dst, val_v, chunk);
        dst += chunk;
        rem -= chunk;
    }

    rkos_drbg_hmac_update(drbg_va, 1u, (size_t)add_len, add_in);
    counter = (((uint64_t)*(const uint32_t *)(const void *)(drbg + 0x8Cu)) << 32) |
              (uint64_t)*(const uint32_t *)(const void *)(drbg + 0x88u);
    counter++;
    *(uint32_t *)(void *)(drbg + 0x88u) = (uint32_t)counter;
    *(uint32_t *)(void *)(drbg + 0x8Cu) = (uint32_t)(counter >> 32);

    rkos_secure_zero_buf(prev_v, out_sz);
    return 0;
}

/* --- AAPCS Utility Helpers in 0x127B0..0x12ACE --- */

/* @provenance RVA 0x127b0 sub_127B0 */
uint32_t rkos_take_and_dec_field58(uint32_t obj_va)
{
    uint32_t *slot = RKOS_VMA_PTR(uint32_t *, obj_va + 0x58u);
    uint32_t prev = *slot;
    *slot = 0u;
    return prev - 1u;
}

/* @provenance RVA 0x127c8 sub_127C8 */
void *rkos_bzero32(void *dst)
{
    return rkos_memset(dst, 0, 0x20u);
}

/* @provenance RVA 0x128fe sub_128FE */
void *rkos_bzero_len(void *dst, int unused_arg, size_t len)
{
    (void)unused_arg;
    return rkos_memset(dst, 0, len);
}

/* @provenance RVA 0x129b6 sub_129B6 */
uint32_t rkos_bitmap_shift_word(uint32_t bit_idx, uint32_t unused_arg, const uint32_t *words)
{
    (void)unused_arg;
    return words[bit_idx >> 5] >> (bit_idx & 31u);
}

/* @provenance RVA 0x12a1a outlined_sub_12A1A */
uint64_t rkos_umul_u32_u64(uint32_t unused_arg, uint32_t mul32, uint64_t val64)
{
    (void)unused_arg;
    return val64 * (uint64_t)mul32;
}

/* @provenance RVA 0x12a64 sub_12A64 */
int rkos_is_zero_u32(uint32_t val)
{
    return (val == 0u) ? 1 : 0;
}

/* @provenance RVA 0x12a96 sub_12A96 */
void *rkos_memcpy16(void *dst, const void *src)
{
    return memcpy_thunk(dst, src, 0x10u);
}

/* @provenance RVA 0x12a9c sub_12A9C */
uint32_t rkos_pack_low2_and_idx(uint32_t flags, uint32_t unused_arg, uint32_t idx)
{
    (void)unused_arg;
    return (flags & 3u) | (idx << 2);
}

/* @provenance RVA 0x12ab8 sub_12AB8 */
int rkos_const_256(void)
{
    return 0x100;
}

/* @provenance RVA 0x12ace sub_12ACE */
uint32_t rkos_deref_table_slot(const uint32_t *table_ptr_va, uint32_t unused_arg,
                               uint32_t idx, uint32_t *out_val)
{
    (void)unused_arg;
    uint32_t table_va = *table_ptr_va;
    uint32_t val = *RKOS_VMA_PTR(const uint32_t *, table_va + 4u * idx);
    *out_val = val;
    return val;
}
