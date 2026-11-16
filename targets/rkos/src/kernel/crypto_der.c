#include "rkos/rkos.h"

/*
 * Reconstructed Image4 ASN.1 DER parser (`0x057C`, `0x0582`) and Apple
 * corecrypto SHA-256 (`0x11DC8`), SHA-512/384 (`0x11F38`), `ccdigest`
 * (`0x11910`, `0x119AC`, `0x11B04`, `0x1220C`), `cchmac` (`0x11BD4`, `0x11C5C`,
 * `0x11CAE`), and HMAC-DRBG state helpers (`0x118D0`, `0x118E6`, `0x11DB8`,
 * `0x12354`, `0x1256C`, `0x125A8`).
 *
 * Firmware detail from `rkos.asm`: both SHA-256 (`0x11DC8`) and SHA-512
 * (`0x11F38`) share the 80x64-bit `K512` constant table in `ROM_RODATA` at
 * `0x00016180`, because the upper 32 bits (`offset +4` in little-endian) of the
 * first 64 `K512` constants equal the 64 `K256` round constants.
 */

#ifdef RKOS_HOST
#define RKOS_DER_EARLY
#else
#define RKOS_DER_EARLY __attribute__((section(".text.early")))
#endif

static inline uint32_t rkos_rotr32(uint32_t x, unsigned int n)
{
    return (x >> n) | (x << (32u - n));
}

static inline uint64_t rkos_rotr64(uint64_t x, unsigned int n)
{
    return (x >> n) | (x << (64u - n));
}

static inline uint32_t rkos_load_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | ((uint32_t)p[3]);
}

static inline void rkos_store_be32(uint32_t v, uint8_t *p)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline uint64_t rkos_load_be64(const uint8_t *p)
{
    return ((uint64_t)rkos_load_be32(p) << 32) | (uint64_t)rkos_load_be32(p + 4);
}

/* @shim RVA 0x582 sub_582 (pending_c: not shown equivalent to raw) */
RKOS_DER_EARLY int rkos_der_decode_ex(const rkos_der_input_t *in_slice,
                                      rkos_der_tlv_t *out_tlv,
                                      uint32_t *raw_len_out)
{
    uint32_t in_len = in_slice->length;
    uint32_t base_va = in_slice->ptr_va;
    if (in_len < 2u || base_va == 0xFFFFFFFFu) {
        return 3;
    }

    const uint8_t *buf = (const uint8_t *)RKOS_VMA_RAW(base_va, in_len);
    uint8_t id_byte = buf[0];
    uint32_t tag_lo = (uint32_t)(id_byte & 0x1Fu);
    uint32_t tag_hi = 0u;
    uint32_t pos = 1u;

    if (tag_lo == 0x1Fu) {
        uint8_t first_sub = buf[pos];
        if (first_sub == 0x80u || first_sub < 0x1Fu) {
            return 3;
        }
        tag_lo = 0u;
        for (;;) {
            if (pos >= in_len) {
                return 3;
            }
            uint8_t b = buf[pos++];
            uint32_t next_hi = (tag_hi << 7) | (tag_lo >> 25);
            tag_lo = (tag_lo << 7) | (uint32_t)(b & 0x7Fu);
            if ((b & 0x80u) == 0u) {
                tag_hi = next_hi;
                break;
            }
            tag_hi = next_hi;
            if ((in_len - pos) < 1u || (tag_hi >> 25) != 0u) {
                return 3;
            }
        }
        if ((tag_hi >> 22) != 0u || pos >= in_len) {
            return 3;
        }
    }

    out_tlv->tag_number_lo = tag_lo;
    out_tlv->tag_hi_and_cls = tag_hi | (((uint32_t)(id_byte & 0xE0u)) << 24);

    uint8_t len_byte = buf[pos++];
    uint32_t rem = in_len - pos;

    if ((len_byte & 0x80u) == 0u) {
        uint32_t val_len = (uint32_t)len_byte;
        if (raw_len_out == NULL && rem < val_len) {
            return 3;
        }
        uint32_t clamped = (rem < val_len) ? rem : val_len;
        out_tlv->content_va = base_va + pos;
        out_tlv->content_len = clamped;
        if (raw_len_out != NULL) {
            *raw_len_out = val_len;
        }
        return 0;
    }

    uint32_t len_octets = (uint32_t)(len_byte & 0x7Fu);
    if (len_octets == 0u || len_octets > 4u || rem < len_octets) {
        return 3;
    }
    if (buf[pos] == 0u) {
        return 3;
    }

    uint32_t long_len = 0u;
    for (uint32_t i = 0u; i < len_octets; i++) {
        long_len = (long_len << 8) | (uint32_t)buf[pos++];
    }
    rem = in_len - pos;

    if (long_len < 0x80u || (raw_len_out == NULL && long_len > rem)) {
        return 3;
    }

    uint32_t clamped = (long_len < rem) ? long_len : rem;
    out_tlv->content_va = base_va + pos;
    out_tlv->content_len = clamped;
    if (raw_len_out != NULL) {
        *raw_len_out = long_len;
    }
    return 0;
}

/* @provenance RVA 0x57c sub_57C */
RKOS_DER_EARLY int rkos_der_decode(const rkos_der_input_t *in_slice, rkos_der_tlv_t *out_tlv)
{
    return rkos_der_decode_ex(in_slice, out_tlv, NULL);
}

/* @provenance RVA 0x118d0 sub_118D0 */
void rkos_sec_init_flag(uint32_t *out_zero)
{
    uint8_t *flag = (uint8_t *)RKOS_VMA_RAW(0x2001DDE0u, 1u);
    if (*flag == 0u) {
        *flag = 1u;
    }
    *out_zero = 0u;
}

/* @provenance RVA 0x118e6 sub_118E6 */
int rkos_cc_cmp_safe(size_t len, const void *a, const void *b)
{
    if (len == 0u) {
        return 1;
    }
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    uint8_t acc = 0u;

    while (len > 0u) {
        len--;
        acc |= (uint8_t)(pa[len] ^ pb[len]);
    }
    return (acc != 0u) ? 1 : 0;
}

/* @provenance RVA 0x12354 sub_12354 */
void rkos_store_be64(uint32_t lo, uint32_t hi, uint8_t out[8])
{
    out[0] = (uint8_t)(hi >> 24);
    out[1] = (uint8_t)(hi >> 16);
    out[2] = (uint8_t)(hi >> 8);
    out[3] = (uint8_t)hi;
    out[4] = (uint8_t)(lo >> 24);
    out[5] = (uint8_t)(lo >> 16);
    out[6] = (uint8_t)(lo >> 8);
    out[7] = (uint8_t)lo;
}

/* @provenance RVA 0x11dc8 sub_11DC8 */
void rkos_sha256_compress(uint32_t state[8], size_t nblocks, const void *data)
{
    const uint8_t *in = (const uint8_t *)data;
    const uint32_t *k512_words = (const uint32_t *)RKOS_VMA_RAW(RKOS_SHA512_K_RVA, 640u);
    uint32_t w[64];

    while (nblocks > 0u) {
        nblocks--;
        for (unsigned int i = 0u; i < 16u; i++) {
            w[i] = rkos_load_be32(in + i * 4u);
        }
        for (unsigned int i = 16u; i < 64u; i++) {
            uint32_t s0 = rkos_rotr32(w[i - 15u], 7u) ^
                          rkos_rotr32(w[i - 15u], 18u) ^
                          (w[i - 15u] >> 3);
            uint32_t s1 = rkos_rotr32(w[i - 2u], 17u) ^
                          rkos_rotr32(w[i - 2u], 19u) ^
                          (w[i - 2u] >> 10);
            w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
        }

        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];
        uint32_t f = state[5];
        uint32_t g = state[6];
        uint32_t h = state[7];

        for (unsigned int i = 0u; i < 64u; i++) {
            uint32_t sigma1 = rkos_rotr32(e, 6u) ^ rkos_rotr32(e, 11u) ^ rkos_rotr32(e, 25u);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t k_i = k512_words[i * 2u + 1u];
            uint32_t temp1 = h + sigma1 + ch + k_i + w[i];
            uint32_t sigma0 = rkos_rotr32(a, 2u) ^ rkos_rotr32(a, 13u) ^ rkos_rotr32(a, 22u);
            uint32_t maj = (a & b) | ((a | b) & c);
            uint32_t temp2 = sigma0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
        in += 64u;
    }
}

/* @provenance RVA 0x11f38 sub_11F38 */
void rkos_sha512_compress(uint64_t state[8], size_t nblocks, const void *data)
{
    const uint8_t *in = (const uint8_t *)data;
    const uint64_t *k512 = (const uint64_t *)RKOS_VMA_RAW(RKOS_SHA512_K_RVA, 640u);
    uint64_t w[80];

    while (nblocks > 0u) {
        nblocks--;
        for (unsigned int i = 0u; i < 16u; i++) {
            w[i] = rkos_load_be64(in + i * 8u);
        }
        for (unsigned int i = 16u; i < 80u; i++) {
            uint64_t s0 = rkos_rotr64(w[i - 15u], 1u) ^
                          rkos_rotr64(w[i - 15u], 8u) ^
                          (w[i - 15u] >> 7);
            uint64_t s1 = rkos_rotr64(w[i - 2u], 19u) ^
                          rkos_rotr64(w[i - 2u], 61u) ^
                          (w[i - 2u] >> 6);
            w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
        }

        uint64_t a = state[0];
        uint64_t b = state[1];
        uint64_t c = state[2];
        uint64_t d = state[3];
        uint64_t e = state[4];
        uint64_t f = state[5];
        uint64_t g = state[6];
        uint64_t h = state[7];

        for (unsigned int i = 0u; i < 80u; i++) {
            uint64_t sigma1 = rkos_rotr64(e, 14u) ^ rkos_rotr64(e, 18u) ^ rkos_rotr64(e, 41u);
            uint64_t ch = (e & f) ^ ((~e) & g);
            uint64_t temp1 = h + sigma1 + ch + k512[i] + w[i];
            uint64_t sigma0 = rkos_rotr64(a, 28u) ^ rkos_rotr64(a, 34u) ^ rkos_rotr64(a, 39u);
            uint64_t maj = (a & b) | ((a | b) & c);
            uint64_t temp2 = sigma0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
        in += 128u;
    }
}

static void rkos_digest_invoke_compress(const rkos_ccdigest_info_t *di,
                                        void *state_ptr, size_t nblocks, const void *blocks)
{
    if (di->block_size == 64u) {
        rkos_sha256_compress((uint32_t *)state_ptr, nblocks, blocks);
    } else if (di->block_size == 128u) {
        rkos_sha512_compress((uint64_t *)state_ptr, nblocks, blocks);
    }
}

/* @provenance RVA 0x11b04 sub_11B04 */
void rkos_ccdigest_update(uint32_t di_va, void *ctx, size_t len, const void *data)
{
    const rkos_ccdigest_info_t *di = RKOS_VMA_PTR(const rkos_ccdigest_info_t *, di_va);
    uint8_t *cbytes = (uint8_t *)ctx;
    uint64_t *bit_count = (uint64_t *)ctx;
    uint8_t *state_ptr = cbytes + 8u;
    uint8_t *buf_ptr = state_ptr + di->state_size;
    uint32_t *num_ptr = (uint32_t *)(void *)(buf_ptr + di->block_size);
    const uint8_t *in = (const uint8_t *)data;

    if (*num_ptr >= di->block_size) {
        *num_ptr = 0u;
    }

    while (len > 0u) {
        uint32_t used = *num_ptr;
        if (used == 0u && len > di->block_size) {
            size_t nblocks = len / di->block_size;
            size_t consumed = nblocks * di->block_size;
            rkos_digest_invoke_compress(di, state_ptr, nblocks, in);
            *bit_count += (uint64_t)consumed * 8ULL;
            in += consumed;
            len -= consumed;
        } else {
            size_t space = di->block_size - used;
            size_t take = (len < space) ? len : space;
            rkos_memcpy(buf_ptr + used, in, take);
            used += (uint32_t)take;
            *num_ptr = used;
            if (used == di->block_size) {
                rkos_digest_invoke_compress(di, state_ptr, 1u, buf_ptr);
                *bit_count += (uint64_t)di->block_size * 8ULL;
                *num_ptr = 0u;
            }
            in += take;
            len -= take;
        }
    }
}

/* @provenance RVA 0x119ac sub_119AC */
void rkos_ccdigest_final_64be(uint32_t di_va, void *ctx, uint8_t *digest)
{
    const rkos_ccdigest_info_t *di = RKOS_VMA_PTR(const rkos_ccdigest_info_t *, di_va);
    uint8_t work[128];
    size_t ctx_bytes = 8u + di->state_size + di->block_size + 4u;
    rkos_memset(work, 0, sizeof(work));
    rkos_memcpy(work, ctx, ctx_bytes);

    uint64_t *bit_count = (uint64_t *)(void *)work;
    uint32_t *state = (uint32_t *)(void *)(work + 8u);
    uint8_t *buf = work + 8u + di->state_size;
    uint32_t *num = (uint32_t *)(void *)(buf + di->block_size);

    uint32_t used = (*num < di->block_size) ? *num : 0u;
    *bit_count += (uint64_t)used * 8ULL;
    buf[used++] = 0x80u;

    if (used > 56u) {
        while (used < 64u) {
            buf[used++] = 0u;
        }
        rkos_digest_invoke_compress(di, state, 1u, buf);
        used = 0u;
    }
    while (used < 56u) {
        buf[used++] = 0u;
    }

    uint32_t lo = (uint32_t)(*bit_count);
    uint32_t hi = (uint32_t)(*bit_count >> 32);
    rkos_store_be64(lo, hi, buf + 56u);
    rkos_digest_invoke_compress(di, state, 1u, buf);

    for (uint32_t i = 0u; i < (di->output_size / 4u); i++) {
        rkos_store_be32(state[i], digest + i * 4u);
    }
    rkos_memset(work, 0, sizeof(work));
}

/* @provenance RVA 0x1220c sub_1220C */
void rkos_ccdigest_final_128be(uint32_t di_va, void *ctx, uint8_t *digest)
{
    const rkos_ccdigest_info_t *di = RKOS_VMA_PTR(const rkos_ccdigest_info_t *, di_va);
    uint8_t work[256];
    size_t ctx_bytes = 8u + di->state_size + di->block_size + 4u;
    rkos_memset(work, 0, sizeof(work));
    rkos_memcpy(work, ctx, ctx_bytes);

    uint64_t *bit_count = (uint64_t *)(void *)work;
    uint64_t *state = (uint64_t *)(void *)(work + 8u);
    uint8_t *buf = work + 8u + di->state_size;
    uint32_t *num = (uint32_t *)(void *)(buf + di->block_size);

    uint32_t used = (*num < di->block_size) ? *num : 0u;
    *bit_count += (uint64_t)used * 8ULL;
    buf[used++] = 0x80u;

    if (used > 112u) {
        while (used < 128u) {
            buf[used++] = 0u;
        }
        rkos_digest_invoke_compress(di, state, 1u, buf);
        used = 0u;
    }
    while (used < 120u) {
        buf[used++] = 0u;
    }

    uint32_t lo = (uint32_t)(*bit_count);
    uint32_t hi = (uint32_t)(*bit_count >> 32);
    rkos_store_be64(lo, hi, buf + 120u);
    rkos_digest_invoke_compress(di, state, 1u, buf);

    for (uint32_t i = 0u; i < (di->output_size / 8u); i++) {
        uint64_t word = state[i];
        rkos_store_be64((uint32_t)word, (uint32_t)(word >> 32), digest + i * 8u);
    }
    rkos_memset(work, 0, sizeof(work));
}

/* @provenance RVA 0x11910 sub_11910 */
void rkos_ccdigest_oneshot(uint32_t di_va, size_t len, const void *data, void *digest)
{
    const rkos_ccdigest_info_t *di = RKOS_VMA_PTR(const rkos_ccdigest_info_t *, di_va);
    uint8_t ctx[256];
    rkos_memset(ctx, 0, sizeof(ctx));

    const void *iv = RKOS_VMA_RAW(di->initial_state, di->state_size);
    rkos_memcpy(ctx + 8u, iv, di->state_size);
    rkos_ccdigest_update(di_va, ctx, len, data);

    if (di->block_size == 64u) {
        rkos_ccdigest_final_64be(di_va, ctx, (uint8_t *)digest);
    } else {
        rkos_ccdigest_final_128be(di_va, ctx, (uint8_t *)digest);
    }
    rkos_memset(ctx, 0, sizeof(ctx));
}

/* @shim RVA 0x11cae sub_11CAE (pending_c: not shown equivalent to raw) */
void rkos_cchmac_init(uint32_t di_va, void *hctx, size_t key_len, const void *key)
{
    const rkos_ccdigest_info_t *di = RKOS_VMA_PTR(const rkos_ccdigest_info_t *, di_va);
    uint8_t *hc = (uint8_t *)hctx;
    size_t inner_off = (di->state_size + di->block_size + 0x13u) & ~7u;
    const void *iv = RKOS_VMA_RAW(di->initial_state, di->state_size);

    uint8_t key_block[128];
    rkos_memset(key_block, 0, sizeof(key_block));
    if (key_len > di->block_size) {
        rkos_ccdigest_oneshot(di_va, key_len, key, key_block);
    } else if (key_len > 0u) {
        rkos_memcpy(key_block, key, key_len);
    }

    uint8_t pad[128];
    for (uint32_t i = 0u; i < di->block_size; i++) {
        pad[i] = (uint8_t)(key_block[i] ^ 0x5Cu);
    }
    rkos_memcpy(hc + inner_off, iv, di->state_size);
    rkos_digest_invoke_compress(di, hc + inner_off, 1u, pad);

    for (uint32_t i = 0u; i < di->block_size; i++) {
        pad[i] = (uint8_t)(key_block[i] ^ 0x36u);
    }
    rkos_memset(hc, 0, inner_off);
    rkos_memcpy(hc + 8u, iv, di->state_size);
    rkos_digest_invoke_compress(di, hc + 8u, 1u, pad);
    *(uint64_t *)(void *)hc = (uint64_t)di->block_size * 8ULL;

    rkos_memset(key_block, 0, sizeof(key_block));
    rkos_memset(pad, 0, sizeof(pad));
}

/* @provenance RVA 0x11c5c sub_11C5C */
void rkos_cchmac_final(uint32_t di_va, void *hctx, uint8_t *mac_out)
{
    const rkos_ccdigest_info_t *di = RKOS_VMA_PTR(const rkos_ccdigest_info_t *, di_va);
    uint8_t *hc = (uint8_t *)hctx;
    size_t inner_off = (di->state_size + di->block_size + 0x13u) & ~7u;
    uint8_t inner_digest[64];

    if (di->block_size == 64u) {
        rkos_ccdigest_final_64be(di_va, hc, inner_digest);
    } else {
        rkos_ccdigest_final_128be(di_va, hc, inner_digest);
    }

    rkos_memset(hc, 0, inner_off);
    rkos_memcpy(hc + 8u, hc + inner_off, di->state_size);
    *(uint64_t *)(void *)hc = (uint64_t)di->block_size * 8ULL;
    rkos_ccdigest_update(di_va, hc, di->output_size, inner_digest);

    if (di->block_size == 64u) {
        rkos_ccdigest_final_64be(di_va, hc, mac_out);
    } else {
        rkos_ccdigest_final_128be(di_va, hc, mac_out);
    }
    rkos_memset(inner_digest, 0, sizeof(inner_digest));
}

/* @provenance RVA 0x11bd4 sub_11BD4 */
void rkos_cchmac_oneshot(uint32_t di_va, size_t key_len, const void *key,
                         size_t data_len, const void *data, uint8_t *mac_out)
{
    uint8_t hctx[384];
    rkos_memset(hctx, 0, sizeof(hctx));
    rkos_cchmac_init(di_va, hctx, key_len, key);
    rkos_ccdigest_update(di_va, hctx, data_len, data);
    rkos_cchmac_final(di_va, hctx, mac_out);
    rkos_memset(hctx, 0, sizeof(hctx));
}

/* @shim RVA 0x11db8 sub_11DB8 (pending_c: not shown equivalent to raw) */
void rkos_crypto_panic_str(const char *msg)
{
    rtkit_panic("%s", msg);
}

/* @shim RVA 0x1256c sub_1256C (pending_c: not shown equivalent to raw) */
void rkos_drbg_clear(uint32_t drbg_va)
{
    uint8_t *ctx = (uint8_t *)RKOS_VMA_RAW(drbg_va, 0x90u);
    rkos_memset(ctx + 0x04u, 0, 0x40u);
    rkos_memset(ctx + 0x44u, 0, 0x40u);
    *(uint32_t *)(void *)(ctx + 0x88u) = 0xFFFFFFFFu;
    *(uint32_t *)(void *)(ctx + 0x8Cu) = 0xFFFFFFFFu;
}

/* @shim RVA 0x125a8 sub_125A8 (pending_c: not shown equivalent to raw) */
int rkos_drbg_need_reseed(uint32_t drbg_va)
{
    const uint32_t *ctx = RKOS_VMA_PTR(const uint32_t *, drbg_va);
    const uint32_t *info = RKOS_VMA_PTR(const uint32_t *, ctx[0]);
    if (info[1] == 0u) {
        return 0;
    }
    uint64_t counter = ((uint64_t)ctx[0x8Cu / 4u] << 32) | (uint64_t)ctx[0x88u / 4u];
    return (counter > 0x10000ULL) ? 1 : 0;
}
