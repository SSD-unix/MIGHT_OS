#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>   // уже определяет int8_t, int16_t, int32_t и т.д.

// Сокращения (u8, u16 и т.д.) оставляем, они базируются на стандартных типах
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;

// Функция memcpy
void memcpy(u8 *src, u8 *dest, u32 bytes);

#endif
