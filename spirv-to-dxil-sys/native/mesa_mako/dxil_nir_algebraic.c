#include "dxil_nir.h"

#include "nir.h"
#include "nir_builder.h"
#include "nir_search.h"
#include "nir_search_helpers.h"

/* What follows is NIR algebraic transform code for the following 120
 * transforms:
 *    ('u2u16', ('u2u8', 'a@16')) => ('iand', 'a', 255)
 *    ('u2u16', ('u2u8', 'a@32')) => ('u2u16', ('iand', 'a', 255))
 *    ('u2u16', ('u2u8', 'a@64')) => ('u2u16', ('iand', 'a', 255))
 *    ('u2u16', ('i2i8', 'a@16')) => ('u2u16', ('iand', 'a', 255))
 *    ('u2u16', ('i2i8', 'a@32')) => ('u2u16', ('iand', 'a', 255))
 *    ('u2u16', ('i2i8', 'a@64')) => ('u2u16', ('iand', 'a', 255))
 *    ('u2u16', ('f2u8', 'a')) => ('iand', ('f2u16', 'a'), 255)
 *    ('u2u16', ('f2i8', 'a')) => ('iand', ('f2i16', 'a'), 255)
 *    ('u2u32', ('u2u8', 'a@16')) => ('u2u32', ('iand', 'a', 255))
 *    ('u2u32', ('u2u8', 'a@32')) => ('iand', 'a', 255)
 *    ('u2u32', ('u2u8', 'a@64')) => ('u2u32', ('iand', 'a', 255))
 *    ('u2u32', ('i2i8', 'a@16')) => ('u2u32', ('iand', 'a', 255))
 *    ('u2u32', ('i2i8', 'a@32')) => ('u2u32', ('iand', 'a', 255))
 *    ('u2u32', ('i2i8', 'a@64')) => ('u2u32', ('iand', 'a', 255))
 *    ('u2u32', ('f2u8', 'a')) => ('iand', ('f2u32', 'a'), 255)
 *    ('u2u32', ('f2i8', 'a')) => ('iand', ('f2i32', 'a'), 255)
 *    ('u2u64', ('u2u8', 'a@16')) => ('u2u64', ('iand', 'a', 255))
 *    ('u2u64', ('u2u8', 'a@32')) => ('u2u64', ('iand', 'a', 255))
 *    ('u2u64', ('u2u8', 'a@64')) => ('iand', 'a', 255)
 *    ('u2u64', ('i2i8', 'a@16')) => ('u2u64', ('iand', 'a', 255))
 *    ('u2u64', ('i2i8', 'a@32')) => ('u2u64', ('iand', 'a', 255))
 *    ('u2u64', ('i2i8', 'a@64')) => ('u2u64', ('iand', 'a', 255))
 *    ('u2u64', ('f2u8', 'a')) => ('iand', ('f2u64', 'a'), 255)
 *    ('u2u64', ('f2i8', 'a')) => ('iand', ('f2i64', 'a'), 255)
 *    ('i2i16', ('u2u8', 'a@16')) => ('i2i16', ('ishr', ('ishl', 'a', 8), 8))
 *    ('i2i16', ('u2u8', 'a@32')) => ('i2i16', ('ishr', ('ishl', 'a', 24), 24))
 *    ('i2i16', ('u2u8', 'a@64')) => ('i2i16', ('ishr', ('ishl', 'a', 56), 56))
 *    ('i2i16', ('i2i8', 'a@16')) => ('ishr', ('ishl', 'a', 8), 8)
 *    ('i2i16', ('i2i8', 'a@32')) => ('i2i16', ('ishr', ('ishl', 'a', 24), 24))
 *    ('i2i16', ('i2i8', 'a@64')) => ('i2i16', ('ishr', ('ishl', 'a', 56), 56))
 *    ('i2i16', ('f2u8', 'a')) => ('ishr', ('ishl', ('f2u16', 'a'), 8), 8)
 *    ('i2i16', ('f2i8', 'a')) => ('ishr', ('ishl', ('f2i16', 'a'), 8), 8)
 *    ('i2i32', ('u2u8', 'a@16')) => ('i2i32', ('ishr', ('ishl', 'a', 8), 8))
 *    ('i2i32', ('u2u8', 'a@32')) => ('i2i32', ('ishr', ('ishl', 'a', 24), 24))
 *    ('i2i32', ('u2u8', 'a@64')) => ('i2i32', ('ishr', ('ishl', 'a', 56), 56))
 *    ('i2i32', ('i2i8', 'a@16')) => ('i2i32', ('ishr', ('ishl', 'a', 8), 8))
 *    ('i2i32', ('i2i8', 'a@32')) => ('ishr', ('ishl', 'a', 24), 24)
 *    ('i2i32', ('i2i8', 'a@64')) => ('i2i32', ('ishr', ('ishl', 'a', 56), 56))
 *    ('i2i32', ('f2u8', 'a')) => ('ishr', ('ishl', ('f2u32', 'a'), 24), 24)
 *    ('i2i32', ('f2i8', 'a')) => ('ishr', ('ishl', ('f2i32', 'a'), 24), 24)
 *    ('i2i64', ('u2u8', 'a@16')) => ('i2i64', ('ishr', ('ishl', 'a', 8), 8))
 *    ('i2i64', ('u2u8', 'a@32')) => ('i2i64', ('ishr', ('ishl', 'a', 24), 24))
 *    ('i2i64', ('u2u8', 'a@64')) => ('i2i64', ('ishr', ('ishl', 'a', 56), 56))
 *    ('i2i64', ('i2i8', 'a@16')) => ('i2i64', ('ishr', ('ishl', 'a', 8), 8))
 *    ('i2i64', ('i2i8', 'a@32')) => ('i2i64', ('ishr', ('ishl', 'a', 24), 24))
 *    ('i2i64', ('i2i8', 'a@64')) => ('ishr', ('ishl', 'a', 56), 56)
 *    ('i2i64', ('f2u8', 'a')) => ('ishr', ('ishl', ('f2u64', 'a'), 56), 56)
 *    ('i2i64', ('f2i8', 'a')) => ('ishr', ('ishl', ('f2i64', 'a'), 56), 56)
 *    ('u2f16', ('u2u8', 'a@16')) => ('u2f16', ('iand', 'a', 255))
 *    ('u2f16', ('u2u8', 'a@32')) => ('u2f16', ('iand', 'a', 255))
 *    ('u2f16', ('u2u8', 'a@64')) => ('u2f16', ('iand', 'a', 255))
 *    ('u2f16', ('i2i8', 'a@16')) => ('u2f16', ('iand', 'a', 255))
 *    ('u2f16', ('i2i8', 'a@32')) => ('u2f16', ('iand', 'a', 255))
 *    ('u2f16', ('i2i8', 'a@64')) => ('u2f16', ('iand', 'a', 255))
 *    ('u2f16', ('f2u8', 'a@16')) => ('fmin', ('fmax', 'a', 0.0), 255.0)
 *    ('u2f16', ('f2u8', 'a@32')) => ('f2f16', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f16', ('f2u8', 'a@64')) => ('f2f16', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f16', ('f2i8', 'a@16')) => ('fmin', ('fmax', 'a', 0.0), 255.0)
 *    ('u2f16', ('f2i8', 'a@32')) => ('f2f16', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f16', ('f2i8', 'a@64')) => ('f2f16', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f32', ('u2u8', 'a@16')) => ('u2f32', ('iand', 'a', 255))
 *    ('u2f32', ('u2u8', 'a@32')) => ('u2f32', ('iand', 'a', 255))
 *    ('u2f32', ('u2u8', 'a@64')) => ('u2f32', ('iand', 'a', 255))
 *    ('u2f32', ('i2i8', 'a@16')) => ('u2f32', ('iand', 'a', 255))
 *    ('u2f32', ('i2i8', 'a@32')) => ('u2f32', ('iand', 'a', 255))
 *    ('u2f32', ('i2i8', 'a@64')) => ('u2f32', ('iand', 'a', 255))
 *    ('u2f32', ('f2u8', 'a@16')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f32', ('f2u8', 'a@32')) => ('fmin', ('fmax', 'a', 0.0), 255.0)
 *    ('u2f32', ('f2u8', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f32', ('f2i8', 'a@16')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f32', ('f2i8', 'a@32')) => ('fmin', ('fmax', 'a', 0.0), 255.0)
 *    ('u2f32', ('f2i8', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f64', ('u2u8', 'a@16')) => ('u2f64', ('iand', 'a', 255))
 *    ('u2f64', ('u2u8', 'a@32')) => ('u2f64', ('iand', 'a', 255))
 *    ('u2f64', ('u2u8', 'a@64')) => ('u2f64', ('iand', 'a', 255))
 *    ('u2f64', ('i2i8', 'a@16')) => ('u2f64', ('iand', 'a', 255))
 *    ('u2f64', ('i2i8', 'a@32')) => ('u2f64', ('iand', 'a', 255))
 *    ('u2f64', ('i2i8', 'a@64')) => ('u2f64', ('iand', 'a', 255))
 *    ('u2f64', ('f2u8', 'a@16')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f64', ('f2u8', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f64', ('f2u8', 'a@64')) => ('fmin', ('fmax', 'a', 0.0), 255.0)
 *    ('u2f64', ('f2i8', 'a@16')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f64', ('f2i8', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 255.0))
 *    ('u2f64', ('f2i8', 'a@64')) => ('fmin', ('fmax', 'a', 0.0), 255.0)
 *    ('i2f16', ('u2u8', 'a@16')) => ('i2f16', ('ishr', ('ishl', 'a', 8), 8))
 *    ('i2f16', ('u2u8', 'a@32')) => ('i2f16', ('ishr', ('ishl', 'a', 24), 24))
 *    ('i2f16', ('u2u8', 'a@64')) => ('i2f16', ('ishr', ('ishl', 'a', 56), 56))
 *    ('i2f16', ('i2i8', 'a@16')) => ('i2f16', ('ishr', ('ishl', 'a', 8), 8))
 *    ('i2f16', ('i2i8', 'a@32')) => ('i2f16', ('ishr', ('ishl', 'a', 24), 24))
 *    ('i2f16', ('i2i8', 'a@64')) => ('i2f16', ('ishr', ('ishl', 'a', 56), 56))
 *    ('i2f16', ('f2u8', 'a@16')) => ('fmin', ('fmax', 'a', -128.0), 127.0)
 *    ('i2f16', ('f2u8', 'a@32')) => ('f2f16', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f16', ('f2u8', 'a@64')) => ('f2f16', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f16', ('f2i8', 'a@16')) => ('fmin', ('fmax', 'a', -128.0), 127.0)
 *    ('i2f16', ('f2i8', 'a@32')) => ('f2f16', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f16', ('f2i8', 'a@64')) => ('f2f16', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f32', ('u2u8', 'a@16')) => ('i2f32', ('ishr', ('ishl', 'a', 8), 8))
 *    ('i2f32', ('u2u8', 'a@32')) => ('i2f32', ('ishr', ('ishl', 'a', 24), 24))
 *    ('i2f32', ('u2u8', 'a@64')) => ('i2f32', ('ishr', ('ishl', 'a', 56), 56))
 *    ('i2f32', ('i2i8', 'a@16')) => ('i2f32', ('ishr', ('ishl', 'a', 8), 8))
 *    ('i2f32', ('i2i8', 'a@32')) => ('i2f32', ('ishr', ('ishl', 'a', 24), 24))
 *    ('i2f32', ('i2i8', 'a@64')) => ('i2f32', ('ishr', ('ishl', 'a', 56), 56))
 *    ('i2f32', ('f2u8', 'a@16')) => ('f2f32', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f32', ('f2u8', 'a@32')) => ('fmin', ('fmax', 'a', -128.0), 127.0)
 *    ('i2f32', ('f2u8', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f32', ('f2i8', 'a@16')) => ('f2f32', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f32', ('f2i8', 'a@32')) => ('fmin', ('fmax', 'a', -128.0), 127.0)
 *    ('i2f32', ('f2i8', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f64', ('u2u8', 'a@16')) => ('i2f64', ('ishr', ('ishl', 'a', 8), 8))
 *    ('i2f64', ('u2u8', 'a@32')) => ('i2f64', ('ishr', ('ishl', 'a', 24), 24))
 *    ('i2f64', ('u2u8', 'a@64')) => ('i2f64', ('ishr', ('ishl', 'a', 56), 56))
 *    ('i2f64', ('i2i8', 'a@16')) => ('i2f64', ('ishr', ('ishl', 'a', 8), 8))
 *    ('i2f64', ('i2i8', 'a@32')) => ('i2f64', ('ishr', ('ishl', 'a', 24), 24))
 *    ('i2f64', ('i2i8', 'a@64')) => ('i2f64', ('ishr', ('ishl', 'a', 56), 56))
 *    ('i2f64', ('f2u8', 'a@16')) => ('f2f64', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f64', ('f2u8', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f64', ('f2u8', 'a@64')) => ('fmin', ('fmax', 'a', -128.0), 127.0)
 *    ('i2f64', ('f2i8', 'a@16')) => ('f2f64', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f64', ('f2i8', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', -128.0), 127.0))
 *    ('i2f64', ('f2i8', 'a@64')) => ('fmin', ('fmax', 'a', -128.0), 127.0)
 */


static const nir_search_value_union dxil_nir_lower_8bit_conv_values[] = {
   /* ('u2u16', ('u2u8', 'a@16')) => ('iand', 'a', 255) */
   { .variable = {
      { nir_search_value_variable, 16 },
      0, /* a */
      false,
      nir_type_invalid,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_u2u8,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace0_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, 16 },
      nir_type_int, { 0xff /* 255 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 0, 3 },
      -1,
   } },

   /* ('u2u16', ('u2u8', 'a@32')) => ('u2u16', ('iand', 'a', 255)) */
   { .variable = {
      { nir_search_value_variable, 32 },
      0, /* a */
      false,
      nir_type_invalid,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_u2u8,
      -1, 0,
      { 5 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace1_0_0 -> 5 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0xff /* 255 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 5, 8 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 1,
      { 9 },
      -1,
   } },

   /* ('u2u16', ('u2u8', 'a@64')) => ('u2u16', ('iand', 'a', 255)) */
   { .variable = {
      { nir_search_value_variable, 64 },
      0, /* a */
      false,
      nir_type_invalid,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_u2u8,
      -1, 0,
      { 11 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace2_0_0 -> 11 in the cache */
   { .constant = {
      { nir_search_value_constant, 64 },
      nir_type_int, { 0xff /* 255 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 11, 14 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 1,
      { 15 },
      -1,
   } },

   /* ('u2u16', ('i2i8', 'a@16')) => ('u2u16', ('iand', 'a', 255)) */
   /* search3_0_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_i2i8,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace3_0_0 -> 0 in the cache */
   /* replace3_0_1 -> 3 in the cache */
   /* replace3_0 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 1,
      { 4 },
      -1,
   } },

   /* ('u2u16', ('i2i8', 'a@32')) => ('u2u16', ('iand', 'a', 255)) */
   /* search4_0_0 -> 5 in the cache */
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_i2i8,
      -1, 0,
      { 5 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace4_0_0 -> 5 in the cache */
   /* replace4_0_1 -> 8 in the cache */
   /* replace4_0 -> 9 in the cache */
   /* replace4 -> 10 in the cache */

   /* ('u2u16', ('i2i8', 'a@64')) => ('u2u16', ('iand', 'a', 255)) */
   /* search5_0_0 -> 11 in the cache */
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_i2i8,
      -1, 0,
      { 11 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace5_0_0 -> 11 in the cache */
   /* replace5_0_1 -> 14 in the cache */
   /* replace5_0 -> 15 in the cache */
   /* replace5 -> 16 in the cache */

   /* ('u2u16', ('f2u8', 'a')) => ('iand', ('f2u16', 'a'), 255) */
   { .variable = {
      { nir_search_value_variable, -1 },
      0, /* a */
      false,
      nir_type_invalid,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_f2u8,
      -1, 0,
      { 24 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 0,
      { 25 },
      -1,
   } },

   /* replace6_0_0 -> 24 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2u16,
      -1, 0,
      { 24 },
      -1,
   } },
   /* replace6_1 -> 3 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 27, 3 },
      -1,
   } },

   /* ('u2u16', ('f2i8', 'a')) => ('iand', ('f2i16', 'a'), 255) */
   /* search7_0_0 -> 24 in the cache */
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_f2i8,
      -1, 0,
      { 24 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 0,
      { 29 },
      -1,
   } },

   /* replace7_0_0 -> 24 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2i16,
      -1, 0,
      { 24 },
      -1,
   } },
   /* replace7_1 -> 3 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 31, 3 },
      -1,
   } },

   /* ('u2u32', ('u2u8', 'a@16')) => ('u2u32', ('iand', 'a', 255)) */
   /* search8_0_0 -> 0 in the cache */
   /* search8_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace8_0_0 -> 0 in the cache */
   /* replace8_0_1 -> 3 in the cache */
   /* replace8_0 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 1,
      { 4 },
      -1,
   } },

   /* ('u2u32', ('u2u8', 'a@32')) => ('iand', 'a', 255) */
   /* search9_0_0 -> 5 in the cache */
   /* search9_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace9_0 -> 5 in the cache */
   /* replace9_1 -> 8 in the cache */
   /* replace9 -> 9 in the cache */

   /* ('u2u32', ('u2u8', 'a@64')) => ('u2u32', ('iand', 'a', 255)) */
   /* search10_0_0 -> 11 in the cache */
   /* search10_0 -> 12 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace10_0_0 -> 11 in the cache */
   /* replace10_0_1 -> 14 in the cache */
   /* replace10_0 -> 15 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 1,
      { 15 },
      -1,
   } },

   /* ('u2u32', ('i2i8', 'a@16')) => ('u2u32', ('iand', 'a', 255)) */
   /* search11_0_0 -> 0 in the cache */
   /* search11_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace11_0_0 -> 0 in the cache */
   /* replace11_0_1 -> 3 in the cache */
   /* replace11_0 -> 4 in the cache */
   /* replace11 -> 34 in the cache */

   /* ('u2u32', ('i2i8', 'a@32')) => ('u2u32', ('iand', 'a', 255)) */
   /* search12_0_0 -> 5 in the cache */
   /* search12_0 -> 20 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace12_0_0 -> 5 in the cache */
   /* replace12_0_1 -> 8 in the cache */
   /* replace12_0 -> 9 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 1,
      { 9 },
      -1,
   } },

   /* ('u2u32', ('i2i8', 'a@64')) => ('u2u32', ('iand', 'a', 255)) */
   /* search13_0_0 -> 11 in the cache */
   /* search13_0 -> 22 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace13_0_0 -> 11 in the cache */
   /* replace13_0_1 -> 14 in the cache */
   /* replace13_0 -> 15 in the cache */
   /* replace13 -> 37 in the cache */

   /* ('u2u32', ('f2u8', 'a')) => ('iand', ('f2u32', 'a'), 255) */
   /* search14_0_0 -> 24 in the cache */
   /* search14_0 -> 25 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 25 },
      -1,
   } },

   /* replace14_0_0 -> 24 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_f2u32,
      -1, 0,
      { 24 },
      -1,
   } },
   /* replace14_1 -> 8 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 43, 8 },
      -1,
   } },

   /* ('u2u32', ('f2i8', 'a')) => ('iand', ('f2i32', 'a'), 255) */
   /* search15_0_0 -> 24 in the cache */
   /* search15_0 -> 29 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 29 },
      -1,
   } },

   /* replace15_0_0 -> 24 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_f2i32,
      -1, 0,
      { 24 },
      -1,
   } },
   /* replace15_1 -> 8 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 46, 8 },
      -1,
   } },

   /* ('u2u64', ('u2u8', 'a@16')) => ('u2u64', ('iand', 'a', 255)) */
   /* search16_0_0 -> 0 in the cache */
   /* search16_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace16_0_0 -> 0 in the cache */
   /* replace16_0_1 -> 3 in the cache */
   /* replace16_0 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 1,
      { 4 },
      -1,
   } },

   /* ('u2u64', ('u2u8', 'a@32')) => ('u2u64', ('iand', 'a', 255)) */
   /* search17_0_0 -> 5 in the cache */
   /* search17_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace17_0_0 -> 5 in the cache */
   /* replace17_0_1 -> 8 in the cache */
   /* replace17_0 -> 9 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 1,
      { 9 },
      -1,
   } },

   /* ('u2u64', ('u2u8', 'a@64')) => ('iand', 'a', 255) */
   /* search18_0_0 -> 11 in the cache */
   /* search18_0 -> 12 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace18_0 -> 11 in the cache */
   /* replace18_1 -> 14 in the cache */
   /* replace18 -> 15 in the cache */

   /* ('u2u64', ('i2i8', 'a@16')) => ('u2u64', ('iand', 'a', 255)) */
   /* search19_0_0 -> 0 in the cache */
   /* search19_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace19_0_0 -> 0 in the cache */
   /* replace19_0_1 -> 3 in the cache */
   /* replace19_0 -> 4 in the cache */
   /* replace19 -> 49 in the cache */

   /* ('u2u64', ('i2i8', 'a@32')) => ('u2u64', ('iand', 'a', 255)) */
   /* search20_0_0 -> 5 in the cache */
   /* search20_0 -> 20 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace20_0_0 -> 5 in the cache */
   /* replace20_0_1 -> 8 in the cache */
   /* replace20_0 -> 9 in the cache */
   /* replace20 -> 51 in the cache */

   /* ('u2u64', ('i2i8', 'a@64')) => ('u2u64', ('iand', 'a', 255)) */
   /* search21_0_0 -> 11 in the cache */
   /* search21_0 -> 22 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace21_0_0 -> 11 in the cache */
   /* replace21_0_1 -> 14 in the cache */
   /* replace21_0 -> 15 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 1,
      { 15 },
      -1,
   } },

   /* ('u2u64', ('f2u8', 'a')) => ('iand', ('f2u64', 'a'), 255) */
   /* search22_0_0 -> 24 in the cache */
   /* search22_0 -> 25 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 25 },
      -1,
   } },

   /* replace22_0_0 -> 24 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_f2u64,
      -1, 0,
      { 24 },
      -1,
   } },
   /* replace22_1 -> 14 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 58, 14 },
      -1,
   } },

   /* ('u2u64', ('f2i8', 'a')) => ('iand', ('f2i64', 'a'), 255) */
   /* search23_0_0 -> 24 in the cache */
   /* search23_0 -> 29 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 29 },
      -1,
   } },

   /* replace23_0_0 -> 24 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_f2i64,
      -1, 0,
      { 24 },
      -1,
   } },
   /* replace23_1 -> 14 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 61, 14 },
      -1,
   } },

   /* ('i2i16', ('u2u8', 'a@16')) => ('i2i16', ('ishr', ('ishl', 'a', 8), 8)) */
   /* search24_0_0 -> 0 in the cache */
   /* search24_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace24_0_0_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0x8 /* 8 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 0, 64 },
      -1,
   } },
   /* replace24_0_1 -> 64 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 65, 64 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 66 },
      -1,
   } },

   /* ('i2i16', ('u2u8', 'a@32')) => ('i2i16', ('ishr', ('ishl', 'a', 24), 24)) */
   /* search25_0_0 -> 5 in the cache */
   /* search25_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace25_0_0_0 -> 5 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0x18 /* 24 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 5, 69 },
      -1,
   } },
   /* replace25_0_1 -> 69 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 70, 69 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 71 },
      -1,
   } },

   /* ('i2i16', ('u2u8', 'a@64')) => ('i2i16', ('ishr', ('ishl', 'a', 56), 56)) */
   /* search26_0_0 -> 11 in the cache */
   /* search26_0 -> 12 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace26_0_0_0 -> 11 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0x38 /* 56 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 11, 74 },
      -1,
   } },
   /* replace26_0_1 -> 74 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 75, 74 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 76 },
      -1,
   } },

   /* ('i2i16', ('i2i8', 'a@16')) => ('ishr', ('ishl', 'a', 8), 8) */
   /* search27_0_0 -> 0 in the cache */
   /* search27_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace27_0_0 -> 0 in the cache */
   /* replace27_0_1 -> 64 in the cache */
   /* replace27_0 -> 65 in the cache */
   /* replace27_1 -> 64 in the cache */
   /* replace27 -> 66 in the cache */

   /* ('i2i16', ('i2i8', 'a@32')) => ('i2i16', ('ishr', ('ishl', 'a', 24), 24)) */
   /* search28_0_0 -> 5 in the cache */
   /* search28_0 -> 20 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace28_0_0_0 -> 5 in the cache */
   /* replace28_0_0_1 -> 69 in the cache */
   /* replace28_0_0 -> 70 in the cache */
   /* replace28_0_1 -> 69 in the cache */
   /* replace28_0 -> 71 in the cache */
   /* replace28 -> 72 in the cache */

   /* ('i2i16', ('i2i8', 'a@64')) => ('i2i16', ('ishr', ('ishl', 'a', 56), 56)) */
   /* search29_0_0 -> 11 in the cache */
   /* search29_0 -> 22 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace29_0_0_0 -> 11 in the cache */
   /* replace29_0_0_1 -> 74 in the cache */
   /* replace29_0_0 -> 75 in the cache */
   /* replace29_0_1 -> 74 in the cache */
   /* replace29_0 -> 76 in the cache */
   /* replace29 -> 77 in the cache */

   /* ('i2i16', ('f2u8', 'a')) => ('ishr', ('ishl', ('f2u16', 'a'), 8), 8) */
   /* search30_0_0 -> 24 in the cache */
   /* search30_0 -> 25 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 25 },
      -1,
   } },

   /* replace30_0_0_0 -> 24 in the cache */
   /* replace30_0_0 -> 27 in the cache */
   /* replace30_0_1 -> 64 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 27, 64 },
      -1,
   } },
   /* replace30_1 -> 64 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 82, 64 },
      -1,
   } },

   /* ('i2i16', ('f2i8', 'a')) => ('ishr', ('ishl', ('f2i16', 'a'), 8), 8) */
   /* search31_0_0 -> 24 in the cache */
   /* search31_0 -> 29 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 29 },
      -1,
   } },

   /* replace31_0_0_0 -> 24 in the cache */
   /* replace31_0_0 -> 31 in the cache */
   /* replace31_0_1 -> 64 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 31, 64 },
      -1,
   } },
   /* replace31_1 -> 64 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 85, 64 },
      -1,
   } },

   /* ('i2i32', ('u2u8', 'a@16')) => ('i2i32', ('ishr', ('ishl', 'a', 8), 8)) */
   /* search32_0_0 -> 0 in the cache */
   /* search32_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace32_0_0_0 -> 0 in the cache */
   /* replace32_0_0_1 -> 64 in the cache */
   /* replace32_0_0 -> 65 in the cache */
   /* replace32_0_1 -> 64 in the cache */
   /* replace32_0 -> 66 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 66 },
      -1,
   } },

   /* ('i2i32', ('u2u8', 'a@32')) => ('i2i32', ('ishr', ('ishl', 'a', 24), 24)) */
   /* search33_0_0 -> 5 in the cache */
   /* search33_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace33_0_0_0 -> 5 in the cache */
   /* replace33_0_0_1 -> 69 in the cache */
   /* replace33_0_0 -> 70 in the cache */
   /* replace33_0_1 -> 69 in the cache */
   /* replace33_0 -> 71 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 71 },
      -1,
   } },

   /* ('i2i32', ('u2u8', 'a@64')) => ('i2i32', ('ishr', ('ishl', 'a', 56), 56)) */
   /* search34_0_0 -> 11 in the cache */
   /* search34_0 -> 12 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace34_0_0_0 -> 11 in the cache */
   /* replace34_0_0_1 -> 74 in the cache */
   /* replace34_0_0 -> 75 in the cache */
   /* replace34_0_1 -> 74 in the cache */
   /* replace34_0 -> 76 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 76 },
      -1,
   } },

   /* ('i2i32', ('i2i8', 'a@16')) => ('i2i32', ('ishr', ('ishl', 'a', 8), 8)) */
   /* search35_0_0 -> 0 in the cache */
   /* search35_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace35_0_0_0 -> 0 in the cache */
   /* replace35_0_0_1 -> 64 in the cache */
   /* replace35_0_0 -> 65 in the cache */
   /* replace35_0_1 -> 64 in the cache */
   /* replace35_0 -> 66 in the cache */
   /* replace35 -> 88 in the cache */

   /* ('i2i32', ('i2i8', 'a@32')) => ('ishr', ('ishl', 'a', 24), 24) */
   /* search36_0_0 -> 5 in the cache */
   /* search36_0 -> 20 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace36_0_0 -> 5 in the cache */
   /* replace36_0_1 -> 69 in the cache */
   /* replace36_0 -> 70 in the cache */
   /* replace36_1 -> 69 in the cache */
   /* replace36 -> 71 in the cache */

   /* ('i2i32', ('i2i8', 'a@64')) => ('i2i32', ('ishr', ('ishl', 'a', 56), 56)) */
   /* search37_0_0 -> 11 in the cache */
   /* search37_0 -> 22 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace37_0_0_0 -> 11 in the cache */
   /* replace37_0_0_1 -> 74 in the cache */
   /* replace37_0_0 -> 75 in the cache */
   /* replace37_0_1 -> 74 in the cache */
   /* replace37_0 -> 76 in the cache */
   /* replace37 -> 92 in the cache */

   /* ('i2i32', ('f2u8', 'a')) => ('ishr', ('ishl', ('f2u32', 'a'), 24), 24) */
   /* search38_0_0 -> 24 in the cache */
   /* search38_0 -> 25 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 25 },
      -1,
   } },

   /* replace38_0_0_0 -> 24 in the cache */
   /* replace38_0_0 -> 43 in the cache */
   /* replace38_0_1 -> 69 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 43, 69 },
      -1,
   } },
   /* replace38_1 -> 69 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 97, 69 },
      -1,
   } },

   /* ('i2i32', ('f2i8', 'a')) => ('ishr', ('ishl', ('f2i32', 'a'), 24), 24) */
   /* search39_0_0 -> 24 in the cache */
   /* search39_0 -> 29 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 29 },
      -1,
   } },

   /* replace39_0_0_0 -> 24 in the cache */
   /* replace39_0_0 -> 46 in the cache */
   /* replace39_0_1 -> 69 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 46, 69 },
      -1,
   } },
   /* replace39_1 -> 69 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 100, 69 },
      -1,
   } },

   /* ('i2i64', ('u2u8', 'a@16')) => ('i2i64', ('ishr', ('ishl', 'a', 8), 8)) */
   /* search40_0_0 -> 0 in the cache */
   /* search40_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace40_0_0_0 -> 0 in the cache */
   /* replace40_0_0_1 -> 64 in the cache */
   /* replace40_0_0 -> 65 in the cache */
   /* replace40_0_1 -> 64 in the cache */
   /* replace40_0 -> 66 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 66 },
      -1,
   } },

   /* ('i2i64', ('u2u8', 'a@32')) => ('i2i64', ('ishr', ('ishl', 'a', 24), 24)) */
   /* search41_0_0 -> 5 in the cache */
   /* search41_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace41_0_0_0 -> 5 in the cache */
   /* replace41_0_0_1 -> 69 in the cache */
   /* replace41_0_0 -> 70 in the cache */
   /* replace41_0_1 -> 69 in the cache */
   /* replace41_0 -> 71 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 71 },
      -1,
   } },

   /* ('i2i64', ('u2u8', 'a@64')) => ('i2i64', ('ishr', ('ishl', 'a', 56), 56)) */
   /* search42_0_0 -> 11 in the cache */
   /* search42_0 -> 12 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace42_0_0_0 -> 11 in the cache */
   /* replace42_0_0_1 -> 74 in the cache */
   /* replace42_0_0 -> 75 in the cache */
   /* replace42_0_1 -> 74 in the cache */
   /* replace42_0 -> 76 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 76 },
      -1,
   } },

   /* ('i2i64', ('i2i8', 'a@16')) => ('i2i64', ('ishr', ('ishl', 'a', 8), 8)) */
   /* search43_0_0 -> 0 in the cache */
   /* search43_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace43_0_0_0 -> 0 in the cache */
   /* replace43_0_0_1 -> 64 in the cache */
   /* replace43_0_0 -> 65 in the cache */
   /* replace43_0_1 -> 64 in the cache */
   /* replace43_0 -> 66 in the cache */
   /* replace43 -> 103 in the cache */

   /* ('i2i64', ('i2i8', 'a@32')) => ('i2i64', ('ishr', ('ishl', 'a', 24), 24)) */
   /* search44_0_0 -> 5 in the cache */
   /* search44_0 -> 20 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace44_0_0_0 -> 5 in the cache */
   /* replace44_0_0_1 -> 69 in the cache */
   /* replace44_0_0 -> 70 in the cache */
   /* replace44_0_1 -> 69 in the cache */
   /* replace44_0 -> 71 in the cache */
   /* replace44 -> 105 in the cache */

   /* ('i2i64', ('i2i8', 'a@64')) => ('ishr', ('ishl', 'a', 56), 56) */
   /* search45_0_0 -> 11 in the cache */
   /* search45_0 -> 22 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace45_0_0 -> 11 in the cache */
   /* replace45_0_1 -> 74 in the cache */
   /* replace45_0 -> 75 in the cache */
   /* replace45_1 -> 74 in the cache */
   /* replace45 -> 76 in the cache */

   /* ('i2i64', ('f2u8', 'a')) => ('ishr', ('ishl', ('f2u64', 'a'), 56), 56) */
   /* search46_0_0 -> 24 in the cache */
   /* search46_0 -> 25 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 25 },
      -1,
   } },

   /* replace46_0_0_0 -> 24 in the cache */
   /* replace46_0_0 -> 58 in the cache */
   /* replace46_0_1 -> 74 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 58, 74 },
      -1,
   } },
   /* replace46_1 -> 74 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 112, 74 },
      -1,
   } },

   /* ('i2i64', ('f2i8', 'a')) => ('ishr', ('ishl', ('f2i64', 'a'), 56), 56) */
   /* search47_0_0 -> 24 in the cache */
   /* search47_0 -> 29 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 29 },
      -1,
   } },

   /* replace47_0_0_0 -> 24 in the cache */
   /* replace47_0_0 -> 61 in the cache */
   /* replace47_0_1 -> 74 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 61, 74 },
      -1,
   } },
   /* replace47_1 -> 74 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 115, 74 },
      -1,
   } },

   /* ('u2f16', ('u2u8', 'a@16')) => ('u2f16', ('iand', 'a', 255)) */
   /* search48_0_0 -> 0 in the cache */
   /* search48_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace48_0_0 -> 0 in the cache */
   /* replace48_0_1 -> 3 in the cache */
   /* replace48_0 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 1,
      { 4 },
      -1,
   } },

   /* ('u2f16', ('u2u8', 'a@32')) => ('u2f16', ('iand', 'a', 255)) */
   /* search49_0_0 -> 5 in the cache */
   /* search49_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace49_0_0 -> 5 in the cache */
   /* replace49_0_1 -> 8 in the cache */
   /* replace49_0 -> 9 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 1,
      { 9 },
      -1,
   } },

   /* ('u2f16', ('u2u8', 'a@64')) => ('u2f16', ('iand', 'a', 255)) */
   /* search50_0_0 -> 11 in the cache */
   /* search50_0 -> 12 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace50_0_0 -> 11 in the cache */
   /* replace50_0_1 -> 14 in the cache */
   /* replace50_0 -> 15 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 1,
      { 15 },
      -1,
   } },

   /* ('u2f16', ('i2i8', 'a@16')) => ('u2f16', ('iand', 'a', 255)) */
   /* search51_0_0 -> 0 in the cache */
   /* search51_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace51_0_0 -> 0 in the cache */
   /* replace51_0_1 -> 3 in the cache */
   /* replace51_0 -> 4 in the cache */
   /* replace51 -> 118 in the cache */

   /* ('u2f16', ('i2i8', 'a@32')) => ('u2f16', ('iand', 'a', 255)) */
   /* search52_0_0 -> 5 in the cache */
   /* search52_0 -> 20 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace52_0_0 -> 5 in the cache */
   /* replace52_0_1 -> 8 in the cache */
   /* replace52_0 -> 9 in the cache */
   /* replace52 -> 120 in the cache */

   /* ('u2f16', ('i2i8', 'a@64')) => ('u2f16', ('iand', 'a', 255)) */
   /* search53_0_0 -> 11 in the cache */
   /* search53_0 -> 22 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace53_0_0 -> 11 in the cache */
   /* replace53_0_1 -> 14 in the cache */
   /* replace53_0 -> 15 in the cache */
   /* replace53 -> 122 in the cache */

   /* ('u2f16', ('f2u8', 'a@16')) => ('fmin', ('fmax', 'a', 0.0), 255.0) */
   /* search54_0_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_f2u8,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 126 },
      -1,
   } },

   /* replace54_0_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, 16 },
      nir_type_float, { 0x0 /* 0.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_fmax,
      1, 1,
      { 0, 128 },
      -1,
   } },
   { .constant = {
      { nir_search_value_constant, 16 },
      nir_type_float, { 0x406fe00000000000 /* 255.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_fmin,
      0, 2,
      { 129, 130 },
      -1,
   } },

   /* ('u2f16', ('f2u8', 'a@32')) => ('f2f16', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search55_0_0 -> 5 in the cache */
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_f2u8,
      -1, 0,
      { 5 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 132 },
      -1,
   } },

   /* replace55_0_0_0 -> 5 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_float, { 0x0 /* 0.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_fmax,
      1, 1,
      { 5, 134 },
      -1,
   } },
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_float, { 0x406fe00000000000 /* 255.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_fmin,
      0, 2,
      { 135, 136 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2f16,
      -1, 2,
      { 137 },
      -1,
   } },

   /* ('u2f16', ('f2u8', 'a@64')) => ('f2f16', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search56_0_0 -> 11 in the cache */
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_f2u8,
      -1, 0,
      { 11 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 139 },
      -1,
   } },

   /* replace56_0_0_0 -> 11 in the cache */
   { .constant = {
      { nir_search_value_constant, 64 },
      nir_type_float, { 0x0 /* 0.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_fmax,
      1, 1,
      { 11, 141 },
      -1,
   } },
   { .constant = {
      { nir_search_value_constant, 64 },
      nir_type_float, { 0x406fe00000000000 /* 255.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_fmin,
      0, 2,
      { 142, 143 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2f16,
      -1, 2,
      { 144 },
      -1,
   } },

   /* ('u2f16', ('f2i8', 'a@16')) => ('fmin', ('fmax', 'a', 0.0), 255.0) */
   /* search57_0_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_f2i8,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 146 },
      -1,
   } },

   /* replace57_0_0 -> 0 in the cache */
   /* replace57_0_1 -> 128 in the cache */
   /* replace57_0 -> 129 in the cache */
   /* replace57_1 -> 130 in the cache */
   /* replace57 -> 131 in the cache */

   /* ('u2f16', ('f2i8', 'a@32')) => ('f2f16', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search58_0_0 -> 5 in the cache */
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_f2i8,
      -1, 0,
      { 5 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 148 },
      -1,
   } },

   /* replace58_0_0_0 -> 5 in the cache */
   /* replace58_0_0_1 -> 134 in the cache */
   /* replace58_0_0 -> 135 in the cache */
   /* replace58_0_1 -> 136 in the cache */
   /* replace58_0 -> 137 in the cache */
   /* replace58 -> 138 in the cache */

   /* ('u2f16', ('f2i8', 'a@64')) => ('f2f16', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search59_0_0 -> 11 in the cache */
   { .expression = {
      { nir_search_value_expression, 8 },
      false,
      false,
      false,
      nir_op_f2i8,
      -1, 0,
      { 11 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2f16,
      -1, 0,
      { 150 },
      -1,
   } },

   /* replace59_0_0_0 -> 11 in the cache */
   /* replace59_0_0_1 -> 141 in the cache */
   /* replace59_0_0 -> 142 in the cache */
   /* replace59_0_1 -> 143 in the cache */
   /* replace59_0 -> 144 in the cache */
   /* replace59 -> 145 in the cache */

   /* ('u2f32', ('u2u8', 'a@16')) => ('u2f32', ('iand', 'a', 255)) */
   /* search60_0_0 -> 0 in the cache */
   /* search60_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace60_0_0 -> 0 in the cache */
   /* replace60_0_1 -> 3 in the cache */
   /* replace60_0 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 1,
      { 4 },
      -1,
   } },

   /* ('u2f32', ('u2u8', 'a@32')) => ('u2f32', ('iand', 'a', 255)) */
   /* search61_0_0 -> 5 in the cache */
   /* search61_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace61_0_0 -> 5 in the cache */
   /* replace61_0_1 -> 8 in the cache */
   /* replace61_0 -> 9 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 1,
      { 9 },
      -1,
   } },

   /* ('u2f32', ('u2u8', 'a@64')) => ('u2f32', ('iand', 'a', 255)) */
   /* search62_0_0 -> 11 in the cache */
   /* search62_0 -> 12 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace62_0_0 -> 11 in the cache */
   /* replace62_0_1 -> 14 in the cache */
   /* replace62_0 -> 15 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 1,
      { 15 },
      -1,
   } },

   /* ('u2f32', ('i2i8', 'a@16')) => ('u2f32', ('iand', 'a', 255)) */
   /* search63_0_0 -> 0 in the cache */
   /* search63_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace63_0_0 -> 0 in the cache */
   /* replace63_0_1 -> 3 in the cache */
   /* replace63_0 -> 4 in the cache */
   /* replace63 -> 153 in the cache */

   /* ('u2f32', ('i2i8', 'a@32')) => ('u2f32', ('iand', 'a', 255)) */
   /* search64_0_0 -> 5 in the cache */
   /* search64_0 -> 20 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace64_0_0 -> 5 in the cache */
   /* replace64_0_1 -> 8 in the cache */
   /* replace64_0 -> 9 in the cache */
   /* replace64 -> 155 in the cache */

   /* ('u2f32', ('i2i8', 'a@64')) => ('u2f32', ('iand', 'a', 255)) */
   /* search65_0_0 -> 11 in the cache */
   /* search65_0 -> 22 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace65_0_0 -> 11 in the cache */
   /* replace65_0_1 -> 14 in the cache */
   /* replace65_0 -> 15 in the cache */
   /* replace65 -> 157 in the cache */

   /* ('u2f32', ('f2u8', 'a@16')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search66_0_0 -> 0 in the cache */
   /* search66_0 -> 126 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 126 },
      -1,
   } },

   /* replace66_0_0_0 -> 0 in the cache */
   /* replace66_0_0_1 -> 128 in the cache */
   /* replace66_0_0 -> 129 in the cache */
   /* replace66_0_1 -> 130 in the cache */
   /* replace66_0 -> 131 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_f2f32,
      -1, 2,
      { 131 },
      -1,
   } },

   /* ('u2f32', ('f2u8', 'a@32')) => ('fmin', ('fmax', 'a', 0.0), 255.0) */
   /* search67_0_0 -> 5 in the cache */
   /* search67_0 -> 132 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 132 },
      -1,
   } },

   /* replace67_0_0 -> 5 in the cache */
   /* replace67_0_1 -> 134 in the cache */
   /* replace67_0 -> 135 in the cache */
   /* replace67_1 -> 136 in the cache */
   /* replace67 -> 137 in the cache */

   /* ('u2f32', ('f2u8', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search68_0_0 -> 11 in the cache */
   /* search68_0 -> 139 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 139 },
      -1,
   } },

   /* replace68_0_0_0 -> 11 in the cache */
   /* replace68_0_0_1 -> 141 in the cache */
   /* replace68_0_0 -> 142 in the cache */
   /* replace68_0_1 -> 143 in the cache */
   /* replace68_0 -> 144 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_f2f32,
      -1, 2,
      { 144 },
      -1,
   } },

   /* ('u2f32', ('f2i8', 'a@16')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search69_0_0 -> 0 in the cache */
   /* search69_0 -> 146 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 146 },
      -1,
   } },

   /* replace69_0_0_0 -> 0 in the cache */
   /* replace69_0_0_1 -> 128 in the cache */
   /* replace69_0_0 -> 129 in the cache */
   /* replace69_0_1 -> 130 in the cache */
   /* replace69_0 -> 131 in the cache */
   /* replace69 -> 162 in the cache */

   /* ('u2f32', ('f2i8', 'a@32')) => ('fmin', ('fmax', 'a', 0.0), 255.0) */
   /* search70_0_0 -> 5 in the cache */
   /* search70_0 -> 148 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 148 },
      -1,
   } },

   /* replace70_0_0 -> 5 in the cache */
   /* replace70_0_1 -> 134 in the cache */
   /* replace70_0 -> 135 in the cache */
   /* replace70_1 -> 136 in the cache */
   /* replace70 -> 137 in the cache */

   /* ('u2f32', ('f2i8', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search71_0_0 -> 11 in the cache */
   /* search71_0 -> 150 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 150 },
      -1,
   } },

   /* replace71_0_0_0 -> 11 in the cache */
   /* replace71_0_0_1 -> 141 in the cache */
   /* replace71_0_0 -> 142 in the cache */
   /* replace71_0_1 -> 143 in the cache */
   /* replace71_0 -> 144 in the cache */
   /* replace71 -> 165 in the cache */

   /* ('u2f64', ('u2u8', 'a@16')) => ('u2f64', ('iand', 'a', 255)) */
   /* search72_0_0 -> 0 in the cache */
   /* search72_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace72_0_0 -> 0 in the cache */
   /* replace72_0_1 -> 3 in the cache */
   /* replace72_0 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 1,
      { 4 },
      -1,
   } },

   /* ('u2f64', ('u2u8', 'a@32')) => ('u2f64', ('iand', 'a', 255)) */
   /* search73_0_0 -> 5 in the cache */
   /* search73_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace73_0_0 -> 5 in the cache */
   /* replace73_0_1 -> 8 in the cache */
   /* replace73_0 -> 9 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 1,
      { 9 },
      -1,
   } },

   /* ('u2f64', ('u2u8', 'a@64')) => ('u2f64', ('iand', 'a', 255)) */
   /* search74_0_0 -> 11 in the cache */
   /* search74_0 -> 12 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace74_0_0 -> 11 in the cache */
   /* replace74_0_1 -> 14 in the cache */
   /* replace74_0 -> 15 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 1,
      { 15 },
      -1,
   } },

   /* ('u2f64', ('i2i8', 'a@16')) => ('u2f64', ('iand', 'a', 255)) */
   /* search75_0_0 -> 0 in the cache */
   /* search75_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace75_0_0 -> 0 in the cache */
   /* replace75_0_1 -> 3 in the cache */
   /* replace75_0 -> 4 in the cache */
   /* replace75 -> 170 in the cache */

   /* ('u2f64', ('i2i8', 'a@32')) => ('u2f64', ('iand', 'a', 255)) */
   /* search76_0_0 -> 5 in the cache */
   /* search76_0 -> 20 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace76_0_0 -> 5 in the cache */
   /* replace76_0_1 -> 8 in the cache */
   /* replace76_0 -> 9 in the cache */
   /* replace76 -> 172 in the cache */

   /* ('u2f64', ('i2i8', 'a@64')) => ('u2f64', ('iand', 'a', 255)) */
   /* search77_0_0 -> 11 in the cache */
   /* search77_0 -> 22 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace77_0_0 -> 11 in the cache */
   /* replace77_0_1 -> 14 in the cache */
   /* replace77_0 -> 15 in the cache */
   /* replace77 -> 174 in the cache */

   /* ('u2f64', ('f2u8', 'a@16')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search78_0_0 -> 0 in the cache */
   /* search78_0 -> 126 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 126 },
      -1,
   } },

   /* replace78_0_0_0 -> 0 in the cache */
   /* replace78_0_0_1 -> 128 in the cache */
   /* replace78_0_0 -> 129 in the cache */
   /* replace78_0_1 -> 130 in the cache */
   /* replace78_0 -> 131 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_f2f64,
      -1, 2,
      { 131 },
      -1,
   } },

   /* ('u2f64', ('f2u8', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search79_0_0 -> 5 in the cache */
   /* search79_0 -> 132 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 132 },
      -1,
   } },

   /* replace79_0_0_0 -> 5 in the cache */
   /* replace79_0_0_1 -> 134 in the cache */
   /* replace79_0_0 -> 135 in the cache */
   /* replace79_0_1 -> 136 in the cache */
   /* replace79_0 -> 137 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_f2f64,
      -1, 2,
      { 137 },
      -1,
   } },

   /* ('u2f64', ('f2u8', 'a@64')) => ('fmin', ('fmax', 'a', 0.0), 255.0) */
   /* search80_0_0 -> 11 in the cache */
   /* search80_0 -> 139 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 139 },
      -1,
   } },

   /* replace80_0_0 -> 11 in the cache */
   /* replace80_0_1 -> 141 in the cache */
   /* replace80_0 -> 142 in the cache */
   /* replace80_1 -> 143 in the cache */
   /* replace80 -> 144 in the cache */

   /* ('u2f64', ('f2i8', 'a@16')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search81_0_0 -> 0 in the cache */
   /* search81_0 -> 146 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 146 },
      -1,
   } },

   /* replace81_0_0_0 -> 0 in the cache */
   /* replace81_0_0_1 -> 128 in the cache */
   /* replace81_0_0 -> 129 in the cache */
   /* replace81_0_1 -> 130 in the cache */
   /* replace81_0 -> 131 in the cache */
   /* replace81 -> 179 in the cache */

   /* ('u2f64', ('f2i8', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 255.0)) */
   /* search82_0_0 -> 5 in the cache */
   /* search82_0 -> 148 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 148 },
      -1,
   } },

   /* replace82_0_0_0 -> 5 in the cache */
   /* replace82_0_0_1 -> 134 in the cache */
   /* replace82_0_0 -> 135 in the cache */
   /* replace82_0_1 -> 136 in the cache */
   /* replace82_0 -> 137 in the cache */
   /* replace82 -> 181 in the cache */

   /* ('u2f64', ('f2i8', 'a@64')) => ('fmin', ('fmax', 'a', 0.0), 255.0) */
   /* search83_0_0 -> 11 in the cache */
   /* search83_0 -> 150 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 150 },
      -1,
   } },

   /* replace83_0_0 -> 11 in the cache */
   /* replace83_0_1 -> 141 in the cache */
   /* replace83_0 -> 142 in the cache */
   /* replace83_1 -> 143 in the cache */
   /* replace83 -> 144 in the cache */

   /* ('i2f16', ('u2u8', 'a@16')) => ('i2f16', ('ishr', ('ishl', 'a', 8), 8)) */
   /* search84_0_0 -> 0 in the cache */
   /* search84_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace84_0_0_0 -> 0 in the cache */
   /* replace84_0_0_1 -> 64 in the cache */
   /* replace84_0_0 -> 65 in the cache */
   /* replace84_0_1 -> 64 in the cache */
   /* replace84_0 -> 66 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 66 },
      -1,
   } },

   /* ('i2f16', ('u2u8', 'a@32')) => ('i2f16', ('ishr', ('ishl', 'a', 24), 24)) */
   /* search85_0_0 -> 5 in the cache */
   /* search85_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace85_0_0_0 -> 5 in the cache */
   /* replace85_0_0_1 -> 69 in the cache */
   /* replace85_0_0 -> 70 in the cache */
   /* replace85_0_1 -> 69 in the cache */
   /* replace85_0 -> 71 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 71 },
      -1,
   } },

   /* ('i2f16', ('u2u8', 'a@64')) => ('i2f16', ('ishr', ('ishl', 'a', 56), 56)) */
   /* search86_0_0 -> 11 in the cache */
   /* search86_0 -> 12 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace86_0_0_0 -> 11 in the cache */
   /* replace86_0_0_1 -> 74 in the cache */
   /* replace86_0_0 -> 75 in the cache */
   /* replace86_0_1 -> 74 in the cache */
   /* replace86_0 -> 76 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 76 },
      -1,
   } },

   /* ('i2f16', ('i2i8', 'a@16')) => ('i2f16', ('ishr', ('ishl', 'a', 8), 8)) */
   /* search87_0_0 -> 0 in the cache */
   /* search87_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace87_0_0_0 -> 0 in the cache */
   /* replace87_0_0_1 -> 64 in the cache */
   /* replace87_0_0 -> 65 in the cache */
   /* replace87_0_1 -> 64 in the cache */
   /* replace87_0 -> 66 in the cache */
   /* replace87 -> 187 in the cache */

   /* ('i2f16', ('i2i8', 'a@32')) => ('i2f16', ('ishr', ('ishl', 'a', 24), 24)) */
   /* search88_0_0 -> 5 in the cache */
   /* search88_0 -> 20 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace88_0_0_0 -> 5 in the cache */
   /* replace88_0_0_1 -> 69 in the cache */
   /* replace88_0_0 -> 70 in the cache */
   /* replace88_0_1 -> 69 in the cache */
   /* replace88_0 -> 71 in the cache */
   /* replace88 -> 189 in the cache */

   /* ('i2f16', ('i2i8', 'a@64')) => ('i2f16', ('ishr', ('ishl', 'a', 56), 56)) */
   /* search89_0_0 -> 11 in the cache */
   /* search89_0 -> 22 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace89_0_0_0 -> 11 in the cache */
   /* replace89_0_0_1 -> 74 in the cache */
   /* replace89_0_0 -> 75 in the cache */
   /* replace89_0_1 -> 74 in the cache */
   /* replace89_0 -> 76 in the cache */
   /* replace89 -> 191 in the cache */

   /* ('i2f16', ('f2u8', 'a@16')) => ('fmin', ('fmax', 'a', -128.0), 127.0) */
   /* search90_0_0 -> 0 in the cache */
   /* search90_0 -> 126 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 126 },
      -1,
   } },

   /* replace90_0_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, 16 },
      nir_type_float, { 0xc060000000000000 /* -128.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_fmax,
      1, 1,
      { 0, 196 },
      -1,
   } },
   { .constant = {
      { nir_search_value_constant, 16 },
      nir_type_float, { 0x405fc00000000000 /* 127.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_fmin,
      0, 2,
      { 197, 198 },
      -1,
   } },

   /* ('i2f16', ('f2u8', 'a@32')) => ('f2f16', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search91_0_0 -> 5 in the cache */
   /* search91_0 -> 132 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 132 },
      -1,
   } },

   /* replace91_0_0_0 -> 5 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_float, { 0xc060000000000000 /* -128.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_fmax,
      1, 1,
      { 5, 201 },
      -1,
   } },
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_float, { 0x405fc00000000000 /* 127.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_fmin,
      0, 2,
      { 202, 203 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2f16,
      -1, 2,
      { 204 },
      -1,
   } },

   /* ('i2f16', ('f2u8', 'a@64')) => ('f2f16', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search92_0_0 -> 11 in the cache */
   /* search92_0 -> 139 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 139 },
      -1,
   } },

   /* replace92_0_0_0 -> 11 in the cache */
   { .constant = {
      { nir_search_value_constant, 64 },
      nir_type_float, { 0xc060000000000000 /* -128.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_fmax,
      1, 1,
      { 11, 207 },
      -1,
   } },
   { .constant = {
      { nir_search_value_constant, 64 },
      nir_type_float, { 0x405fc00000000000 /* 127.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_fmin,
      0, 2,
      { 208, 209 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2f16,
      -1, 2,
      { 210 },
      -1,
   } },

   /* ('i2f16', ('f2i8', 'a@16')) => ('fmin', ('fmax', 'a', -128.0), 127.0) */
   /* search93_0_0 -> 0 in the cache */
   /* search93_0 -> 146 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 146 },
      -1,
   } },

   /* replace93_0_0 -> 0 in the cache */
   /* replace93_0_1 -> 196 in the cache */
   /* replace93_0 -> 197 in the cache */
   /* replace93_1 -> 198 in the cache */
   /* replace93 -> 199 in the cache */

   /* ('i2f16', ('f2i8', 'a@32')) => ('f2f16', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search94_0_0 -> 5 in the cache */
   /* search94_0 -> 148 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 148 },
      -1,
   } },

   /* replace94_0_0_0 -> 5 in the cache */
   /* replace94_0_0_1 -> 201 in the cache */
   /* replace94_0_0 -> 202 in the cache */
   /* replace94_0_1 -> 203 in the cache */
   /* replace94_0 -> 204 in the cache */
   /* replace94 -> 205 in the cache */

   /* ('i2f16', ('f2i8', 'a@64')) => ('f2f16', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search95_0_0 -> 11 in the cache */
   /* search95_0 -> 150 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2f16,
      -1, 0,
      { 150 },
      -1,
   } },

   /* replace95_0_0_0 -> 11 in the cache */
   /* replace95_0_0_1 -> 207 in the cache */
   /* replace95_0_0 -> 208 in the cache */
   /* replace95_0_1 -> 209 in the cache */
   /* replace95_0 -> 210 in the cache */
   /* replace95 -> 211 in the cache */

   /* ('i2f32', ('u2u8', 'a@16')) => ('i2f32', ('ishr', ('ishl', 'a', 8), 8)) */
   /* search96_0_0 -> 0 in the cache */
   /* search96_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace96_0_0_0 -> 0 in the cache */
   /* replace96_0_0_1 -> 64 in the cache */
   /* replace96_0_0 -> 65 in the cache */
   /* replace96_0_1 -> 64 in the cache */
   /* replace96_0 -> 66 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 66 },
      -1,
   } },

   /* ('i2f32', ('u2u8', 'a@32')) => ('i2f32', ('ishr', ('ishl', 'a', 24), 24)) */
   /* search97_0_0 -> 5 in the cache */
   /* search97_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace97_0_0_0 -> 5 in the cache */
   /* replace97_0_0_1 -> 69 in the cache */
   /* replace97_0_0 -> 70 in the cache */
   /* replace97_0_1 -> 69 in the cache */
   /* replace97_0 -> 71 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 71 },
      -1,
   } },

   /* ('i2f32', ('u2u8', 'a@64')) => ('i2f32', ('ishr', ('ishl', 'a', 56), 56)) */
   /* search98_0_0 -> 11 in the cache */
   /* search98_0 -> 12 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace98_0_0_0 -> 11 in the cache */
   /* replace98_0_0_1 -> 74 in the cache */
   /* replace98_0_0 -> 75 in the cache */
   /* replace98_0_1 -> 74 in the cache */
   /* replace98_0 -> 76 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 76 },
      -1,
   } },

   /* ('i2f32', ('i2i8', 'a@16')) => ('i2f32', ('ishr', ('ishl', 'a', 8), 8)) */
   /* search99_0_0 -> 0 in the cache */
   /* search99_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace99_0_0_0 -> 0 in the cache */
   /* replace99_0_0_1 -> 64 in the cache */
   /* replace99_0_0 -> 65 in the cache */
   /* replace99_0_1 -> 64 in the cache */
   /* replace99_0 -> 66 in the cache */
   /* replace99 -> 216 in the cache */

   /* ('i2f32', ('i2i8', 'a@32')) => ('i2f32', ('ishr', ('ishl', 'a', 24), 24)) */
   /* search100_0_0 -> 5 in the cache */
   /* search100_0 -> 20 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace100_0_0_0 -> 5 in the cache */
   /* replace100_0_0_1 -> 69 in the cache */
   /* replace100_0_0 -> 70 in the cache */
   /* replace100_0_1 -> 69 in the cache */
   /* replace100_0 -> 71 in the cache */
   /* replace100 -> 218 in the cache */

   /* ('i2f32', ('i2i8', 'a@64')) => ('i2f32', ('ishr', ('ishl', 'a', 56), 56)) */
   /* search101_0_0 -> 11 in the cache */
   /* search101_0 -> 22 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace101_0_0_0 -> 11 in the cache */
   /* replace101_0_0_1 -> 74 in the cache */
   /* replace101_0_0 -> 75 in the cache */
   /* replace101_0_1 -> 74 in the cache */
   /* replace101_0 -> 76 in the cache */
   /* replace101 -> 220 in the cache */

   /* ('i2f32', ('f2u8', 'a@16')) => ('f2f32', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search102_0_0 -> 0 in the cache */
   /* search102_0 -> 126 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 126 },
      -1,
   } },

   /* replace102_0_0_0 -> 0 in the cache */
   /* replace102_0_0_1 -> 196 in the cache */
   /* replace102_0_0 -> 197 in the cache */
   /* replace102_0_1 -> 198 in the cache */
   /* replace102_0 -> 199 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_f2f32,
      -1, 2,
      { 199 },
      -1,
   } },

   /* ('i2f32', ('f2u8', 'a@32')) => ('fmin', ('fmax', 'a', -128.0), 127.0) */
   /* search103_0_0 -> 5 in the cache */
   /* search103_0 -> 132 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 132 },
      -1,
   } },

   /* replace103_0_0 -> 5 in the cache */
   /* replace103_0_1 -> 201 in the cache */
   /* replace103_0 -> 202 in the cache */
   /* replace103_1 -> 203 in the cache */
   /* replace103 -> 204 in the cache */

   /* ('i2f32', ('f2u8', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search104_0_0 -> 11 in the cache */
   /* search104_0 -> 139 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 139 },
      -1,
   } },

   /* replace104_0_0_0 -> 11 in the cache */
   /* replace104_0_0_1 -> 207 in the cache */
   /* replace104_0_0 -> 208 in the cache */
   /* replace104_0_1 -> 209 in the cache */
   /* replace104_0 -> 210 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_f2f32,
      -1, 2,
      { 210 },
      -1,
   } },

   /* ('i2f32', ('f2i8', 'a@16')) => ('f2f32', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search105_0_0 -> 0 in the cache */
   /* search105_0 -> 146 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 146 },
      -1,
   } },

   /* replace105_0_0_0 -> 0 in the cache */
   /* replace105_0_0_1 -> 196 in the cache */
   /* replace105_0_0 -> 197 in the cache */
   /* replace105_0_1 -> 198 in the cache */
   /* replace105_0 -> 199 in the cache */
   /* replace105 -> 225 in the cache */

   /* ('i2f32', ('f2i8', 'a@32')) => ('fmin', ('fmax', 'a', -128.0), 127.0) */
   /* search106_0_0 -> 5 in the cache */
   /* search106_0 -> 148 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 148 },
      -1,
   } },

   /* replace106_0_0 -> 5 in the cache */
   /* replace106_0_1 -> 201 in the cache */
   /* replace106_0 -> 202 in the cache */
   /* replace106_1 -> 203 in the cache */
   /* replace106 -> 204 in the cache */

   /* ('i2f32', ('f2i8', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search107_0_0 -> 11 in the cache */
   /* search107_0 -> 150 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 150 },
      -1,
   } },

   /* replace107_0_0_0 -> 11 in the cache */
   /* replace107_0_0_1 -> 207 in the cache */
   /* replace107_0_0 -> 208 in the cache */
   /* replace107_0_1 -> 209 in the cache */
   /* replace107_0 -> 210 in the cache */
   /* replace107 -> 228 in the cache */

   /* ('i2f64', ('u2u8', 'a@16')) => ('i2f64', ('ishr', ('ishl', 'a', 8), 8)) */
   /* search108_0_0 -> 0 in the cache */
   /* search108_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace108_0_0_0 -> 0 in the cache */
   /* replace108_0_0_1 -> 64 in the cache */
   /* replace108_0_0 -> 65 in the cache */
   /* replace108_0_1 -> 64 in the cache */
   /* replace108_0 -> 66 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 66 },
      -1,
   } },

   /* ('i2f64', ('u2u8', 'a@32')) => ('i2f64', ('ishr', ('ishl', 'a', 24), 24)) */
   /* search109_0_0 -> 5 in the cache */
   /* search109_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace109_0_0_0 -> 5 in the cache */
   /* replace109_0_0_1 -> 69 in the cache */
   /* replace109_0_0 -> 70 in the cache */
   /* replace109_0_1 -> 69 in the cache */
   /* replace109_0 -> 71 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 71 },
      -1,
   } },

   /* ('i2f64', ('u2u8', 'a@64')) => ('i2f64', ('ishr', ('ishl', 'a', 56), 56)) */
   /* search110_0_0 -> 11 in the cache */
   /* search110_0 -> 12 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 12 },
      -1,
   } },

   /* replace110_0_0_0 -> 11 in the cache */
   /* replace110_0_0_1 -> 74 in the cache */
   /* replace110_0_0 -> 75 in the cache */
   /* replace110_0_1 -> 74 in the cache */
   /* replace110_0 -> 76 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 76 },
      -1,
   } },

   /* ('i2f64', ('i2i8', 'a@16')) => ('i2f64', ('ishr', ('ishl', 'a', 8), 8)) */
   /* search111_0_0 -> 0 in the cache */
   /* search111_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace111_0_0_0 -> 0 in the cache */
   /* replace111_0_0_1 -> 64 in the cache */
   /* replace111_0_0 -> 65 in the cache */
   /* replace111_0_1 -> 64 in the cache */
   /* replace111_0 -> 66 in the cache */
   /* replace111 -> 233 in the cache */

   /* ('i2f64', ('i2i8', 'a@32')) => ('i2f64', ('ishr', ('ishl', 'a', 24), 24)) */
   /* search112_0_0 -> 5 in the cache */
   /* search112_0 -> 20 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 20 },
      -1,
   } },

   /* replace112_0_0_0 -> 5 in the cache */
   /* replace112_0_0_1 -> 69 in the cache */
   /* replace112_0_0 -> 70 in the cache */
   /* replace112_0_1 -> 69 in the cache */
   /* replace112_0 -> 71 in the cache */
   /* replace112 -> 235 in the cache */

   /* ('i2f64', ('i2i8', 'a@64')) => ('i2f64', ('ishr', ('ishl', 'a', 56), 56)) */
   /* search113_0_0 -> 11 in the cache */
   /* search113_0 -> 22 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace113_0_0_0 -> 11 in the cache */
   /* replace113_0_0_1 -> 74 in the cache */
   /* replace113_0_0 -> 75 in the cache */
   /* replace113_0_1 -> 74 in the cache */
   /* replace113_0 -> 76 in the cache */
   /* replace113 -> 237 in the cache */

   /* ('i2f64', ('f2u8', 'a@16')) => ('f2f64', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search114_0_0 -> 0 in the cache */
   /* search114_0 -> 126 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 126 },
      -1,
   } },

   /* replace114_0_0_0 -> 0 in the cache */
   /* replace114_0_0_1 -> 196 in the cache */
   /* replace114_0_0 -> 197 in the cache */
   /* replace114_0_1 -> 198 in the cache */
   /* replace114_0 -> 199 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_f2f64,
      -1, 2,
      { 199 },
      -1,
   } },

   /* ('i2f64', ('f2u8', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search115_0_0 -> 5 in the cache */
   /* search115_0 -> 132 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 132 },
      -1,
   } },

   /* replace115_0_0_0 -> 5 in the cache */
   /* replace115_0_0_1 -> 201 in the cache */
   /* replace115_0_0 -> 202 in the cache */
   /* replace115_0_1 -> 203 in the cache */
   /* replace115_0 -> 204 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_f2f64,
      -1, 2,
      { 204 },
      -1,
   } },

   /* ('i2f64', ('f2u8', 'a@64')) => ('fmin', ('fmax', 'a', -128.0), 127.0) */
   /* search116_0_0 -> 11 in the cache */
   /* search116_0 -> 139 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 139 },
      -1,
   } },

   /* replace116_0_0 -> 11 in the cache */
   /* replace116_0_1 -> 207 in the cache */
   /* replace116_0 -> 208 in the cache */
   /* replace116_1 -> 209 in the cache */
   /* replace116 -> 210 in the cache */

   /* ('i2f64', ('f2i8', 'a@16')) => ('f2f64', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search117_0_0 -> 0 in the cache */
   /* search117_0 -> 146 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 146 },
      -1,
   } },

   /* replace117_0_0_0 -> 0 in the cache */
   /* replace117_0_0_1 -> 196 in the cache */
   /* replace117_0_0 -> 197 in the cache */
   /* replace117_0_1 -> 198 in the cache */
   /* replace117_0 -> 199 in the cache */
   /* replace117 -> 242 in the cache */

   /* ('i2f64', ('f2i8', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', -128.0), 127.0)) */
   /* search118_0_0 -> 5 in the cache */
   /* search118_0 -> 148 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 148 },
      -1,
   } },

   /* replace118_0_0_0 -> 5 in the cache */
   /* replace118_0_0_1 -> 201 in the cache */
   /* replace118_0_0 -> 202 in the cache */
   /* replace118_0_1 -> 203 in the cache */
   /* replace118_0 -> 204 in the cache */
   /* replace118 -> 244 in the cache */

   /* ('i2f64', ('f2i8', 'a@64')) => ('fmin', ('fmax', 'a', -128.0), 127.0) */
   /* search119_0_0 -> 11 in the cache */
   /* search119_0 -> 150 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 150 },
      -1,
   } },

   /* replace119_0_0 -> 11 in the cache */
   /* replace119_0_1 -> 207 in the cache */
   /* replace119_0 -> 208 in the cache */
   /* replace119_1 -> 209 in the cache */
   /* replace119 -> 210 in the cache */

};



static const struct transform dxil_nir_lower_8bit_conv_transforms[] = {
   { ~0, ~0, ~0 }, /* Sentinel */

   { 2, 4, 0 },
   { 7, 10, 0 },
   { 13, 16, 0 },
   { 33, 34, 0 },
   { 35, 9, 0 },
   { 36, 37, 0 },
   { 48, 49, 0 },
   { 50, 51, 0 },
   { 52, 15, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 18, 19, 0 },
   { 21, 10, 0 },
   { 23, 16, 0 },
   { 38, 34, 0 },
   { 39, 40, 0 },
   { 41, 37, 0 },
   { 53, 49, 0 },
   { 54, 51, 0 },
   { 55, 56, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 26, 28, 0 },
   { 42, 44, 0 },
   { 57, 59, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 30, 32, 0 },
   { 45, 47, 0 },
   { 60, 62, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 63, 67, 0 },
   { 68, 72, 0 },
   { 73, 77, 0 },
   { 87, 88, 0 },
   { 89, 90, 0 },
   { 91, 92, 0 },
   { 102, 103, 0 },
   { 104, 105, 0 },
   { 106, 107, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 78, 66, 0 },
   { 79, 72, 0 },
   { 80, 77, 0 },
   { 93, 88, 0 },
   { 94, 71, 0 },
   { 95, 92, 0 },
   { 108, 103, 0 },
   { 109, 105, 0 },
   { 110, 76, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 81, 83, 0 },
   { 96, 98, 0 },
   { 111, 113, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 84, 86, 0 },
   { 99, 101, 0 },
   { 114, 116, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 117, 118, 0 },
   { 119, 120, 0 },
   { 121, 122, 0 },
   { 152, 153, 0 },
   { 154, 155, 0 },
   { 156, 157, 0 },
   { 169, 170, 0 },
   { 171, 172, 0 },
   { 173, 174, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 123, 118, 0 },
   { 124, 120, 0 },
   { 125, 122, 0 },
   { 158, 153, 0 },
   { 159, 155, 0 },
   { 160, 157, 0 },
   { 175, 170, 0 },
   { 176, 172, 0 },
   { 177, 174, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 127, 131, 0 },
   { 133, 138, 0 },
   { 140, 145, 0 },
   { 161, 162, 0 },
   { 163, 137, 0 },
   { 164, 165, 0 },
   { 178, 179, 0 },
   { 180, 181, 0 },
   { 182, 144, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 147, 131, 0 },
   { 149, 138, 0 },
   { 151, 145, 0 },
   { 166, 162, 0 },
   { 167, 137, 0 },
   { 168, 165, 0 },
   { 183, 179, 0 },
   { 184, 181, 0 },
   { 185, 144, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 186, 187, 0 },
   { 188, 189, 0 },
   { 190, 191, 0 },
   { 215, 216, 0 },
   { 217, 218, 0 },
   { 219, 220, 0 },
   { 232, 233, 0 },
   { 234, 235, 0 },
   { 236, 237, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 192, 187, 0 },
   { 193, 189, 0 },
   { 194, 191, 0 },
   { 221, 216, 0 },
   { 222, 218, 0 },
   { 223, 220, 0 },
   { 238, 233, 0 },
   { 239, 235, 0 },
   { 240, 237, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 195, 199, 0 },
   { 200, 205, 0 },
   { 206, 211, 0 },
   { 224, 225, 0 },
   { 226, 204, 0 },
   { 227, 228, 0 },
   { 241, 242, 0 },
   { 243, 244, 0 },
   { 245, 210, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 212, 199, 0 },
   { 213, 205, 0 },
   { 214, 211, 0 },
   { 229, 225, 0 },
   { 230, 204, 0 },
   { 231, 228, 0 },
   { 246, 242, 0 },
   { 247, 244, 0 },
   { 248, 210, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

};

static const struct per_op_table dxil_nir_lower_8bit_conv_pass_op_table[nir_num_search_ops] = {
   [nir_search_op_u2u] = {
      .filter = (const uint16_t []) {
         0,
         0,
         1,
         2,
         3,
         4,
         1,
         1,
         1,
         1,
         2,
         2,
         2,
         2,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 5,
      .table = (const uint16_t []) {
      
         2,
         6,
         7,
         8,
         9,
      },
   },
   [nir_search_op_i2i] = {
      .filter = (const uint16_t []) {
         0,
         0,
         1,
         2,
         3,
         4,
         1,
         1,
         1,
         1,
         2,
         2,
         2,
         2,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 5,
      .table = (const uint16_t []) {
      
         3,
         10,
         11,
         12,
         13,
      },
   },
   [nir_search_op_f2u] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         4,
      },
   },
   [nir_search_op_f2i] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         5,
      },
   },
   [nir_search_op_u2f] = {
      .filter = (const uint16_t []) {
         0,
         0,
         1,
         2,
         3,
         4,
         1,
         1,
         1,
         1,
         2,
         2,
         2,
         2,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 5,
      .table = (const uint16_t []) {
      
         0,
         14,
         15,
         16,
         17,
      },
   },
   [nir_search_op_i2f] = {
      .filter = (const uint16_t []) {
         0,
         0,
         1,
         2,
         3,
         4,
         1,
         1,
         1,
         1,
         2,
         2,
         2,
         2,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 5,
      .table = (const uint16_t []) {
      
         0,
         18,
         19,
         20,
         21,
      },
   },
};

/* Mapping from state index to offset in transforms (0 being no transforms) */
static const uint16_t dxil_nir_lower_8bit_conv_transform_offsets[] = {
   0,
   0,
   0,
   0,
   0,
   0,
   1,
   11,
   21,
   25,
   29,
   39,
   49,
   53,
   57,
   67,
   77,
   87,
   97,
   107,
   117,
   127,
};

static const nir_algebraic_table dxil_nir_lower_8bit_conv_table = {
   .transforms = dxil_nir_lower_8bit_conv_transforms,
   .transform_offsets = dxil_nir_lower_8bit_conv_transform_offsets,
   .pass_op_table = dxil_nir_lower_8bit_conv_pass_op_table,
   .values = dxil_nir_lower_8bit_conv_values,
   .expression_cond = NULL,
   .variable_cond = NULL,
};

bool
dxil_nir_lower_8bit_conv(nir_shader *shader)
{
   bool progress = false;
   bool condition_flags[1];
   const nir_shader_compiler_options *options = shader->options;
   const shader_info *info = &shader->info;
   (void) options;
   (void) info;

   STATIC_ASSERT(249 == ARRAY_SIZE(dxil_nir_lower_8bit_conv_values));
   condition_flags[0] = true;

   nir_foreach_function_impl(impl, shader) {
     progress |= nir_algebraic_impl(impl, condition_flags, &dxil_nir_lower_8bit_conv_table);
   }

   return progress;
}


#include "nir.h"
#include "nir_builder.h"
#include "nir_search.h"
#include "nir_search_helpers.h"

/* What follows is NIR algebraic transform code for the following 58
 * transforms:
 *    ('u2u32', ('u2u16', 'a@32')) => ('iand', 'a', 65535)
 *    ('u2u32', ('u2u16', 'a@64')) => ('u2u32', ('iand', 'a', 65535))
 *    ('u2u32', ('i2i16', 'a@32')) => ('u2u32', ('iand', 'a', 65535))
 *    ('u2u32', ('i2i16', 'a@64')) => ('u2u32', ('iand', 'a', 65535))
 *    ('u2u32', ('f2u16', 'a')) => ('iand', ('f2u32', 'a'), 65535)
 *    ('u2u32', ('f2i16', 'a')) => ('iand', ('f2i32', 'a'), 65535)
 *    ('u2u64', ('u2u16', 'a@32')) => ('u2u64', ('iand', 'a', 65535))
 *    ('u2u64', ('u2u16', 'a@64')) => ('iand', 'a', 65535)
 *    ('u2u64', ('i2i16', 'a@32')) => ('u2u64', ('iand', 'a', 65535))
 *    ('u2u64', ('i2i16', 'a@64')) => ('u2u64', ('iand', 'a', 65535))
 *    ('u2u64', ('f2u16', 'a')) => ('iand', ('f2u64', 'a'), 65535)
 *    ('u2u64', ('f2i16', 'a')) => ('iand', ('f2i64', 'a'), 65535)
 *    ('i2i32', ('u2u16', 'a@32')) => ('i2i32', ('ishr', ('ishl', 'a', 16), 16))
 *    ('i2i32', ('u2u16', 'a@64')) => ('i2i32', ('ishr', ('ishl', 'a', 48), 48))
 *    ('i2i32', ('i2i16', 'a@32')) => ('ishr', ('ishl', 'a', 16), 16)
 *    ('i2i32', ('i2i16', 'a@64')) => ('i2i32', ('ishr', ('ishl', 'a', 48), 48))
 *    ('i2i32', ('f2u16', 'a')) => ('ishr', ('ishl', ('f2u32', 'a'), 16), 16)
 *    ('i2i32', ('f2i16', 'a')) => ('ishr', ('ishl', ('f2i32', 'a'), 16), 16)
 *    ('i2i64', ('u2u16', 'a@32')) => ('i2i64', ('ishr', ('ishl', 'a', 16), 16))
 *    ('i2i64', ('u2u16', 'a@64')) => ('i2i64', ('ishr', ('ishl', 'a', 48), 48))
 *    ('i2i64', ('i2i16', 'a@32')) => ('i2i64', ('ishr', ('ishl', 'a', 16), 16))
 *    ('i2i64', ('i2i16', 'a@64')) => ('ishr', ('ishl', 'a', 48), 48)
 *    ('i2i64', ('f2u16', 'a')) => ('ishr', ('ishl', ('f2u64', 'a'), 48), 48)
 *    ('i2i64', ('f2i16', 'a')) => ('ishr', ('ishl', ('f2i64', 'a'), 48), 48)
 *    ('u2f32', ('u2u16', 'a@32')) => ('u2f32', ('iand', 'a', 65535))
 *    ('u2f32', ('u2u16', 'a@64')) => ('u2f32', ('iand', 'a', 65535))
 *    ('u2f32', ('i2i16', 'a@32')) => ('u2f32', ('iand', 'a', 65535))
 *    ('u2f32', ('i2i16', 'a@64')) => ('u2f32', ('iand', 'a', 65535))
 *    ('u2f32', ('f2u16', 'a@32')) => ('fmin', ('fmax', 'a', 0.0), 65535.0)
 *    ('u2f32', ('f2u16', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 65535.0))
 *    ('u2f32', ('f2i16', 'a@32')) => ('fmin', ('fmax', 'a', 0.0), 65535.0)
 *    ('u2f32', ('f2i16', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 65535.0))
 *    ('u2f64', ('u2u16', 'a@32')) => ('u2f64', ('iand', 'a', 65535))
 *    ('u2f64', ('u2u16', 'a@64')) => ('u2f64', ('iand', 'a', 65535))
 *    ('u2f64', ('i2i16', 'a@32')) => ('u2f64', ('iand', 'a', 65535))
 *    ('u2f64', ('i2i16', 'a@64')) => ('u2f64', ('iand', 'a', 65535))
 *    ('u2f64', ('f2u16', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 65535.0))
 *    ('u2f64', ('f2u16', 'a@64')) => ('fmin', ('fmax', 'a', 0.0), 65535.0)
 *    ('u2f64', ('f2i16', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 65535.0))
 *    ('u2f64', ('f2i16', 'a@64')) => ('fmin', ('fmax', 'a', 0.0), 65535.0)
 *    ('i2f32', ('u2u16', 'a@32')) => ('i2f32', ('ishr', ('ishl', 'a', 16), 16))
 *    ('i2f32', ('u2u16', 'a@64')) => ('i2f32', ('ishr', ('ishl', 'a', 48), 48))
 *    ('i2f32', ('i2i16', 'a@32')) => ('i2f32', ('ishr', ('ishl', 'a', 16), 16))
 *    ('i2f32', ('i2i16', 'a@64')) => ('i2f32', ('ishr', ('ishl', 'a', 48), 48))
 *    ('i2f32', ('f2u16', 'a@32')) => ('fmin', ('fmax', 'a', -32768.0), 32767.0)
 *    ('i2f32', ('f2u16', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', -32768.0), 32767.0))
 *    ('i2f32', ('f2i16', 'a@32')) => ('fmin', ('fmax', 'a', -32768.0), 32767.0)
 *    ('i2f32', ('f2i16', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', -32768.0), 32767.0))
 *    ('i2f64', ('u2u16', 'a@32')) => ('i2f64', ('ishr', ('ishl', 'a', 16), 16))
 *    ('i2f64', ('u2u16', 'a@64')) => ('i2f64', ('ishr', ('ishl', 'a', 48), 48))
 *    ('i2f64', ('i2i16', 'a@32')) => ('i2f64', ('ishr', ('ishl', 'a', 16), 16))
 *    ('i2f64', ('i2i16', 'a@64')) => ('i2f64', ('ishr', ('ishl', 'a', 48), 48))
 *    ('i2f64', ('f2u16', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', -32768.0), 32767.0))
 *    ('i2f64', ('f2u16', 'a@64')) => ('fmin', ('fmax', 'a', -32768.0), 32767.0)
 *    ('i2f64', ('f2i16', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', -32768.0), 32767.0))
 *    ('i2f64', ('f2i16', 'a@64')) => ('fmin', ('fmax', 'a', -32768.0), 32767.0)
 *    ('f2f32', ('u2u16', 'a@32')) => ('unpack_half_2x16_split_x', 'a')
 *    ('u2u32', ('f2f16_rtz', 'a@32')) => ('pack_half_2x16_split', 'a', 0)
 */


static const nir_search_value_union dxil_nir_lower_16bit_conv_values[] = {
   /* ('u2u32', ('u2u16', 'a@32')) => ('iand', 'a', 65535) */
   { .variable = {
      { nir_search_value_variable, 32 },
      0, /* a */
      false,
      nir_type_invalid,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace120_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0xffff /* 65535 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 0, 3 },
      -1,
   } },

   /* ('u2u32', ('u2u16', 'a@64')) => ('u2u32', ('iand', 'a', 65535)) */
   { .variable = {
      { nir_search_value_variable, 64 },
      0, /* a */
      false,
      nir_type_invalid,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_u2u16,
      -1, 0,
      { 5 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace121_0_0 -> 5 in the cache */
   { .constant = {
      { nir_search_value_constant, 64 },
      nir_type_int, { 0xffff /* 65535 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 5, 8 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 1,
      { 9 },
      -1,
   } },

   /* ('u2u32', ('i2i16', 'a@32')) => ('u2u32', ('iand', 'a', 65535)) */
   /* search122_0_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 11 },
      -1,
   } },

   /* replace122_0_0 -> 0 in the cache */
   /* replace122_0_1 -> 3 in the cache */
   /* replace122_0 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 1,
      { 4 },
      -1,
   } },

   /* ('u2u32', ('i2i16', 'a@64')) => ('u2u32', ('iand', 'a', 65535)) */
   /* search123_0_0 -> 5 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_i2i16,
      -1, 0,
      { 5 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 14 },
      -1,
   } },

   /* replace123_0_0 -> 5 in the cache */
   /* replace123_0_1 -> 8 in the cache */
   /* replace123_0 -> 9 in the cache */
   /* replace123 -> 10 in the cache */

   /* ('u2u32', ('f2u16', 'a')) => ('iand', ('f2u32', 'a'), 65535) */
   { .variable = {
      { nir_search_value_variable, -1 },
      0, /* a */
      false,
      nir_type_invalid,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2u16,
      -1, 0,
      { 16 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace124_0_0 -> 16 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_f2u32,
      -1, 0,
      { 16 },
      -1,
   } },
   /* replace124_1 -> 3 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 19, 3 },
      -1,
   } },

   /* ('u2u32', ('f2i16', 'a')) => ('iand', ('f2i32', 'a'), 65535) */
   /* search125_0_0 -> 16 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2i16,
      -1, 0,
      { 16 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 21 },
      -1,
   } },

   /* replace125_0_0 -> 16 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_f2i32,
      -1, 0,
      { 16 },
      -1,
   } },
   /* replace125_1 -> 3 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 23, 3 },
      -1,
   } },

   /* ('u2u64', ('u2u16', 'a@32')) => ('u2u64', ('iand', 'a', 65535)) */
   /* search126_0_0 -> 0 in the cache */
   /* search126_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace126_0_0 -> 0 in the cache */
   /* replace126_0_1 -> 3 in the cache */
   /* replace126_0 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 1,
      { 4 },
      -1,
   } },

   /* ('u2u64', ('u2u16', 'a@64')) => ('iand', 'a', 65535) */
   /* search127_0_0 -> 5 in the cache */
   /* search127_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace127_0 -> 5 in the cache */
   /* replace127_1 -> 8 in the cache */
   /* replace127 -> 9 in the cache */

   /* ('u2u64', ('i2i16', 'a@32')) => ('u2u64', ('iand', 'a', 65535)) */
   /* search128_0_0 -> 0 in the cache */
   /* search128_0 -> 11 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 11 },
      -1,
   } },

   /* replace128_0_0 -> 0 in the cache */
   /* replace128_0_1 -> 3 in the cache */
   /* replace128_0 -> 4 in the cache */
   /* replace128 -> 26 in the cache */

   /* ('u2u64', ('i2i16', 'a@64')) => ('u2u64', ('iand', 'a', 65535)) */
   /* search129_0_0 -> 5 in the cache */
   /* search129_0 -> 14 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 14 },
      -1,
   } },

   /* replace129_0_0 -> 5 in the cache */
   /* replace129_0_1 -> 8 in the cache */
   /* replace129_0 -> 9 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 1,
      { 9 },
      -1,
   } },

   /* ('u2u64', ('f2u16', 'a')) => ('iand', ('f2u64', 'a'), 65535) */
   /* search130_0_0 -> 16 in the cache */
   /* search130_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace130_0_0 -> 16 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_f2u64,
      -1, 0,
      { 16 },
      -1,
   } },
   /* replace130_1 -> 8 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 32, 8 },
      -1,
   } },

   /* ('u2u64', ('f2i16', 'a')) => ('iand', ('f2i64', 'a'), 65535) */
   /* search131_0_0 -> 16 in the cache */
   /* search131_0 -> 21 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2u64,
      -1, 0,
      { 21 },
      -1,
   } },

   /* replace131_0_0 -> 16 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_f2i64,
      -1, 0,
      { 16 },
      -1,
   } },
   /* replace131_1 -> 8 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_iand,
      0, 1,
      { 35, 8 },
      -1,
   } },

   /* ('i2i32', ('u2u16', 'a@32')) => ('i2i32', ('ishr', ('ishl', 'a', 16), 16)) */
   /* search132_0_0 -> 0 in the cache */
   /* search132_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace132_0_0_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0x10 /* 16 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 0, 38 },
      -1,
   } },
   /* replace132_0_1 -> 38 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 39, 38 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 40 },
      -1,
   } },

   /* ('i2i32', ('u2u16', 'a@64')) => ('i2i32', ('ishr', ('ishl', 'a', 48), 48)) */
   /* search133_0_0 -> 5 in the cache */
   /* search133_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace133_0_0_0 -> 5 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0x30 /* 48 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 5, 43 },
      -1,
   } },
   /* replace133_0_1 -> 43 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 44, 43 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 45 },
      -1,
   } },

   /* ('i2i32', ('i2i16', 'a@32')) => ('ishr', ('ishl', 'a', 16), 16) */
   /* search134_0_0 -> 0 in the cache */
   /* search134_0 -> 11 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 11 },
      -1,
   } },

   /* replace134_0_0 -> 0 in the cache */
   /* replace134_0_1 -> 38 in the cache */
   /* replace134_0 -> 39 in the cache */
   /* replace134_1 -> 38 in the cache */
   /* replace134 -> 40 in the cache */

   /* ('i2i32', ('i2i16', 'a@64')) => ('i2i32', ('ishr', ('ishl', 'a', 48), 48)) */
   /* search135_0_0 -> 5 in the cache */
   /* search135_0 -> 14 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 14 },
      -1,
   } },

   /* replace135_0_0_0 -> 5 in the cache */
   /* replace135_0_0_1 -> 43 in the cache */
   /* replace135_0_0 -> 44 in the cache */
   /* replace135_0_1 -> 43 in the cache */
   /* replace135_0 -> 45 in the cache */
   /* replace135 -> 46 in the cache */

   /* ('i2i32', ('f2u16', 'a')) => ('ishr', ('ishl', ('f2u32', 'a'), 16), 16) */
   /* search136_0_0 -> 16 in the cache */
   /* search136_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace136_0_0_0 -> 16 in the cache */
   /* replace136_0_0 -> 19 in the cache */
   /* replace136_0_1 -> 38 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 19, 38 },
      -1,
   } },
   /* replace136_1 -> 38 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 50, 38 },
      -1,
   } },

   /* ('i2i32', ('f2i16', 'a')) => ('ishr', ('ishl', ('f2i32', 'a'), 16), 16) */
   /* search137_0_0 -> 16 in the cache */
   /* search137_0 -> 21 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2i32,
      -1, 0,
      { 21 },
      -1,
   } },

   /* replace137_0_0_0 -> 16 in the cache */
   /* replace137_0_0 -> 23 in the cache */
   /* replace137_0_1 -> 38 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 23, 38 },
      -1,
   } },
   /* replace137_1 -> 38 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 53, 38 },
      -1,
   } },

   /* ('i2i64', ('u2u16', 'a@32')) => ('i2i64', ('ishr', ('ishl', 'a', 16), 16)) */
   /* search138_0_0 -> 0 in the cache */
   /* search138_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace138_0_0_0 -> 0 in the cache */
   /* replace138_0_0_1 -> 38 in the cache */
   /* replace138_0_0 -> 39 in the cache */
   /* replace138_0_1 -> 38 in the cache */
   /* replace138_0 -> 40 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 40 },
      -1,
   } },

   /* ('i2i64', ('u2u16', 'a@64')) => ('i2i64', ('ishr', ('ishl', 'a', 48), 48)) */
   /* search139_0_0 -> 5 in the cache */
   /* search139_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace139_0_0_0 -> 5 in the cache */
   /* replace139_0_0_1 -> 43 in the cache */
   /* replace139_0_0 -> 44 in the cache */
   /* replace139_0_1 -> 43 in the cache */
   /* replace139_0 -> 45 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 45 },
      -1,
   } },

   /* ('i2i64', ('i2i16', 'a@32')) => ('i2i64', ('ishr', ('ishl', 'a', 16), 16)) */
   /* search140_0_0 -> 0 in the cache */
   /* search140_0 -> 11 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 11 },
      -1,
   } },

   /* replace140_0_0_0 -> 0 in the cache */
   /* replace140_0_0_1 -> 38 in the cache */
   /* replace140_0_0 -> 39 in the cache */
   /* replace140_0_1 -> 38 in the cache */
   /* replace140_0 -> 40 in the cache */
   /* replace140 -> 56 in the cache */

   /* ('i2i64', ('i2i16', 'a@64')) => ('ishr', ('ishl', 'a', 48), 48) */
   /* search141_0_0 -> 5 in the cache */
   /* search141_0 -> 14 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 14 },
      -1,
   } },

   /* replace141_0_0 -> 5 in the cache */
   /* replace141_0_1 -> 43 in the cache */
   /* replace141_0 -> 44 in the cache */
   /* replace141_1 -> 43 in the cache */
   /* replace141 -> 45 in the cache */

   /* ('i2i64', ('f2u16', 'a')) => ('ishr', ('ishl', ('f2u64', 'a'), 48), 48) */
   /* search142_0_0 -> 16 in the cache */
   /* search142_0 -> 17 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 17 },
      -1,
   } },

   /* replace142_0_0_0 -> 16 in the cache */
   /* replace142_0_0 -> 32 in the cache */
   /* replace142_0_1 -> 43 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 32, 43 },
      -1,
   } },
   /* replace142_1 -> 43 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 62, 43 },
      -1,
   } },

   /* ('i2i64', ('f2i16', 'a')) => ('ishr', ('ishl', ('f2i64', 'a'), 48), 48) */
   /* search143_0_0 -> 16 in the cache */
   /* search143_0 -> 21 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2i64,
      -1, 0,
      { 21 },
      -1,
   } },

   /* replace143_0_0_0 -> 16 in the cache */
   /* replace143_0_0 -> 35 in the cache */
   /* replace143_0_1 -> 43 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishl,
      -1, 0,
      { 35, 43 },
      -1,
   } },
   /* replace143_1 -> 43 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_ishr,
      -1, 0,
      { 65, 43 },
      -1,
   } },

   /* ('u2f32', ('u2u16', 'a@32')) => ('u2f32', ('iand', 'a', 65535)) */
   /* search144_0_0 -> 0 in the cache */
   /* search144_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace144_0_0 -> 0 in the cache */
   /* replace144_0_1 -> 3 in the cache */
   /* replace144_0 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 1,
      { 4 },
      -1,
   } },

   /* ('u2f32', ('u2u16', 'a@64')) => ('u2f32', ('iand', 'a', 65535)) */
   /* search145_0_0 -> 5 in the cache */
   /* search145_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace145_0_0 -> 5 in the cache */
   /* replace145_0_1 -> 8 in the cache */
   /* replace145_0 -> 9 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 1,
      { 9 },
      -1,
   } },

   /* ('u2f32', ('i2i16', 'a@32')) => ('u2f32', ('iand', 'a', 65535)) */
   /* search146_0_0 -> 0 in the cache */
   /* search146_0 -> 11 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 11 },
      -1,
   } },

   /* replace146_0_0 -> 0 in the cache */
   /* replace146_0_1 -> 3 in the cache */
   /* replace146_0 -> 4 in the cache */
   /* replace146 -> 68 in the cache */

   /* ('u2f32', ('i2i16', 'a@64')) => ('u2f32', ('iand', 'a', 65535)) */
   /* search147_0_0 -> 5 in the cache */
   /* search147_0 -> 14 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 14 },
      -1,
   } },

   /* replace147_0_0 -> 5 in the cache */
   /* replace147_0_1 -> 8 in the cache */
   /* replace147_0 -> 9 in the cache */
   /* replace147 -> 70 in the cache */

   /* ('u2f32', ('f2u16', 'a@32')) => ('fmin', ('fmax', 'a', 0.0), 65535.0) */
   /* search148_0_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2u16,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 73 },
      -1,
   } },

   /* replace148_0_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_float, { 0x0 /* 0.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_fmax,
      1, 1,
      { 0, 75 },
      -1,
   } },
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_float, { 0x40efffe000000000 /* 65535.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_fmin,
      0, 2,
      { 76, 77 },
      -1,
   } },

   /* ('u2f32', ('f2u16', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 65535.0)) */
   /* search149_0_0 -> 5 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2u16,
      -1, 0,
      { 5 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 79 },
      -1,
   } },

   /* replace149_0_0_0 -> 5 in the cache */
   { .constant = {
      { nir_search_value_constant, 64 },
      nir_type_float, { 0x0 /* 0.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_fmax,
      1, 1,
      { 5, 81 },
      -1,
   } },
   { .constant = {
      { nir_search_value_constant, 64 },
      nir_type_float, { 0x40efffe000000000 /* 65535.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_fmin,
      0, 2,
      { 82, 83 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_f2f32,
      -1, 2,
      { 84 },
      -1,
   } },

   /* ('u2f32', ('f2i16', 'a@32')) => ('fmin', ('fmax', 'a', 0.0), 65535.0) */
   /* search150_0_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2i16,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 86 },
      -1,
   } },

   /* replace150_0_0 -> 0 in the cache */
   /* replace150_0_1 -> 75 in the cache */
   /* replace150_0 -> 76 in the cache */
   /* replace150_1 -> 77 in the cache */
   /* replace150 -> 78 in the cache */

   /* ('u2f32', ('f2i16', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', 0.0), 65535.0)) */
   /* search151_0_0 -> 5 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2i16,
      -1, 0,
      { 5 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2f32,
      -1, 0,
      { 88 },
      -1,
   } },

   /* replace151_0_0_0 -> 5 in the cache */
   /* replace151_0_0_1 -> 81 in the cache */
   /* replace151_0_0 -> 82 in the cache */
   /* replace151_0_1 -> 83 in the cache */
   /* replace151_0 -> 84 in the cache */
   /* replace151 -> 85 in the cache */

   /* ('u2f64', ('u2u16', 'a@32')) => ('u2f64', ('iand', 'a', 65535)) */
   /* search152_0_0 -> 0 in the cache */
   /* search152_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace152_0_0 -> 0 in the cache */
   /* replace152_0_1 -> 3 in the cache */
   /* replace152_0 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 1,
      { 4 },
      -1,
   } },

   /* ('u2f64', ('u2u16', 'a@64')) => ('u2f64', ('iand', 'a', 65535)) */
   /* search153_0_0 -> 5 in the cache */
   /* search153_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace153_0_0 -> 5 in the cache */
   /* replace153_0_1 -> 8 in the cache */
   /* replace153_0 -> 9 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 1,
      { 9 },
      -1,
   } },

   /* ('u2f64', ('i2i16', 'a@32')) => ('u2f64', ('iand', 'a', 65535)) */
   /* search154_0_0 -> 0 in the cache */
   /* search154_0 -> 11 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 11 },
      -1,
   } },

   /* replace154_0_0 -> 0 in the cache */
   /* replace154_0_1 -> 3 in the cache */
   /* replace154_0 -> 4 in the cache */
   /* replace154 -> 91 in the cache */

   /* ('u2f64', ('i2i16', 'a@64')) => ('u2f64', ('iand', 'a', 65535)) */
   /* search155_0_0 -> 5 in the cache */
   /* search155_0 -> 14 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 14 },
      -1,
   } },

   /* replace155_0_0 -> 5 in the cache */
   /* replace155_0_1 -> 8 in the cache */
   /* replace155_0 -> 9 in the cache */
   /* replace155 -> 93 in the cache */

   /* ('u2f64', ('f2u16', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 65535.0)) */
   /* search156_0_0 -> 0 in the cache */
   /* search156_0 -> 73 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 73 },
      -1,
   } },

   /* replace156_0_0_0 -> 0 in the cache */
   /* replace156_0_0_1 -> 75 in the cache */
   /* replace156_0_0 -> 76 in the cache */
   /* replace156_0_1 -> 77 in the cache */
   /* replace156_0 -> 78 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_f2f64,
      -1, 2,
      { 78 },
      -1,
   } },

   /* ('u2f64', ('f2u16', 'a@64')) => ('fmin', ('fmax', 'a', 0.0), 65535.0) */
   /* search157_0_0 -> 5 in the cache */
   /* search157_0 -> 79 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 79 },
      -1,
   } },

   /* replace157_0_0 -> 5 in the cache */
   /* replace157_0_1 -> 81 in the cache */
   /* replace157_0 -> 82 in the cache */
   /* replace157_1 -> 83 in the cache */
   /* replace157 -> 84 in the cache */

   /* ('u2f64', ('f2i16', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', 0.0), 65535.0)) */
   /* search158_0_0 -> 0 in the cache */
   /* search158_0 -> 86 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 86 },
      -1,
   } },

   /* replace158_0_0_0 -> 0 in the cache */
   /* replace158_0_0_1 -> 75 in the cache */
   /* replace158_0_0 -> 76 in the cache */
   /* replace158_0_1 -> 77 in the cache */
   /* replace158_0 -> 78 in the cache */
   /* replace158 -> 97 in the cache */

   /* ('u2f64', ('f2i16', 'a@64')) => ('fmin', ('fmax', 'a', 0.0), 65535.0) */
   /* search159_0_0 -> 5 in the cache */
   /* search159_0 -> 88 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_u2f64,
      -1, 0,
      { 88 },
      -1,
   } },

   /* replace159_0_0 -> 5 in the cache */
   /* replace159_0_1 -> 81 in the cache */
   /* replace159_0 -> 82 in the cache */
   /* replace159_1 -> 83 in the cache */
   /* replace159 -> 84 in the cache */

   /* ('i2f32', ('u2u16', 'a@32')) => ('i2f32', ('ishr', ('ishl', 'a', 16), 16)) */
   /* search160_0_0 -> 0 in the cache */
   /* search160_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace160_0_0_0 -> 0 in the cache */
   /* replace160_0_0_1 -> 38 in the cache */
   /* replace160_0_0 -> 39 in the cache */
   /* replace160_0_1 -> 38 in the cache */
   /* replace160_0 -> 40 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 40 },
      -1,
   } },

   /* ('i2f32', ('u2u16', 'a@64')) => ('i2f32', ('ishr', ('ishl', 'a', 48), 48)) */
   /* search161_0_0 -> 5 in the cache */
   /* search161_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace161_0_0_0 -> 5 in the cache */
   /* replace161_0_0_1 -> 43 in the cache */
   /* replace161_0_0 -> 44 in the cache */
   /* replace161_0_1 -> 43 in the cache */
   /* replace161_0 -> 45 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 45 },
      -1,
   } },

   /* ('i2f32', ('i2i16', 'a@32')) => ('i2f32', ('ishr', ('ishl', 'a', 16), 16)) */
   /* search162_0_0 -> 0 in the cache */
   /* search162_0 -> 11 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 11 },
      -1,
   } },

   /* replace162_0_0_0 -> 0 in the cache */
   /* replace162_0_0_1 -> 38 in the cache */
   /* replace162_0_0 -> 39 in the cache */
   /* replace162_0_1 -> 38 in the cache */
   /* replace162_0 -> 40 in the cache */
   /* replace162 -> 102 in the cache */

   /* ('i2f32', ('i2i16', 'a@64')) => ('i2f32', ('ishr', ('ishl', 'a', 48), 48)) */
   /* search163_0_0 -> 5 in the cache */
   /* search163_0 -> 14 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 14 },
      -1,
   } },

   /* replace163_0_0_0 -> 5 in the cache */
   /* replace163_0_0_1 -> 43 in the cache */
   /* replace163_0_0 -> 44 in the cache */
   /* replace163_0_1 -> 43 in the cache */
   /* replace163_0 -> 45 in the cache */
   /* replace163 -> 104 in the cache */

   /* ('i2f32', ('f2u16', 'a@32')) => ('fmin', ('fmax', 'a', -32768.0), 32767.0) */
   /* search164_0_0 -> 0 in the cache */
   /* search164_0 -> 73 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 73 },
      -1,
   } },

   /* replace164_0_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_float, { 0xc0e0000000000000 /* -32768.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_fmax,
      1, 1,
      { 0, 108 },
      -1,
   } },
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_float, { 0x40dfffc000000000 /* 32767.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_fmin,
      0, 2,
      { 109, 110 },
      -1,
   } },

   /* ('i2f32', ('f2u16', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', -32768.0), 32767.0)) */
   /* search165_0_0 -> 5 in the cache */
   /* search165_0 -> 79 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 79 },
      -1,
   } },

   /* replace165_0_0_0 -> 5 in the cache */
   { .constant = {
      { nir_search_value_constant, 64 },
      nir_type_float, { 0xc0e0000000000000 /* -32768.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_fmax,
      1, 1,
      { 5, 113 },
      -1,
   } },
   { .constant = {
      { nir_search_value_constant, 64 },
      nir_type_float, { 0x40dfffc000000000 /* 32767.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_fmin,
      0, 2,
      { 114, 115 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_f2f32,
      -1, 2,
      { 116 },
      -1,
   } },

   /* ('i2f32', ('f2i16', 'a@32')) => ('fmin', ('fmax', 'a', -32768.0), 32767.0) */
   /* search166_0_0 -> 0 in the cache */
   /* search166_0 -> 86 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 86 },
      -1,
   } },

   /* replace166_0_0 -> 0 in the cache */
   /* replace166_0_1 -> 108 in the cache */
   /* replace166_0 -> 109 in the cache */
   /* replace166_1 -> 110 in the cache */
   /* replace166 -> 111 in the cache */

   /* ('i2f32', ('f2i16', 'a@64')) => ('f2f32', ('fmin', ('fmax', 'a', -32768.0), 32767.0)) */
   /* search167_0_0 -> 5 in the cache */
   /* search167_0 -> 88 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_i2f32,
      -1, 0,
      { 88 },
      -1,
   } },

   /* replace167_0_0_0 -> 5 in the cache */
   /* replace167_0_0_1 -> 113 in the cache */
   /* replace167_0_0 -> 114 in the cache */
   /* replace167_0_1 -> 115 in the cache */
   /* replace167_0 -> 116 in the cache */
   /* replace167 -> 117 in the cache */

   /* ('i2f64', ('u2u16', 'a@32')) => ('i2f64', ('ishr', ('ishl', 'a', 16), 16)) */
   /* search168_0_0 -> 0 in the cache */
   /* search168_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace168_0_0_0 -> 0 in the cache */
   /* replace168_0_0_1 -> 38 in the cache */
   /* replace168_0_0 -> 39 in the cache */
   /* replace168_0_1 -> 38 in the cache */
   /* replace168_0 -> 40 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 40 },
      -1,
   } },

   /* ('i2f64', ('u2u16', 'a@64')) => ('i2f64', ('ishr', ('ishl', 'a', 48), 48)) */
   /* search169_0_0 -> 5 in the cache */
   /* search169_0 -> 6 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 6 },
      -1,
   } },

   /* replace169_0_0_0 -> 5 in the cache */
   /* replace169_0_0_1 -> 43 in the cache */
   /* replace169_0_0 -> 44 in the cache */
   /* replace169_0_1 -> 43 in the cache */
   /* replace169_0 -> 45 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 45 },
      -1,
   } },

   /* ('i2f64', ('i2i16', 'a@32')) => ('i2f64', ('ishr', ('ishl', 'a', 16), 16)) */
   /* search170_0_0 -> 0 in the cache */
   /* search170_0 -> 11 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 11 },
      -1,
   } },

   /* replace170_0_0_0 -> 0 in the cache */
   /* replace170_0_0_1 -> 38 in the cache */
   /* replace170_0_0 -> 39 in the cache */
   /* replace170_0_1 -> 38 in the cache */
   /* replace170_0 -> 40 in the cache */
   /* replace170 -> 121 in the cache */

   /* ('i2f64', ('i2i16', 'a@64')) => ('i2f64', ('ishr', ('ishl', 'a', 48), 48)) */
   /* search171_0_0 -> 5 in the cache */
   /* search171_0 -> 14 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 14 },
      -1,
   } },

   /* replace171_0_0_0 -> 5 in the cache */
   /* replace171_0_0_1 -> 43 in the cache */
   /* replace171_0_0 -> 44 in the cache */
   /* replace171_0_1 -> 43 in the cache */
   /* replace171_0 -> 45 in the cache */
   /* replace171 -> 123 in the cache */

   /* ('i2f64', ('f2u16', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', -32768.0), 32767.0)) */
   /* search172_0_0 -> 0 in the cache */
   /* search172_0 -> 73 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 73 },
      -1,
   } },

   /* replace172_0_0_0 -> 0 in the cache */
   /* replace172_0_0_1 -> 108 in the cache */
   /* replace172_0_0 -> 109 in the cache */
   /* replace172_0_1 -> 110 in the cache */
   /* replace172_0 -> 111 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_f2f64,
      -1, 2,
      { 111 },
      -1,
   } },

   /* ('i2f64', ('f2u16', 'a@64')) => ('fmin', ('fmax', 'a', -32768.0), 32767.0) */
   /* search173_0_0 -> 5 in the cache */
   /* search173_0 -> 79 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 79 },
      -1,
   } },

   /* replace173_0_0 -> 5 in the cache */
   /* replace173_0_1 -> 113 in the cache */
   /* replace173_0 -> 114 in the cache */
   /* replace173_1 -> 115 in the cache */
   /* replace173 -> 116 in the cache */

   /* ('i2f64', ('f2i16', 'a@32')) => ('f2f64', ('fmin', ('fmax', 'a', -32768.0), 32767.0)) */
   /* search174_0_0 -> 0 in the cache */
   /* search174_0 -> 86 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 86 },
      -1,
   } },

   /* replace174_0_0_0 -> 0 in the cache */
   /* replace174_0_0_1 -> 108 in the cache */
   /* replace174_0_0 -> 109 in the cache */
   /* replace174_0_1 -> 110 in the cache */
   /* replace174_0 -> 111 in the cache */
   /* replace174 -> 127 in the cache */

   /* ('i2f64', ('f2i16', 'a@64')) => ('fmin', ('fmax', 'a', -32768.0), 32767.0) */
   /* search175_0_0 -> 5 in the cache */
   /* search175_0 -> 88 in the cache */
   { .expression = {
      { nir_search_value_expression, 64 },
      false,
      false,
      false,
      nir_op_i2f64,
      -1, 0,
      { 88 },
      -1,
   } },

   /* replace175_0_0 -> 5 in the cache */
   /* replace175_0_1 -> 113 in the cache */
   /* replace175_0 -> 114 in the cache */
   /* replace175_1 -> 115 in the cache */
   /* replace175 -> 116 in the cache */

   /* ('f2f32', ('u2u16', 'a@32')) => ('unpack_half_2x16_split_x', 'a') */
   /* search176_0_0 -> 0 in the cache */
   /* search176_0 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_f2f32,
      -1, 0,
      { 1 },
      -1,
   } },

   /* replace176_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_unpack_half_2x16_split_x,
      -1, 0,
      { 0 },
      -1,
   } },

   /* ('u2u32', ('f2f16_rtz', 'a@32')) => ('pack_half_2x16_split', 'a', 0) */
   /* search177_0_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 16 },
      false,
      false,
      false,
      nir_op_f2f16_rtz,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_u2u32,
      -1, 0,
      { 133 },
      -1,
   } },

   /* replace177_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0x0 /* 0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_pack_half_2x16_split,
      -1, 0,
      { 0, 135 },
      -1,
   } },

};



static const struct transform dxil_nir_lower_16bit_conv_transforms[] = {
   { ~0, ~0, ~0 }, /* Sentinel */

   { 2, 4, 0 },
   { 7, 10, 0 },
   { 25, 26, 0 },
   { 27, 9, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 12, 13, 0 },
   { 15, 10, 0 },
   { 28, 26, 0 },
   { 29, 30, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 18, 20, 0 },
   { 31, 33, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 22, 24, 0 },
   { 34, 36, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 134, 136, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 37, 41, 0 },
   { 42, 46, 0 },
   { 55, 56, 0 },
   { 57, 58, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 47, 40, 0 },
   { 48, 46, 0 },
   { 59, 56, 0 },
   { 60, 45, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 49, 51, 0 },
   { 61, 63, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 52, 54, 0 },
   { 64, 66, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 67, 68, 0 },
   { 69, 70, 0 },
   { 90, 91, 0 },
   { 92, 93, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 71, 68, 0 },
   { 72, 70, 0 },
   { 94, 91, 0 },
   { 95, 93, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 74, 78, 0 },
   { 80, 85, 0 },
   { 96, 97, 0 },
   { 98, 84, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 87, 78, 0 },
   { 89, 85, 0 },
   { 99, 97, 0 },
   { 100, 84, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 101, 102, 0 },
   { 103, 104, 0 },
   { 120, 121, 0 },
   { 122, 123, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 105, 102, 0 },
   { 106, 104, 0 },
   { 124, 121, 0 },
   { 125, 123, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 107, 111, 0 },
   { 112, 117, 0 },
   { 126, 127, 0 },
   { 128, 116, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 118, 111, 0 },
   { 119, 117, 0 },
   { 129, 127, 0 },
   { 130, 116, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 131, 132, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

};

static const struct per_op_table dxil_nir_lower_16bit_conv_pass_op_table[nir_num_search_ops] = {
   [nir_search_op_u2u] = {
      .filter = (const uint16_t []) {
         0,
         0,
         1,
         2,
         3,
         4,
         5,
         1,
         1,
         1,
         1,
         1,
         2,
         2,
         2,
         2,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 6,
      .table = (const uint16_t []) {
      
         2,
         7,
         8,
         9,
         10,
         11,
      },
   },
   [nir_search_op_i2i] = {
      .filter = (const uint16_t []) {
         0,
         0,
         1,
         2,
         3,
         4,
         0,
         1,
         1,
         1,
         1,
         1,
         2,
         2,
         2,
         2,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 5,
      .table = (const uint16_t []) {
      
         3,
         12,
         13,
         14,
         15,
      },
   },
   [nir_search_op_f2u] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         4,
      },
   },
   [nir_search_op_f2i] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         5,
      },
   },
   [nir_search_op_u2f] = {
      .filter = (const uint16_t []) {
         0,
         0,
         1,
         2,
         3,
         4,
         0,
         1,
         1,
         1,
         1,
         1,
         2,
         2,
         2,
         2,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 5,
      .table = (const uint16_t []) {
      
         0,
         16,
         17,
         18,
         19,
      },
   },
   [nir_search_op_i2f] = {
      .filter = (const uint16_t []) {
         0,
         0,
         1,
         2,
         3,
         4,
         0,
         1,
         1,
         1,
         1,
         1,
         2,
         2,
         2,
         2,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 5,
      .table = (const uint16_t []) {
      
         0,
         20,
         21,
         22,
         23,
      },
   },
   [nir_search_op_f2f] = {
      .filter = (const uint16_t []) {
         0,
         0,
         1,
         0,
         0,
         0,
         0,
         1,
         1,
         1,
         1,
         1,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 2,
      .table = (const uint16_t []) {
      
         0,
         24,
      },
   },
   [nir_op_f2f16_rtz] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         6,
      },
   },
};

/* Mapping from state index to offset in transforms (0 being no transforms) */
static const uint16_t dxil_nir_lower_16bit_conv_transform_offsets[] = {
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   1,
   6,
   11,
   14,
   17,
   19,
   24,
   29,
   32,
   35,
   40,
   45,
   50,
   55,
   60,
   65,
   70,
   75,
};

static const nir_algebraic_table dxil_nir_lower_16bit_conv_table = {
   .transforms = dxil_nir_lower_16bit_conv_transforms,
   .transform_offsets = dxil_nir_lower_16bit_conv_transform_offsets,
   .pass_op_table = dxil_nir_lower_16bit_conv_pass_op_table,
   .values = dxil_nir_lower_16bit_conv_values,
   .expression_cond = NULL,
   .variable_cond = NULL,
};

bool
dxil_nir_lower_16bit_conv(nir_shader *shader)
{
   bool progress = false;
   bool condition_flags[1];
   const nir_shader_compiler_options *options = shader->options;
   const shader_info *info = &shader->info;
   (void) options;
   (void) info;

   STATIC_ASSERT(137 == ARRAY_SIZE(dxil_nir_lower_16bit_conv_values));
   condition_flags[0] = true;

   nir_foreach_function_impl(impl, shader) {
     progress |= nir_algebraic_impl(impl, condition_flags, &dxil_nir_lower_16bit_conv_table);
   }

   return progress;
}


#include "nir.h"
#include "nir_builder.h"
#include "nir_search.h"
#include "nir_search_helpers.h"

/* What follows is NIR algebraic transform code for the following 2
 * transforms:
 *    ('b2b32', 'a') => ('b2i32', 'a')
 *    ('b2b1', 'a') => ('ine', ('b2i32', 'a'), 0)
 */


static const nir_search_value_union dxil_nir_algebraic_values[] = {
   /* ('b2b32', 'a') => ('b2i32', 'a') */
   { .variable = {
      { nir_search_value_variable, -1 },
      0, /* a */
      false,
      nir_type_invalid,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_b2b32,
      -1, 0,
      { 0 },
      -1,
   } },

   /* replace178_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      nir_op_b2i32,
      -1, 0,
      { 0 },
      -1,
   } },

   /* ('b2b1', 'a') => ('ine', ('b2i32', 'a'), 0) */
   /* search179_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 1 },
      false,
      false,
      false,
      nir_op_b2b1,
      -1, 0,
      { 0 },
      -1,
   } },

   /* replace179_0_0 -> 0 in the cache */
   /* replace179_0 -> 2 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0x0 /* 0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 1 },
      false,
      false,
      false,
      nir_op_ine,
      0, 1,
      { 2, 4 },
      -1,
   } },

};



static const struct transform dxil_nir_algebraic_transforms[] = {
   { ~0, ~0, ~0 }, /* Sentinel */

   { 1, 2, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 3, 5, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

};

static const struct per_op_table dxil_nir_algebraic_pass_op_table[nir_num_search_ops] = {
   [nir_op_b2b32] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         2,
      },
   },
   [nir_op_b2b1] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         3,
      },
   },
};

/* Mapping from state index to offset in transforms (0 being no transforms) */
static const uint16_t dxil_nir_algebraic_transform_offsets[] = {
   0,
   0,
   1,
   3,
};

static const nir_algebraic_table dxil_nir_algebraic_table = {
   .transforms = dxil_nir_algebraic_transforms,
   .transform_offsets = dxil_nir_algebraic_transform_offsets,
   .pass_op_table = dxil_nir_algebraic_pass_op_table,
   .values = dxil_nir_algebraic_values,
   .expression_cond = NULL,
   .variable_cond = NULL,
};

bool
dxil_nir_algebraic(nir_shader *shader)
{
   bool progress = false;
   bool condition_flags[1];
   const nir_shader_compiler_options *options = shader->options;
   const shader_info *info = &shader->info;
   (void) options;
   (void) info;

   STATIC_ASSERT(6 == ARRAY_SIZE(dxil_nir_algebraic_values));
   condition_flags[0] = true;

   nir_foreach_function_impl(impl, shader) {
     progress |= nir_algebraic_impl(impl, condition_flags, &dxil_nir_algebraic_table);
   }

   return progress;
}

