#include <stddef.h>
#include <stdint.h>

#include "decompress.h"

#ifdef UNBHLZ
/* stand-alone decompress program */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ZBLOCK (0x8000)

int main(int argc, char *argv[]) {
  uint8_t *src, *srcp, *dest, *destp;
  size_t read, z, a;
  unsigned b = 1; /* block counter */
  int ret;

  ret = 0;

  srcp = src = malloc((size_t)(ZBLOCK * 1.5f));
  destp = dest = malloc(DECOMPRESS_MAX_ZBLOCK_A);

  while ((read = fread(srcp, sizeof(uint8_t), ZBLOCK, stdin)) > 0) {
    decompress(src, destp, &z, &a);

    if (!(a > 0 && z > 0)) { 
      /* Nothing happened */
      ret = 1;
      break;
    }

    if (fwrite(destp, sizeof(uint8_t), a, stdout) != a) {
      /* Write failure */
      ret = 2;
      break;
    }

    dest = realloc(dest, DECOMPRESS_MAX_ZBLOCK_A * ++b); /* Allocate space for another block */
    destp += (ptrdiff_t)a; /* `ptr` for fwrite */

    if (z < read) { /* Decompressed less than what was read */
      size_t w = (read - z);

      /* Move what we have of the next block to base of src */
      memcpy(src, src + (ptrdiff_t)z, w);
      srcp = src + (ptrdiff_t)w; /* `ptr` for fread */
    } else { /* z >= read */
      srcp = src;
    }

    if (feof(stdin) && srcp != src) {
      /* Incomplete src */
      ret = 3;
      break;
    }
  }

  free(src);
  free(dest);

  return ret;
}
#endif

#define MARKER_BITS (16)

typedef struct {
  uint32_t bits;
  uint32_t value;
} _marker;

static inline void read_marker(uint8_t **srcpp, _marker *out) {
  out->bits = MARKER_BITS;
  out->value = (*srcpp)[1] << 8 | (*srcpp)[0];
  (*srcpp) += 2;
}

/* Gets the next bit in the marker
   reads the next marker if bits exhaust */
static inline uint32_t getbit(uint8_t **srcpp, _marker *m) {
  uint32_t b;

  b = m->value & 1;
  m->value >>= 1; 
  --m->bits;

  if (m->bits == 0)
    read_marker(srcpp, m);

  return b;
}

/* sub_001DE0B0 */
void decompress(uint8_t *src, uint8_t *dest, size_t *outz, size_t *outa)  {
  _marker m = {0};
  uint8_t *srcp, *destp, *copyp;
  uint32_t x, l;

  srcp = src;
  destp = dest;

  read_marker(&srcp, &m);

  while (1) { /* loc_1DE0DC */
    if (getbit(&srcp, &m)) { 
      *destp++ = *srcp++; /* literal */
      continue;
    }

    /* loc_1DE124 */
    if (!getbit(&srcp, &m)) {
      uint32_t a, b;

      a = getbit(&srcp, &m);
      b = getbit(&srcp, &m);

      x = 0xffffff00 | *srcp++;
      l = (a << 1 | b) + 3;
    } else { /* loc_1DE1C8 */
      x = (((srcp[1] & 0xf0) << 4) - 0x1000) | srcp[0];
      l = (srcp[1] & 0x0f) + 3;
      srcp += 2;

      if (l == 3) { /* possibly end of src */
        if (*srcp == 0) { /* end of src */
          if (outz) /* how much of src was consumed */
            *outz = (size_t)(srcp - src);
          if (outa) /* how much was decompresed */
            *outa = (size_t)(destp - dest);
          return;
        }

        l = 1 + *srcp++;
        /* fall into copy */
      }
    }

    /* loc_1DE21C */
    copyp = destp + (int32_t)x; /* wraps */
    while (l--) /* loc_1DE22C */
      *destp++ = *copyp++;
  }
}
