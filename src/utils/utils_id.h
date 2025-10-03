#ifndef CORECDTL_UTILS_ID_H
#define CORECDTL_UTILS_ID_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define RAND32() \
((((uint32_t)rand() & 0x7FFF) << 17) | ((uint32_t)rand() & 0x1FFFF))

#define RAND64() \
((((uint64_t)rand() & 0x7FFF) << 49) | \
(((uint64_t)rand() & 0x7FFF) << 34) | \
(((uint64_t)rand() & 0x7FFF) << 19) | \
((uint64_t)rand() & 0x7FFFF))

#define RAND128() \
(((__uint128_t)(RAND64()) << 64) | (__uint128_t)(RAND64()))

// 16-bit ID
#define ID_CREATE_U16() ((uint16_t)(rand() & 0xFFFF))
#define ID_CMP_U16(a, b) ((a) == (b))

// 32-bit ID
#define ID_CREATE_U32() (RAND32())
#define ID_CMP_U32(a, b) ((a) == (b))

// 64-bit ID
#define ID_CREATE_U64() (RAND64())
#define ID_CMP_U64(a, b) ((a) == (b))

// 128-bit ID
#define ID_CREATE_U128() (RAND128())
#define ID_CMP_U128(a, b) ((a) == (b))

// 128-bit ID write (hex)
#define ID_PRINT_U128(id) \
printf("0x%016llX%016llX\n", \
(unsigned long long)((uint64_t)((id) >> 64)), \
(unsigned long long)((uint64_t)(id)))

#endif // CORECDTL_UTILS_ID_H
