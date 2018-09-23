#include <stddef.h>
#include <stdint.h>

#include "decompress.h"

#define MARKER_BITS (0x10)

typedef struct {
  uint32_t bits;
  uint32_t value;
} _marker;

static inline void read_marker(uint8_t **srcpp, _marker *out) {
  out->bits = MARKER_BITS;
  out->value = ((*srcpp)[1] << 8) | (*srcpp)[0];
  (*srcpp) += 2;
}

static inline uint32_t check_marker(uint8_t **srcpp, _marker *m) {
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
    if (check_marker(&srcp, &m)) {
      *destp++ = *srcp++;
      continue;
    }

    /* loc_1DE124 */
    if (!check_marker(&srcp, &m)) {
      uint32_t a, b;

      a = check_marker(&srcp, &m) << 1;
      b = check_marker(&srcp, &m);

      x = 0xffffff00 | *srcp++;
      l = (a | b) + 3;
    } else { /* loc_1DE1C8 */
      x = (((srcp[1] & 0xf0) << 4) - 0x1000) | srcp[0];
      l = (srcp[1] & 0x0f) + 3;
      srcp += 2;

      if (l == 3) {
        uint8_t n = *srcp++;

        l = n + 1;

        if (n == 0) { /* end of source data */
          if (outz) /* how much z was sourced */
            *outz = (size_t)(srcp - src);
          if (outa) /* how much was decompresed */
            *outa = (size_t)(destp - dest);

          return;
        }
        /* fall into copy */
      }
    }

    /* loc_1DE21C */
    copyp = destp + (int32_t)x; /* wraps */
    while (l--) /* loc_1DE22C */
      *destp++ = *copyp++;
  }
}
