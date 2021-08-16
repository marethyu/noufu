#ifndef _BIT_MAGIC_H_
#define _BIT_MAGIC_H_

#define GET_BIT(n, p) ((n & (1 << (p))) != 0)
#define SET_BIT(n, p) do { n |= (1 << (p)); } while (0)
#define RES_BIT(n, p) do { n &= ~(1 << (p)); } while (0)
#define SWAP_NIBBLES8(n) do { n = ((n & 0x0F) << 4) | (n >> 4); } while (0)

#endif