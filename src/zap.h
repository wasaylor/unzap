#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Star Wars: Bounty Hunter ZAP file structure defs
   all in little-endian */

#define ZAP_HEADER_MAGIC ((uint64_t)0x0123456789abcdef)
#define ENT_SIZE_EMPTY ((uint32_t)0xdcdcdcdc)

typedef struct {
  uint64_t magic;   /* ZAP_HEADER_MAGIC */
  uint32_t numents; /* number of ZapEnts */
  uint32_t unkx0c;  /* 0 */
} ZapHeader;

typedef struct {
  struct {
    uint32_t moffset;     /* in-game memory offset, irrelevant here */
    struct {
      uint32_t a;         /* decompress size, eq to z if not compress */
      uint32_t z;         /* zap (compress) size or ENT_SIZE_EMPTY */
      uint32_t zblocks[0x21]; /* z broken into smaller pieces - per call to decompress(...) */
    } size;
    uint32_t pad;         /* aligns size.z by 0x10 w/ (uint8_t)0xdc */
    uint32_t unkx94;      /* 0 */
  } meta;
  char name[0x100];       /* C string */
} ZapEnt;

/* _not_ part of the file structure
   */
typedef struct {
  size_t st_size; /* total size of the file from fstat */
  ZapHeader *header;
  ZapEnt *ents;
} Bundle;

bool unzap(void (*cb)(ZapEnt*, const char*), const char *prefix, Bundle *b);
void zap_free(Bundle *b);
Bundle* zap_open_file(const char *path);
