#ifndef JIGSAW_H_
#define JIGSAW_H_

#include "util.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define JIGSAW_EMPTY 0xffff
#define BLOCK_MAX    64
#define RESERVED_MAX 64
#define TMPL_MAX     256

typedef struct
{
    Pos3 pos;
    uint8_t facing;
    uint8_t name;
    int16_t pool;
    uint8_t target;
} Jigsaw;

typedef struct
{
    int8_t x, y, z;
    uint8_t facing;
    uint8_t name;
    int16_t pool;
    uint8_t target;
} JigsawBlock;

typedef struct
{
    const char *name;
    uint8_t sx, sy, sz;
    uint8_t  type;
    uint16_t block;
    uint8_t  blockCnt;
} JigsawTemplate;

typedef struct
{
    uint16_t tmpl;
    uint8_t  weight;
} JigsawEntry;

typedef struct
{
    uint16_t entry;
    uint8_t  entryCnt;
    int16_t  fallback;
    int16_t  alias;
} JigsawPool;

typedef struct
{
    const JigsawTemplate *tmpls;
    const JigsawBlock    *blocks;
    const JigsawEntry    *entries;
    const JigsawPool     *pools;
    int maxDepth;
    int maxDist;
} JigsawSet;

typedef struct
{
    Pos3 pos;
    BB bb;
    uint16_t tmpl;
    uint8_t  rot;
    uint8_t  depth;
    uint8_t  alias;
} JigsawPiece;

typedef struct
{
    BB bb;
    int level;
    int used;
} Space;

typedef struct
{
    const JigsawSet *set;
    JigsawPiece *p;
    int n, cap;
    int *level;
    Space *sp;
    int spN, spCap;
} Placer;

typedef struct
{
    BB bb;
    int depth;
    BB res[RESERVED_MAX];
    int resCnt;
} Parent;

BB jigsawBB(const JigsawTemplate *t, Pos3 pos, int rot);
Pos3 jigsawAnchor(const JigsawSet *set, int tmpl, int rot, int name);

int placeLegacyJigsaw(JigsawPiece *out, int cap, const JigsawSet *set, int tmpl, Pos3 pos, int rot, int alias);
int placeJigsaw(JigsawPiece *out, int cap, const JigsawSet *set, int tmpl, Pos3 pos, int rot);

#ifdef __cplusplus
}
#endif

#endif
