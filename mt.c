/*
   A C-program for MT19937, with initialization improved 2002/1/26.
   Coded by Takuji Nishimura and Makoto Matsumoto.
   
   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.
   
   http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
*/

#include <stdio.h>
#include <stdlib.h>
#include "mt.h"

#define PERIOD 397

#if defined(_WIN32) && defined(__GNUC__) && defined(__x86_64__)
#include <windows.h>

uint32_t mt_tls_slot;
static _Thread_local MTState mt_tls;

__attribute__((constructor(101)))
static void mt_tls_alloc(void)
{
  DWORD slot = TlsAlloc();
  if (slot >= 64) {
      fprintf(stderr, "MT ERR: no TEB TLS slot left\n");
      abort();
  }
  mt_tls_slot = slot;
}

MTState *mt_state_init(void)
{
  TlsSetValue(mt_tls_slot, &mt_tls);
  return &mt_tls;
}
#elif defined(_MSC_VER)
__declspec(thread) MTState mt_tls;
#else
_Thread_local MTState mt_tls;
#endif

static inline uint32_t temper(uint32_t y)
{
  y ^= y >> 11;
  y ^= (y << 7) & 0x9d2c5680;
  y ^= (y << 15) & 0xefc60000;
  y ^= y >> 18;
  return y;
}

static inline uint32_t twist(uint32_t a, uint32_t b, uint32_t m)
{
  uint32_t y = (a & 0x80000000) | (b & 0x7fffffff);
  return m ^ (y >> 1) ^ (-(y & 1) & 0x9908b0df);
}

static void twistRun(uint32_t *restrict mt, int k, int n, uint32_t *restrict out)
{
  if (k == MT_N - 1) {
      mt[k] = twist(mt[k], mt[0], mt[PERIOD - 1]);
      out[0] = temper(mt[k]);
      return;
  }
  
  const int off = (k < MT_N - PERIOD) ? PERIOD : PERIOD - MT_N;
  for (int i = 0; i < n; i++) {
      mt[k + i] = twist(mt[k + i], mt[k + i + 1], mt[k + i + off]);
      out[i] = temper(mt[k + i]);
  }
}

static void generate(MTState *s, uint64_t target)
{
  uint32_t *mt = s->mt;
  
  if (s->init < MT_N) {
      int n = s->k + (int)(target - s->gen) + PERIOD;
      if (n > MT_N)
          n = MT_N;
      for (int i = s->init; i < n; i++)
          mt[i] = 0x6c078965 * (mt[i-1] ^ (mt[i-1] >> 30)) + i;
      s->init = n;
  }
  
  while (s->gen < target) {
      int k = s->k;
      int end = (k < MT_N - PERIOD) ? MT_N - PERIOD :
                (k < MT_N - 1)      ? MT_N - 1 : MT_N;
      uint64_t n = target - s->gen;
      if (n > (uint64_t)(end - k))
          n = end - k;
  
      uint32_t tmp[16], *out = tmp;
      if (s->gen < MT_BUF) {
          out = s->out + s->gen;
          if (n > MT_BUF - s->gen)
              n = MT_BUF - s->gen;
      } else if (n > 16) {
          n = 16;
      }
  
      twistRun(mt, k, (int)n, out);
      s->last = out[n-1];
      s->gen += n;
      s->k = (k + n < MT_N) ? k + n : 0;
  }
}

void mt_seed(uint32_t value)
{
  MTState *s = mt_state();
  s->mt[0] = value;
  s->init = 1;
  s->k = 0;
  s->seed = value;
  s->gen = 0;
  s->pos = 0;
}

uint32_t mt_next_slow(MTState *s)
{
  uint64_t pos = s->pos;
  
  if (!s->init)
      mt_seed(5489);
  
  /* rewound past the buffer: replay from seed */
  if (pos >= MT_BUF && pos + 1 < s->gen)
      mt_seed(s->seed);
  
  s->pos = pos;
  generate(s, pos < MT_BUF ? (pos | 15) + 1 : pos + 1);
  s->pos = pos + 1;
  
  return (pos < MT_BUF) ? s->out[pos] : s->last;
}
