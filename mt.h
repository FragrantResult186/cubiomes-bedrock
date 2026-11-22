#ifndef mt_H_
#define mt_H_

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32) && defined(__GNUC__) && defined(__x86_64__)
#include <intrin.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

enum {
    MT_N   = 624,
    MT_BUF = 2048
};

typedef struct MTState
{
    uint32_t mt[MT_N];
    uint32_t out[MT_BUF];
    uint32_t seed;
    int init;
    int k;              // next mt[] entry to twist
    uint64_t gen;       // outputs generated since seeding
    uint64_t pos;       // outputs consumed since seeding
    uint32_t last;      // last generated value (for pos >= MT_BUF)
    uint64_t ws;        // cached world seed
    int a;
    int b;
    int hasWs;
} MTState;

typedef struct MTRngState
{
    uint32_t seed;
    uint64_t pos;
    uint64_t ws;
    int a;
    int b;
    int hasWs;
} MTRngState;

#if defined(_WIN32) && defined(__GNUC__) && defined(__x86_64__)
/**
 * MinGW _Thread_local uses emutls (function call).
 * Read the TLS slot directly from TEB instead.
 */
extern uint32_t mt_tls_slot;
MTState *mt_state_init(void);
static inline MTState *mt_state(void)
{
  MTState *s = (MTState *)__readgsqword(0x1480 + 8 * mt_tls_slot);
  return s ? s : mt_state_init();
}
#elif defined(_MSC_VER)
extern __declspec(thread) MTState mt_tls;
static inline MTState *mt_state(void) { return &mt_tls; }
#else
extern _Thread_local MTState mt_tls;
static inline MTState *mt_state(void) { return &mt_tls; }
#endif

void mt_seed(uint32_t seed_value);
uint32_t mt_next_slow(MTState *s);

static inline uint32_t mt_next(void)
{
    MTState *s = mt_state();
    if (s->pos < s->gen && s->pos < MT_BUF)
        return s->out[s->pos++];
    return mt_next_slow(s);
}

static inline void mt_get_state(MTRngState *out)
{
    const MTState *s = mt_state();
    out->seed  = s->seed;
    out->pos   = s->pos;
    out->ws    = s->ws;
    out->a     = s->a;
    out->b     = s->b;
    out->hasWs = s->hasWs;
}

static inline void mt_set_state(const MTRngState *in)
{
    MTState *s = mt_state();
    if (!s->init || s->seed != in->seed)
        mt_seed(in->seed);
    s->pos   = in->pos;
    s->ws    = in->ws;
    s->a     = in->a;
    s->b     = in->b;
    s->hasWs = in->hasWs;
}

static inline void setSeed(uint32_t value)
{
    mt_seed(value);
}

static inline int next()
{
    return (int)(mt_next() >> 1);
}

static inline int nextInt(const int n)
{
    return (int)(mt_next() % n);
}

static inline int nextIntRange(const int a, const int b)
{
    return (a < b) ? (a + nextInt(b - a)) : a;
}

static inline int nextBoolean()
{
    return (mt_next() & 0x8000000) != 0;
}

static inline double nextDouble()
{
    return mt_next() * (1.0 / 4294967296.0);
}

static inline float nextFloat()
{
    return (float)nextDouble();
}

static inline void skipNextN(int n)
{
    mt_state()->pos += n;
}

//==============================================================================
// ChunkRandom
//==============================================================================

#define REGION_A 341873128712L
#define REGION_B 132897987541L

static inline const MTState *worldSeedAB(uint64_t worldSeed)
{
    MTState *s = mt_state();
    if (!s->hasWs || s->ws != worldSeed) {
        setSeed(worldSeed);
        s->a = next();
        s->b = next();
        s->ws = worldSeed;
        s->hasWs = 1;
    }
    return s;
}

static inline uint32_t populationSeed(uint64_t worldSeed, int chunkX, int chunkZ)
{
    const MTState *s = worldSeedAB(worldSeed);
    return (chunkX * (s->a | 1u) + chunkZ * (s->b | 1u)) ^ worldSeed;
}

static inline int32_t setTerrainSeed(int chunkX, int chunkZ)
{
    uint32_t seed = (chunkX * REGION_A + chunkZ * REGION_B);
    setSeed(seed);
    return seed;
}

static inline uint32_t setPopulationSeed(uint64_t worldSeed, int chunkX, int chunkZ)
{
    uint32_t seed = populationSeed(worldSeed, chunkX, chunkZ);
    setSeed(seed);
    return seed;
}

static inline uint32_t setDecorationSeed(uint64_t worldSeed, int chunkX, int chunkZ, int salt)
{
    uint32_t seed = populationSeed(worldSeed, chunkX, chunkZ);
    seed = ((seed >> 2) + (seed << 6) + salt - 1640531527u) ^ seed;
    setSeed(seed);
    return seed;
}

static inline uint32_t setCarverSeed(uint64_t worldSeed, int chunkX, int chunkZ)
{
    const MTState *s = worldSeedAB(worldSeed);
    uint32_t seed = (chunkX * s->a) ^ (chunkZ * s->b) ^ worldSeed;
    setSeed(seed);
    return seed;
}

static inline uint32_t setRegionSeed(uint64_t worldSeed, int regX, int regZ, int salt)
{
    uint32_t seed = (regX * REGION_A) + (regZ * REGION_B) + worldSeed + salt;
    setSeed(seed);
    return seed;
}

static inline uint32_t setFortressSeed(uint64_t worldSeed, int chunkX, int chunkZ)
{
    uint32_t seed = ((chunkX >> 4) ^ ((chunkZ >> 4) << 4)) ^ worldSeed;
    setSeed(seed);
    return seed;
}

static inline uint32_t seedSlimeChunk(int chunkX, int chunkZ)
{
    uint32_t seed = (chunkX * 522133279U) ^ chunkZ;
    setSeed(seed);
    return seed;
}

#ifdef __cplusplus
}
#endif

#endif
