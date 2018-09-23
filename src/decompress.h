#pragma once

#include <stddef.h>
#include <stdint.h>

#define DECOMPRESS_MAX_BLOCK_A (65535)

void decompress(uint8_t *src, uint8_t* dest, size_t *outz, size_t *outa);

