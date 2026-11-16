#include "rkos/rkos.h"

/*
 * Reconstructed Apple corecrypto SHA-256 (`0x11DC8`), SHA-512/384 (`0x11F38`),
 * `ccdigest` (`0x11910`, `0x119AC`, `0x11B04`, `0x1220C`), `cchmac` (`0x11BD4`,
 * `0x11C5C`, `0x11CAE`), and HMAC-DRBG state helpers (`0x118D0`, `0x118E6`,
 * `0x11DB8`, `0x12354`, `0x1256C`, `0x125A8`).
 */

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
int rkos_cc_cmp_safe(size_t len, const void *lhs_buf, const void *rhs_buf)
{
    if (len == 0u) {
        return 1;
    }
    const uint8_t *pa = (const uint8_t *)lhs_buf;
    const uint8_t *pb = (const uint8_t *)rhs_buf;
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

        uint32_t st_a = state[0];
        uint32_t st_b = state[1];
        uint32_t st_c = state[2];
        uint32_t st_d = state[3];
        uint32_t st_e = state[4];
        uint32_t st_f = state[5];
        uint32_t st_g = state[6];
        uint32_t st_h = state[7];

        for (unsigned int i = 0u; i < 64u; i++) {
            uint32_t sigma1 = rkos_rotr32(st_e, 6u) ^ rkos_rotr32(st_e, 11u) ^ rkos_rotr32(st_e, 25u);
            uint32_t ch = (st_e & st_f) ^ ((~st_e) & st_g);
            uint32_t k_i = k512_words[i * 2u + 1u];
            uint32_t temp1 = st_h + sigma1 + ch + k_i + w[i];
            uint32_t sigma0 = rkos_rotr32(st_a, 2u) ^ rkos_rotr32(st_a, 13u) ^ rkos_rotr32(st_a, 22u);
            uint32_t maj = (st_a & st_b) | ((st_a | st_b) & st_c);
            uint32_t temp2 = sigma0 + maj;

            st_h = st_g;
            st_g = st_f;
            st_f = st_e;
            st_e = st_d + temp1;
            st_d = st_c;
            st_c = st_b;
            st_b = st_a;
            st_a = temp1 + temp2;
        }

        state[0] += st_a;
        state[1] += st_b;
        state[2] += st_c;
        state[3] += st_d;
        state[4] += st_e;
        state[5] += st_f;
        state[6] += st_g;
        state[7] += st_h;
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

        uint64_t st_a = state[0];
        uint64_t st_b = state[1];
        uint64_t st_c = state[2];
        uint64_t st_d = state[3];
        uint64_t st_e = state[4];
        uint64_t st_f = state[5];
        uint64_t st_g = state[6];
        uint64_t st_h = state[7];

        for (unsigned int i = 0u; i < 80u; i++) {
            uint64_t sigma1 = rkos_rotr64(st_e, 14u) ^ rkos_rotr64(st_e, 18u) ^ rkos_rotr64(st_e, 41u);
            uint64_t ch = (st_e & st_f) ^ ((~st_e) & st_g);
            uint64_t temp1 = st_h + sigma1 + ch + k512[i] + w[i];
            uint64_t sigma0 = rkos_rotr64(st_a, 28u) ^ rkos_rotr64(st_a, 34u) ^ rkos_rotr64(st_a, 39u);
            uint64_t maj = (st_a & st_b) | ((st_a | st_b) & st_c);
            uint64_t temp2 = sigma0 + maj;

            st_h = st_g;
            st_g = st_f;
            st_f = st_e;
            st_e = st_d + temp1;
            st_d = st_c;
            st_c = st_b;
            st_b = st_a;
            st_a = temp1 + temp2;
        }

        state[0] += st_a;
        state[1] += st_b;
        state[2] += st_c;
        state[3] += st_d;
        state[4] += st_e;
        state[5] += st_f;
        state[6] += st_g;
        state[7] += st_h;
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

static void rkos_digest_invoke_final(uint32_t di_va, const rkos_ccdigest_info_t *di,
                                     void *ctx, uint8_t *digest)
{
    if (di->block_size == 64u) {
        rkos_ccdigest_final_64be(di_va, ctx, digest);
    } else {
        rkos_ccdigest_final_128be(di_va, ctx, digest);
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
    rkos_digest_invoke_final(di_va, di, ctx, (uint8_t *)digest);
    rkos_memset(ctx, 0, sizeof(ctx));
}

/* @provenance RVA 0x11cae sub_11CAE */
int rkos_cchmac_init(uint32_t di_va, void *hctx, size_t key_len, const void *key)
{
    const rkos_ccdigest_info_t *di = RKOS_VMA_PTR(const rkos_ccdigest_info_t *, di_va);
    uint8_t *hc = (uint8_t *)hctx;
    uint8_t *state_ptr = hc + 8u;
    uint8_t *pad_buf = hc + 8u + di->state_size;
    const uint8_t *key_bytes = (const uint8_t *)key;
    const void *iv = RKOS_VMA_RAW(di->initial_state, di->state_size);
    size_t used_key_len = key_len;

    if (key_len <= di->block_size) {
        for (size_t idx = 0u; idx < key_len; idx++) {
            pad_buf[idx] = (uint8_t)(key_bytes[idx] ^ 0x5Cu);
        }
    } else {
        (void)memcpy_thunk(state_ptr, iv, di->state_size);
        *(uint64_t *)(void *)hc = 0ULL;
        *(uint32_t *)(void *)(pad_buf + di->block_size) = 0u;
        rkos_ccdigest_update(di_va, hc, key_len, key);
        rkos_digest_invoke_final(di_va, di, hc, pad_buf);
        used_key_len = di->output_size;
        for (size_t idx = 0u; idx < used_key_len; idx++) {
            pad_buf[idx] ^= 0x5Cu;
        }
    }

    if (di->block_size > used_key_len) {
        (void)rkos_memset(pad_buf + used_key_len, 0x5C, di->block_size - used_key_len);
    }

    size_t outer_off = (di->block_size + di->state_size + 0x13u) & ~7u;
    (void)memcpy_thunk(hc + outer_off, iv, di->state_size);
    rkos_digest_invoke_compress(di, hc + outer_off, 1u, pad_buf);

    for (uint32_t idx = 0u; idx < di->block_size; idx++) {
        pad_buf[idx] ^= 0x6Au;
    }

    (void)memcpy_thunk(state_ptr, iv, di->state_size);
    rkos_digest_invoke_compress(di, state_ptr, 1u, pad_buf);
    *(uint64_t *)(void *)hc = (uint64_t)di->block_size * 8ULL;
    *(uint32_t *)(void *)(pad_buf + di->block_size) = 0u;
    return 0;
}

/* @provenance RVA 0x11c5c sub_11C5C */
void rkos_cchmac_final(uint32_t di_va, void *hctx, uint8_t *mac_out)
{
    const rkos_ccdigest_info_t *di = RKOS_VMA_PTR(const rkos_ccdigest_info_t *, di_va);
    uint8_t *hc = (uint8_t *)hctx;
    size_t outer_off = (di->state_size + di->block_size + 0x13u) & ~7u;
    uint8_t inner_digest[64];

    rkos_digest_invoke_final(di_va, di, hc, inner_digest);

    rkos_memset(hc, 0, outer_off);
    rkos_memcpy(hc + 8u, hc + outer_off, di->state_size);
    *(uint64_t *)(void *)hc = (uint64_t)di->block_size * 8ULL;
    rkos_ccdigest_update(di_va, hc, di->output_size, inner_digest);
    rkos_digest_invoke_final(di_va, di, hc, mac_out);
    rkos_memset(inner_digest, 0, sizeof(inner_digest));
}

/* @provenance RVA 0x11bd4 sub_11BD4 */
void rkos_cchmac_oneshot(uint32_t di_va, size_t key_len, const void *key,
                         size_t data_len, const void *data, uint8_t *mac_out)
{
    uint8_t hctx[384];
    rkos_memset(hctx, 0, sizeof(hctx));
    (void)rkos_cchmac_init(di_va, hctx, key_len, key);
    rkos_ccdigest_update(di_va, hctx, data_len, data);
    rkos_cchmac_final(di_va, hctx, mac_out);
    rkos_memset(hctx, 0, sizeof(hctx));
}

/* @provenance RVA 0x11db8 sub_11DB8 */
void rkos_crypto_panic_str(const char *msg)
{
    rtkit_panic("%s", msg);
}

/* @provenance RVA 0x1256c sub_1256C */
uint32_t rkos_drbg_clear(uint32_t drbg_va)
{
    uint8_t *ctx = (uint8_t *)RKOS_VMA_RAW(drbg_va, 0x90u);
#ifdef RKOS_HOST
    (void)rkos_memset(ctx + 0x04u, 0, 0x40u);
    (void)rkos_memset(ctx + 0x44u, 0, 0x40u);
#else
    uint32_t fn_va = *RKOS_VMA_PTR(const uint32_t *, 0x000160C0u);
    (void)rkos_call_fn3(0x000160C0u, fn_va, drbg_va + 0x04u, 0u, 0x40u);
    fn_va = *RKOS_VMA_PTR(const uint32_t *, 0x000160C0u);
    (void)rkos_call_fn3(0x000160C0u, fn_va, drbg_va + 0x44u, 0u, 0x40u);
#endif
    *(uint32_t *)(void *)(ctx + 0x8Cu) = 0xFFFFFFFFu;
    *(uint32_t *)(void *)(ctx + 0x88u) = 0xFFFFFFFFu;
    return 0xFFFFFFFFu;
}

/* @provenance RVA 0x125a8 sub_125A8 */
int rkos_drbg_need_reseed(uint32_t drbg_va)
{
    const uint32_t *ctx = RKOS_VMA_PTR(const uint32_t *, drbg_va);
    const uint32_t *info = RKOS_VMA_PTR(const uint32_t *, ctx[0]);
    if (info[1] == 0u) {
        return 0;
    }
    uint64_t counter = ((uint64_t)ctx[0x8Cu / 4u] << 32) | (uint64_t)ctx[0x88u / 4u];
    return (counter > 0x0001000000000000ULL) ? 1 : 0;
}
