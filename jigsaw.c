#include "jigsaw.h"
#include "mt.h"

#include <stdlib.h>
#include <string.h>

static const Pos3    g_dir[6]       = {{0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}};
static const uint8_t g_opposite[6]  = {1, 0, 3, 2, 5, 4};
static const uint8_t g_cw[6]        = {0, 1, 5, 4, 2, 3};

// Fisher-Yates shuffle
static void shuffle(void *arr, int n, size_t sz)
{
    char *a = (char*) arr;
    for (int i = 1; i < n; i++)
    {
        int j = nextInt(i + 1);
        if (j == i)
            continue;
        for (size_t b = 0; b < sz; b++)
        {
            char t = a[i*sz + b];
            a[i*sz + b] = a[j*sz + b];
            a[j*sz + b] = t;
        }
    }
}

static void expand(Placer *pl, int idx);

BB jigsawBB(const JigsawTemplate *t, Pos3 pos, int rot)
{
    int dx = ((rot & 1) ? t->sz : t->sx) - 1;
    int dz = ((rot & 1) ? t->sx : t->sz) - 1;
    int x = (rot == 1 || rot == 2) ? pos.x - dx : pos.x;
    int z = (rot == 2 || rot == 3) ? pos.z - dz : pos.z;
    return bb3(x, pos.y, z, x + dx, pos.y + t->sy - 1, z + dz);
}

static int getJigsaws(Jigsaw *out, const JigsawSet *s, const JigsawTemplate *t, Pos3 pos, int rot)
{
    for (int i = 0; i < t->blockCnt; i++)
    {
        const JigsawBlock *b = &s->blocks[t->block + i];
        int x = b->x, z = b->z, f = b->facing;
        for (int r = 0; r < rot; r++)
        {
            int tmp = x;
            x = -z;
            z = tmp;
            f = g_cw[f];
        }
        out[i] = (Jigsaw) {{pos.x + x, pos.y + b->y, pos.z + z}, f, b->name, b->pool, b->target};
    }
    return t->blockCnt;
}

Pos3 jigsawAnchor(const JigsawSet *set, int tmpl, int rot, int name)
{
    Jigsaw js[BLOCK_MAX];
    int jn = getJigsaws(js, set, &set->tmpls[tmpl], (Pos3){0, 0, 0}, rot);
    shuffle(js, jn, sizeof(*js));
    for (int i = 0; i < jn; i++) {
        if (js[i].name == name)
            return js[i].pos;
    }
    return (Pos3){0, 0, 0};
}

static int shuffledTemplates(uint16_t *out, const JigsawSet *s, int pool)
{
    const JigsawPool *jp = &s->pools[pool];
    int cnt = 0;
    for (int i = 0; i < jp->entryCnt; i++)
    {
        const JigsawEntry *e = &s->entries[jp->entry + i];
        for (int w = 0; w < e->weight && cnt < TMPL_MAX; w++) {
            out[cnt++] = e->tmpl;
        }
    }
    shuffle(out, cnt, sizeof(*out));
    return cnt;
}

static Pos3 target(const Jigsaw *j)
{
    Pos3 d = g_dir[j->facing];
    return (Pos3) {
        j->pos.x + d.x,
        j->pos.y + d.y,
        j->pos.z + d.z
    };
}

static int fits(const Placer *pl, Parent *pa, const BB *bb, int inside)
{
    if (!inside)
    {
        for (int i = 0; i < pl->n; i++) {
            if (overlapXZ(bb, &pl->p[i].bb))
                return 0;
        }
        return 1;
    }
    if (bb->minX < pa->bb.minX || bb->maxX > pa->bb.maxX ||
        bb->minZ < pa->bb.minZ || bb->maxZ > pa->bb.maxZ)
        return 0;
    for (int i = 0; i < pa->resCnt; i++) {
        if (overlapXZ(bb, &pa->res[i]))
            return 0;
    }
    if (pa->resCnt < RESERVED_MAX)
        pa->res[pa->resCnt++] = *bb;
    return 1;
}

static int tryPool(Placer *pl, Parent *pa, const Jigsaw *pj, int pool, int alias)
{
    const JigsawSet *s = pl->set;
    uint16_t tmpls[TMPL_MAX];
    int cnt = shuffledTemplates(tmpls, s, pool);
    Pos3 tgt = target(pj);
    int inside = inBB(&pa->bb, tgt);

    for (int i = 0; i < cnt; i++)
    {
        if (tmpls[i] == JIGSAW_EMPTY)
            return 1;
        const JigsawTemplate *t = &s->tmpls[tmpls[i]];
        uint8_t rots[4] = {1, 2, 3, 0};
        shuffle(rots, 4, 1);
        for (int r = 0; r < 4; r++)
        {
            Jigsaw js[BLOCK_MAX];
            int jn = getJigsaws(js, s, t, (Pos3){0, 0, 0}, rots[r]);
            shuffle(js, jn, sizeof(*js));
            for (int k = 0; k < jn; k++)
            {
                if (js[k].facing != g_opposite[pj->facing] || js[k].name != pj->name)
                    continue;
                Pos3 pos = {
                    tgt.x - js[k].pos.x,
                    tgt.y - js[k].pos.y,
                    tgt.z - js[k].pos.z
                };
                BB bb = jigsawBB(t, pos, rots[r]);
                if (!fits(pl, pa, &bb, inside))
                    continue;
                if (pl->n < pl->cap)
                {
                    pl->p[pl->n] = (JigsawPiece) {pos, bb, tmpls[i], rots[r], pa->depth + 1, alias};
                    expand(pl, pl->n++);
                }
                return 1;
            }
        }
    }
    return 0;
}

static void expand(Placer *pl, int idx)
{
    const JigsawSet  *s = pl->set;
    const JigsawPiece p = pl->p[idx];
    if (p.depth > s->maxDepth)
        return;

    Jigsaw js[BLOCK_MAX];
    int jn = getJigsaws(js, s, &s->tmpls[p.tmpl], p.pos, p.rot);
    shuffle(js, jn, sizeof(*js));

    Parent pa = {p.bb, p.depth, {{0}}, 0};
    for (int k = 0; k < jn; k++)
    {
        int pool = js[k].pool;
        if (pool < 0)
            continue;
        int alias = p.alias && s->pools[pool].alias >= 0;
        if (alias)
            pool = s->pools[pool].alias;
        int fallback = s->pools[pool].fallback;
        int ok = p.depth < s->maxDepth && tryPool(pl, &pa, &js[k], pool, alias);
        if (!ok && fallback >= 0)
            tryPool(pl, &pa, &js[k], fallback, 0);
    }
}

static int addSpace(Placer *pl, BB bb, int level, int used)
{
    if (pl->spN == pl->spCap)
    {
        int cap = pl->spCap ? 2 * pl->spCap : 256;
        Space *sp = (Space*) realloc(pl->sp, cap * sizeof(*sp));
        if (!sp)
            return 0;
        pl->sp = sp;
        pl->spCap = cap;
    }
    pl->sp[pl->spN++] = (Space) { bb, level, used };
    return 1;
}

static int isFree(const Placer *pl, const BB *bb, int level)
{
    int in = 0;
    for (int i = 0; i < pl->spN; i++)
    {
        const Space *s = &pl->sp[i];
        if (s->level != level)
            continue;
        if (!s->used)
            in |= contains(&s->bb, bb);
        else if (intersects(&s->bb, bb))
            return 0;
    }
    return in;
}

static int tryPlace(Placer *pl, int par, const Jigsaw *pj, int pool, int level)
{
    const JigsawSet *s = pl->set;
    const JigsawPiece pa = pl->p[par];
    uint16_t tmpls[TMPL_MAX];
    int cnt = shuffledTemplates(tmpls, s, pool);
    Pos3 tgt = target(pj);
    int inside = inBB(&pa.bb, tgt);

    for (int i = 0; i < cnt; i++)
    {
        if (tmpls[i] == JIGSAW_EMPTY)
            return 1;
        const JigsawTemplate *t = &s->tmpls[tmpls[i]];
        uint8_t rots[4] = {1, 2, 3, 0};
        shuffle(rots, 4, 1);
        for (int r = 0; r < 4; r++)
        {
            Jigsaw js[BLOCK_MAX];
            int jn = getJigsaws(js, s, t, (Pos3){0, 0, 0}, rots[r]);
            shuffle(js, jn, sizeof(*js));
            for (int k = 0; k < jn; k++)
            {
                if (js[k].facing != g_opposite[pj->facing] || js[k].name != pj->target)
                    continue;
                Pos3 pos = {
                    tgt.x - js[k].pos.x,
                    tgt.y - js[k].pos.y,
                    tgt.z - js[k].pos.z
                };
                BB bb = jigsawBB(t, pos, rots[r]);
                if (inside && !addSpace(pl, pa.bb, ++level, 0))
                    return 1;
                if (!isFree(pl, &bb, level))
                    continue;
                if (pl->n < pl->cap && addSpace(pl, bb, level, 1))
                {
                    pl->level[pl->n] = level;
                    pl->p[pl->n++] = (JigsawPiece){pos, bb, tmpls[i], rots[r], pa.depth + 1, 0};
                }
                return 1;
            }
        }
    }
    return 0;
}

static void addPiece(Placer *pl, int idx)
{
    const JigsawSet *s = pl->set;
    const JigsawPiece p = pl->p[idx];
    if (p.depth > s->maxDepth)
        return;

    Jigsaw js[BLOCK_MAX];
    int jn = getJigsaws(js, s, &s->tmpls[p.tmpl], p.pos, p.rot);
    shuffle(js, jn, sizeof(*js));

    for (int k = 0; k < jn; k++)
    {
        int pool = js[k].pool;
        if (pool < 0)
            continue;
        int fallback = s->pools[pool].fallback;
        int ok = p.depth < s->maxDepth && tryPlace(pl, idx, &js[k], pool, pl->level[idx]);
        if (!ok && fallback >= 0)
            tryPlace(pl, idx, &js[k], fallback, pl->level[idx]);
    }
}

int placeLegacyJigsaw(JigsawPiece *out, int cap, const JigsawSet *set, int tmpl, Pos3 pos, int rot, int alias)
{
    if (cap <= 0)
        return 0;
    Placer pl = {set, out, 1, cap, NULL, NULL, 0, 0};
    out[0] = (JigsawPiece){pos, jigsawBB(&set->tmpls[tmpl], pos, rot), tmpl, rot, 0, alias};
    expand(&pl, 0);
    return pl.n;
}

int placeJigsaw(JigsawPiece *out, int cap, const JigsawSet *set, int tmpl, Pos3 pos, int rot)
{
    if (cap <= 0)
        return 0;
    BB bb = jigsawBB(&set->tmpls[tmpl], pos, rot);
    int d = set->maxDist;
    int x = bb.minX + (bb.maxX - bb.minX+1)/2;
    int y = bb.minY + (bb.maxY - bb.minY+1)/2;
    int z = bb.minZ + (bb.maxZ - bb.minZ+1)/2;

    Placer pl = {set, out, 1, cap, (int*) calloc(cap, sizeof(int)), NULL, 0, 0};
    out[0] = (JigsawPiece) {pos, bb, tmpl, rot, 0, 0};
    if (pl.level &&
        addSpace(&pl, bb3(x - d, y - d, z - d, x + d, y + d, z + d), 0, 0) &&
        addSpace(&pl, bb, 0, 1))
    {
        for (int i = 0; i < pl.n; i++) {
            addPiece(&pl, i);
        }
    }
    free(pl.level);
    free(pl.sp);
    return pl.n;
}
