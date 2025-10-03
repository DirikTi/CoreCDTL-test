#include <string.h>
#if defined(__AVX2__) && defined(__SSE4_1__)
#include <immintrin.h>
#endif


#if defined(__AVX2__) && defined(__SSE4_1__)
int simd_strcmp_u128(const char *a, const char *b) {
    __m256i *a_vec = (__m256i*)a;
    __m256i *b_vec = (__m256i*)b;
    __m256i zero = _mm256_setzero_si256();

    while (1) {
        __m256i a_val = _mm256_loadu_si256(a_vec);
        __m256i b_val = _mm256_loadu_si256(b_vec);

        // Karakter karşılaştırma
        __m256i cmp = _mm256_cmpeq_epi8(a_val, b_val);
        int mask = _mm256_movemask_epi8(cmp);

        if (mask != 0xFFFFFFFF) return 1; // Farklılık bulundu

        // Null terminator kontrolü (her 32 byte'ta bir)
        __m256i null_check = _mm256_cmpeq_epi8(a_val, zero);
        int null_mask = _mm256_movemask_epi8(null_check);

        if (null_mask != 0) break; // String sonu

        a_vec++; b_vec++;
    }
    return 0;
}
#endif

#if defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#include <arm_sve.h>

int simd_strcmp_arm(const char *a, const char *b)
{
    const uint8_t *a_ptr = (const uint8_t*)a;
    const uint8_t *b_ptr = (const uint8_t*)b;
    uint8x16_t zero = vdupq_n_u8(0);

    while (1) {
        uint8x16_t a_val = vld1q_u8(a_ptr);
        uint8x16_t b_val = vld1q_u8(b_ptr);

        uint8x16_t cmp = vceqq_u8(a_val, b_val);
        uint8_t all_equal = vminvq_u8(cmp);
        if (all_equal != 0xFF) return 1;

        uint8x16_t null_check = vceqq_u8(a_val, zero);
        uint8_t has_null = vmaxvq_u8(null_check);
        if (has_null == 0xFF) break;

        a_ptr += 16;
        b_ptr += 16;
    }
    return 0;
}

// ARM SVE (Scalable Vector Extension)
#if defined(__ARM_FEATURE_SVE)
int simd_strcmp_arm_sve(const char *a, const char *b) {
    svuint8_t zero = svdup_n_u8(0);
    size_t i = 0;

    while (1) {
        svbool_t pg = svwhilelt_b8(i, svcntb());
        svuint8_t a_val = svld1_u8(pg, (const uint8_t*)a + i);
        svuint8_t b_val = svld1_u8(pg, (const uint8_t*)b + i);

        // Karşılaştırma
        svbool_t cmp = svcmpeq_u8(pg, a_val, b_val);
        if (!svptest_last(pg, cmp)) return 1;

        // Null kontrolü
        svbool_t null_cmp = svcmpeq_u8(pg, a_val, zero);
        if (svptest_any(pg, null_cmp)) break;

        i += svcntb();
    }
    return 0;
}
#endif
#endif

#if defined(__riscv) && defined(__riscv_v)
#include <riscv_vector.h>

int simd_strcmp_riscv(const char *a, const char *b) {
    size_t vl;
    const uint8_t *a_ptr = (const uint8_t*)a;
    const uint8_t *b_ptr = (const uint8_t*)b;

    while (1) {
        vl = __riscv_vsetvl_e8m8(strlen(a_ptr));

        vuint8m8_t a_val = __riscv_vle8_v_u8m8(a_ptr, vl);
        vuint8m8_t b_val = __riscv_vle8_v_u8m8(b_ptr, vl);

        // Karakter karşılaştırma
        vbool1_t cmp = __riscv_vmseq_vv_u8m8_b1(a_val, b_val, vl);
        bool all_equal = __riscv_vfirst_m_b1(cmp, vl) == -1;

        if (!all_equal) return 1;

        // Null terminator kontrolü
        vuint8m8_t zero = __riscv_vmv_v_x_u8m8(0, vl);
        vbool1_t null_cmp = __riscv_vmseq_vv_u8m8_b1(a_val, zero, vl);
        bool has_null = __riscv_vfirst_m_b1(null_cmp, vl) != -1;

        if (has_null) break;

        a_ptr += vl; b_ptr += vl;
    }
    return 0;
}
#endif

int optimized_strcmp(const char *a, const char *b)
{
    if (__builtin_expect(strlen(a) < 16, 0) ||
        __builtin_expect(strlen(b) < 16, 0)) {
        return strcmp(a, b);
        }

#if defined(__AVX2__) && defined(__SSE4_1__)
    return simd_strcmp_x86(a, b);
#elif defined(__aarch64__) && defined(__ARM_FEATURE_SVE)
    return simd_strcmp_arm_sve(a, b);
#elif defined(__aarch64__)
    return simd_strcmp_arm(a, b);
#elif defined(__riscv_v)
    return simd_strcmp_riscv(a, b);
#else
    return strcmp(a, b); // Fallback
#endif
}