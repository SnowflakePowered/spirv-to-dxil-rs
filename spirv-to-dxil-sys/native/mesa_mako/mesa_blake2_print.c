#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "mesa-blake3.h"

static void
blake3_to_uint32(const blake3_hash blake3,
                 uint32_t out[BLAKE3_OUT_LEN32])
{
   memset(out, 0, BLAKE3_OUT_LEN);

   for (unsigned i = 0; i < BLAKE3_OUT_LEN; i++)
      out[i / 4] |= (uint32_t)blake3[i] << ((i % 4) * 8);
}

void
_mesa_blake3_print(FILE *f, const blake3_hash blake3)
{
   uint32_t u32[BLAKE3_OUT_LEN32];
   blake3_to_uint32(blake3, u32);
   for (unsigned i = 0; i < BLAKE3_OUT_LEN32; i++) {
      fprintf(f, i ? ", 0x%08" PRIx32 : "0x%08" PRIx32, u32[i]);
   }
}