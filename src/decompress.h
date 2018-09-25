#pragma once

#include <stddef.h>
#include <stdint.h>

#define DECOMPRESS_MAX_ZBLOCK_A (65535)

/* decompress function reverse engineered from Star Wars: Bounty Hunter (PS2.)
   An LZSS variant, with the maximum `outa` value being 65535 per-call to this function.
   If `src` should decompress into more than 65535 octets, it will be broken up "blocks."
   Each call per-block should preserve `dest` until the final block is reached. This .. 
   .. is because a decompress call may reach into the previous blocks `dest` to copy from. */
void decompress(uint8_t *src, uint8_t* dest, size_t *outz, size_t *outa);
/* src  - the compressed data
   dest - the decompressed data
   outz - NULL, or save how many octets sourced
   outa - NULL, or save how many octets decompressed. Will not exceed 65535 */

