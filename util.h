#ifndef UTIL_H_
#define UTIL_H_


#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    int x, z;
} Pos;

typedef struct {
    int x, y, z;
} Pos3;

typedef struct
{
    int minX, minY, minZ;
    int maxX, maxY, maxZ;
} BB;

static inline BB bb3(int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
{
    BB b = {minX, minY, minZ, maxX, maxY, maxZ};
    return b;
}

static inline BB bb2(int minX, int minZ, int maxX, int maxZ)
{
    return bb3(minX, 0, minZ, maxX, 0, maxZ);
}

static inline BB bb3Empty(void)
{
    return bb3(0x7fffffff,0x7fffffff,0x7fffffff,(int)0x80000000,(int)0x80000000,(int)0x80000000);
}

static inline int intersects(const BB *a, const BB *b)
{
    return a->maxX >= b->minX && a->minX <= b->maxX &&
           a->maxZ >= b->minZ && a->minZ <= b->maxZ &&
           a->maxY >= b->minY && a->minY <= b->maxY;
}

static inline int xSize(const BB *b)
{
    return b->maxX - b->minX + 1;
}

static inline int zSize(const BB *b)
{
    return b->maxZ - b->minZ + 1;
}

enum
{
    FACING_DOWN,
    FACING_UP,
    FACING_NORTH,
    FACING_SOUTH,
    FACING_WEST,
    FACING_EAST,
};

static inline int toRot(int facing)
{
    switch (facing)
    {
    case FACING_NORTH: return 0;
    case FACING_EAST:  return 1;
    case FACING_SOUTH: return 2;
    case FACING_WEST:  return 3;
    default:           return facing;
    }
}

static inline BB orientedBB3(
    int x, int y, int z,
    int offX, int offY, int offZ,
    int sizeX, int sizeY, int sizeZ,
    int rot)
{
    switch (rot)
    {
    default:
    case 0: return bb3(x+offX, y+offY, z-sizeZ+1+offZ, x+sizeX-1+offX, y+sizeY-1+offY, z+offZ);
    case 2: return bb3(x+offX, y+offY, z+offZ, x+sizeX-1+offX, y+sizeY-1+offY, z+sizeZ-1+offZ);
    case 3: return bb3(x-sizeZ+1+offZ, y+offY, z+offX, x+offZ, y+sizeY-1+offY, z+sizeX-1+offX);
    case 1: return bb3(x+offZ, y+offY, z+offX, x+sizeZ-1+offZ, y+sizeY-1+offY, z+sizeX-1+offX);
    }
}

static inline void rotatePos2D(
    int rot,
    int pivotX, int pivotZ,
    int x, int z,
    int *outX, int *outZ)
{
    switch (rot & 3)
    {
    case 1: // 90 clockwise
        *outX = pivotX + pivotZ - z;
        *outZ = pivotZ - pivotX + x;
        break;
    case 2: // 180
        *outX = 2*pivotX - x;
        *outZ = 2*pivotZ - z;
        break;
    case 3: // 90 counter-clockwise
        *outX = pivotX - pivotZ + z;
        *outZ = pivotX + pivotZ - x;
        break;
    default:
        *outX = x;
        *outZ = z;
    }
}

int applyXTransform(BB bb, int rot, int x, int z);
int applyYTransform(BB bb, int y);
int applyZTransform(BB bb, int rot, int x, int z);
Pos3 transformPos(BB bb, int rot, int x, int y, int z);

typedef struct
{
    int64_t key;
    int8_t  val;
    uint8_t used;
} SparseCell;

static inline int64_t packPosKey(int x, int y, int z, int originX, int originZ)
{
    return ((int64_t)(uint16_t)(x - originX) << 32)
         | ((int64_t)(uint16_t)(z - originZ) << 16)
         | (uint16_t)(y);
}

uint64_t sparseHash(int64_t key);

void sparseSet(SparseCell *tab, int cap, int64_t key, int val);

int sparseFind(const SparseCell *tab, int cap, int64_t key, int *val);

/* Loads a list of seeds from a file. The seeds should be written as decimal
 * ASCII numbers separated by newlines.
 * @fnam: file path
 * @scnt: number of valid seeds found in the file, which is also the number of
 *        elements in the returned buffer
 *
 * Return a pointer to a dynamically allocated seed list.
 */
uint64_t *loadSavedSeeds(const char *fnam, uint64_t *scnt);


/// convert between version enum and text
const char* mc2str(int mc);
int str2mc(const char *s);

/// get the resource id name for a biome (for versions 1.13+)
const char *biome2str(int mc, int id);

/// get the resource id name for a structure
const char *struct2str(int stype);

/// get the resource pool key name for a camp tent structure piece
const char *getCampTentName(int biomeID, int idx);

/// get the resource pool key name for a campsite structure piece
const char *getCampsiteName(int biomeID, int idx, int biomeCampCount);

/// initialize a biome colormap with some defaults
void initBiomeColors(unsigned char biomeColors[256][3]);
void initBiomeTypeColors(unsigned char biomeColors[256][3]);

/* Attempts to parse a biome-color mappings from a text buffer.
 * The parser makes one attempt per line and is not very picky regarding a
 * combination of biomeID/name with a color, represented as either a single
 * number or as a triplet in decimal or as hex (preceeded by 0x or #).
 * Returns the number of successfully mapped biome ids
 */
int parseBiomeColors(unsigned char biomeColors[256][3], const char *buf);

int biomesToImage(unsigned char *pixels,
        unsigned char biomeColors[256][3], const int *biomes,
        const unsigned int sx, const unsigned int sy,
        const unsigned int pixscale, const int flip);

/* Save the pixel buffer (e.g. from biomesToImage) to the given path as an PPM
 * image file. Returns 0 if successful, or -1 if the file could not be opened,
 * or 1 if not all the pixel data could be written to the file.
 */
int savePPM(const char* path, const unsigned char *pixels,
        const unsigned int sx, const unsigned int sy);

#ifdef __cplusplus
}
#endif

#endif
