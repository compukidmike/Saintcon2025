/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --bpp 1 --size 16 --no-compress --no-prefilter --font /Users/dwarkentin/Downloads/whitrabt.ttf -r 0x20-0x7E --format lvgl --lv-include lvgl.h -o /Users/dwarkentin/src/projects/Saintcon2025Dev/Firmware/badge/components/ui/fonts/white_rabbit_16.c --force-fast-kern-format
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef WHITE_RABBIT_16
#define WHITE_RABBIT_16 1
#endif

#if WHITE_RABBIT_16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0xc0, 0xff, 0x80,

    /* U+0022 "\"" */
    0xde, 0xf6,

    /* U+0023 "#" */
    0x6c, 0x6c, 0x6c, 0xff, 0x6c, 0x6c, 0xff, 0x6c,
    0x6c, 0x6c, 0x6c,

    /* U+0024 "$" */
    0x18, 0x18, 0x7f, 0xd8, 0xd8, 0x7e, 0x1b, 0x1b,
    0xfe, 0x18, 0x18,

    /* U+0025 "%" */
    0xe3, 0xe3, 0xe7, 0x6, 0xc, 0x18, 0x30, 0x60,
    0xe7, 0xc7, 0xc7,

    /* U+0026 "&" */
    0x70, 0xd8, 0xd8, 0xd8, 0x70, 0x70, 0xdb, 0xdf,
    0xce, 0xce, 0x7a,

    /* U+0027 "'" */
    0xfc,

    /* U+0028 "(" */
    0x7b, 0x6d, 0xb6, 0xd9, 0x80,

    /* U+0029 ")" */
    0xe3, 0x33, 0x33, 0x33, 0x33, 0xe0,

    /* U+002A "*" */
    0x6c, 0x6c, 0x38, 0xff, 0x38, 0x6c, 0x6c,

    /* U+002B "+" */
    0x18, 0x18, 0x18, 0xff, 0x18, 0x18, 0x18, 0x18,

    /* U+002C "," */
    0xff, 0xec,

    /* U+002D "-" */
    0xff,

    /* U+002E "." */
    0xff, 0x80,

    /* U+002F "/" */
    0x3, 0x3, 0x3, 0x6, 0xc, 0x18, 0x30, 0x60,
    0xc0, 0xc0, 0xc0,

    /* U+0030 "0" */
    0x7e, 0xc3, 0xc7, 0xc7, 0xcf, 0xdb, 0xf3, 0xe3,
    0xe3, 0xc3, 0x7e,

    /* U+0031 "1" */
    0x31, 0xcf, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc7,
    0xc0,

    /* U+0032 "2" */
    0x7e, 0xc3, 0xc3, 0x3, 0x7, 0xe, 0x1c, 0x38,
    0x70, 0xe0, 0xff,

    /* U+0033 "3" */
    0x7e, 0xc3, 0xc3, 0x3, 0x3, 0x1e, 0x3, 0x3,
    0xc3, 0xc3, 0x7e,

    /* U+0034 "4" */
    0xc, 0xc, 0xcc, 0xcc, 0xcc, 0xcc, 0xff, 0xc,
    0xc, 0xc, 0xc,

    /* U+0035 "5" */
    0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xfe, 0x3, 0x3,
    0x3, 0x3, 0xfe,

    /* U+0036 "6" */
    0x7c, 0xc0, 0xc0, 0xc0, 0xc0, 0xfe, 0xc3, 0xc3,
    0xc3, 0xc3, 0x7e,

    /* U+0037 "7" */
    0xff, 0x3, 0x3, 0x3, 0x7, 0xe, 0x1c, 0x38,
    0x70, 0xe0, 0xc0,

    /* U+0038 "8" */
    0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0xc3, 0xc3,
    0xc3, 0xc3, 0x7e,

    /* U+0039 "9" */
    0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0x7f, 0x3, 0x3,
    0x3, 0x3, 0xfe,

    /* U+003A ":" */
    0xff, 0x80, 0x3f, 0xe0,

    /* U+003B ";" */
    0xff, 0x80, 0x3f, 0xfb, 0x0,

    /* U+003C "<" */
    0xc, 0x73, 0x8c, 0x63, 0x6, 0xc, 0x18, 0x70,
    0xc0,

    /* U+003D "=" */
    0xff, 0x0, 0x0, 0xff,

    /* U+003E ">" */
    0xc3, 0x86, 0xc, 0x18, 0x31, 0x8c, 0x73, 0x8c,
    0x0,

    /* U+003F "?" */
    0x7e, 0xc3, 0xc3, 0x3, 0x7, 0xe, 0x1c, 0x18,
    0x0, 0x0, 0x18,

    /* U+0040 "@" */
    0x7e, 0xc3, 0xc3, 0xcf, 0xdb, 0xdb, 0xce, 0xc0,
    0xc0, 0xc0, 0x7f,

    /* U+0041 "A" */
    0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3,

    /* U+0042 "B" */
    0xfe, 0xc3, 0xc3, 0xc3, 0xc3, 0xfe, 0xc3, 0xc3,
    0xc3, 0xc3, 0xfe,

    /* U+0043 "C" */
    0x7e, 0xc3, 0xc3, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc3, 0xc3, 0x7e,

    /* U+0044 "D" */
    0xfe, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xfe,

    /* U+0045 "E" */
    0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xfe, 0xc0, 0xc0,
    0xc0, 0xc0, 0xff,

    /* U+0046 "F" */
    0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xfe, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0,

    /* U+0047 "G" */
    0x7e, 0xc3, 0xc3, 0xc0, 0xc0, 0xcf, 0xc3, 0xc3,
    0xc3, 0xc3, 0x7e,

    /* U+0048 "H" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3,

    /* U+0049 "I" */
    0xfb, 0x18, 0xc6, 0x31, 0x8c, 0x63, 0x3e,

    /* U+004A "J" */
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
    0x83, 0xc3, 0x7e,

    /* U+004B "K" */
    0xc1, 0xc3, 0xc6, 0xcc, 0xd8, 0xf0, 0xf8, 0xcc,
    0xc6, 0xc3, 0xc1,

    /* U+004C "L" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xff,

    /* U+004D "M" */
    0xc3, 0xe7, 0xff, 0xdb, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3,

    /* U+004E "N" */
    0xc3, 0xc3, 0xe3, 0xd3, 0xcb, 0xcf, 0xc7, 0xc3,
    0xc3, 0xc3, 0xc3,

    /* U+004F "O" */
    0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0x7e,

    /* U+0050 "P" */
    0xfe, 0xc3, 0xc3, 0xc3, 0xc3, 0xfe, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0,

    /* U+0051 "Q" */
    0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xdf,
    0xce, 0xce, 0x7b,

    /* U+0052 "R" */
    0xfe, 0xc3, 0xc3, 0xc3, 0xc3, 0xfe, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3,

    /* U+0053 "S" */
    0x7e, 0xc3, 0xc3, 0xc0, 0xc0, 0x7e, 0x3, 0x3,
    0xc3, 0xc3, 0x7e,

    /* U+0054 "T" */
    0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18,

    /* U+0055 "U" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0x7e,

    /* U+0056 "V" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xe7,
    0x7e, 0x3c, 0x18,

    /* U+0057 "W" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xdb, 0xdb, 0xdb,
    0xdb, 0xdb, 0x7e,

    /* U+0058 "X" */
    0xc3, 0xc3, 0xc3, 0x66, 0x3c, 0x18, 0x3c, 0x66,
    0xc3, 0xc3, 0xc3,

    /* U+0059 "Y" */
    0xc3, 0xc3, 0xc3, 0x66, 0x3c, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18,

    /* U+005A "Z" */
    0xff, 0x3, 0x7, 0xe, 0x1c, 0x3c, 0x78, 0x70,
    0xe0, 0xc0, 0xff,

    /* U+005B "[" */
    0xfc, 0xcc, 0xcc, 0xcc, 0xcc, 0xf0,

    /* U+005C "\\" */
    0xc0, 0xc0, 0xc0, 0x60, 0x30, 0x18, 0xc, 0x6,
    0x3, 0x3, 0x3,

    /* U+005D "]" */
    0xf3, 0x33, 0x33, 0x33, 0x33, 0xf0,

    /* U+005E "^" */
    0x18, 0x3c, 0x7e, 0xe7, 0xc3,

    /* U+005F "_" */
    0xff,

    /* U+0060 "`" */
    0xdd, 0x80,

    /* U+0061 "a" */
    0x3e, 0x3, 0x3, 0x7f, 0xc3, 0xc3, 0xc3, 0x7f,

    /* U+0062 "b" */
    0xc0, 0xc0, 0xc0, 0xfe, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xfe,

    /* U+0063 "c" */
    0x7e, 0xc3, 0xc3, 0xc0, 0xc0, 0xc3, 0xc3, 0x7e,

    /* U+0064 "d" */
    0x3, 0x3, 0x3, 0x7f, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0x7f,

    /* U+0065 "e" */
    0x7e, 0xc3, 0xc3, 0xff, 0xc0, 0xc0, 0xc0, 0x7c,

    /* U+0066 "f" */
    0x3d, 0x86, 0x3e, 0x61, 0x86, 0x18, 0x61, 0x86,
    0x0,

    /* U+0067 "g" */
    0x7f, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7f, 0x3,
    0x3, 0x3e,

    /* U+0068 "h" */
    0xc0, 0xc0, 0xc0, 0xfe, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3,

    /* U+0069 "i" */
    0x30, 0x0, 0x3c, 0x30, 0xc3, 0xc, 0x30, 0xc7,
    0xc0,

    /* U+006A "j" */
    0xc, 0x0, 0xf, 0xc, 0x30, 0xc3, 0xc, 0x3c,
    0xf3, 0x78,

    /* U+006B "k" */
    0xc1, 0x83, 0x6, 0x3c, 0xdb, 0x3c, 0x78, 0xd9,
    0x9b, 0x18,

    /* U+006C "l" */
    0xf0, 0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc7,
    0xc0,

    /* U+006D "m" */
    0xfe, 0xdb, 0xdb, 0xdb, 0xdb, 0xdb, 0xdb, 0xdb,

    /* U+006E "n" */
    0xfe, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,

    /* U+006F "o" */
    0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e,

    /* U+0070 "p" */
    0xfe, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xfe, 0xc0,
    0xc0, 0xc0,

    /* U+0071 "q" */
    0x7f, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7f, 0x3,
    0x3, 0x3,

    /* U+0072 "r" */
    0xfb, 0x3c, 0xf0, 0xc3, 0xc, 0x30,

    /* U+0073 "s" */
    0x7f, 0xc0, 0xc0, 0x7e, 0x3, 0x3, 0x3, 0xfe,

    /* U+0074 "t" */
    0x61, 0x86, 0x3f, 0x61, 0x86, 0x18, 0x61, 0x83,
    0xc0,

    /* U+0075 "u" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7f,

    /* U+0076 "v" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xe7, 0x7e, 0x3c, 0x18,

    /* U+0077 "w" */
    0xc3, 0xc3, 0xdb, 0xdb, 0xdb, 0xdb, 0xdb, 0x7e,

    /* U+0078 "x" */
    0xc3, 0xe7, 0x7e, 0x3c, 0x3c, 0x7e, 0xe7, 0xc3,

    /* U+0079 "y" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7f, 0x3,
    0x3, 0x3e,

    /* U+007A "z" */
    0xff, 0x7, 0xe, 0x1c, 0x38, 0x70, 0xe0, 0xff,

    /* U+007B "{" */
    0x19, 0x8c, 0x63, 0x70, 0xc6, 0x31, 0x86,

    /* U+007C "|" */
    0xff, 0xff, 0xfc,

    /* U+007D "}" */
    0xe1, 0x8c, 0x63, 0xc, 0xc6, 0x31, 0xb8,

    /* U+007E "~" */
    0x70, 0xdb, 0xdb, 0xdb, 0xe
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 146, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 146, .box_w = 3, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 6, .adv_w = 146, .box_w = 5, .box_h = 3, .ofs_x = 2, .ofs_y = 8},
    {.bitmap_index = 8, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 19, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 30, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 41, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 52, .adv_w = 146, .box_w = 2, .box_h = 3, .ofs_x = 4, .ofs_y = 8},
    {.bitmap_index = 53, .adv_w = 146, .box_w = 3, .box_h = 11, .ofs_x = 4, .ofs_y = 0},
    {.bitmap_index = 58, .adv_w = 146, .box_w = 4, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 64, .adv_w = 146, .box_w = 8, .box_h = 7, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 71, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 79, .adv_w = 146, .box_w = 3, .box_h = 5, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 81, .adv_w = 146, .box_w = 8, .box_h = 1, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 82, .adv_w = 146, .box_w = 3, .box_h = 3, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 84, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 95, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 106, .adv_w = 146, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 115, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 137, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 148, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 159, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 170, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 181, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 192, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 146, .box_w = 3, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 207, .adv_w = 146, .box_w = 3, .box_h = 11, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 212, .adv_w = 146, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 221, .adv_w = 146, .box_w = 8, .box_h = 4, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 225, .adv_w = 146, .box_w = 6, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 234, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 245, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 256, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 267, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 278, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 300, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 311, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 322, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 333, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 344, .adv_w = 146, .box_w = 5, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 351, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 362, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 373, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 395, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 406, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 417, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 428, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 439, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 450, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 472, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 483, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 494, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 505, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 516, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 527, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 538, .adv_w = 146, .box_w = 4, .box_h = 11, .ofs_x = 4, .ofs_y = 0},
    {.bitmap_index = 544, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 555, .adv_w = 146, .box_w = 4, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 561, .adv_w = 146, .box_w = 8, .box_h = 5, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 566, .adv_w = 146, .box_w = 8, .box_h = 1, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 567, .adv_w = 146, .box_w = 3, .box_h = 3, .ofs_x = 4, .ofs_y = 8},
    {.bitmap_index = 569, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 577, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 588, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 596, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 607, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 615, .adv_w = 146, .box_w = 6, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 624, .adv_w = 146, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 634, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 645, .adv_w = 146, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 654, .adv_w = 146, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 664, .adv_w = 146, .box_w = 7, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 674, .adv_w = 146, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 683, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 691, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 699, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 707, .adv_w = 146, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 717, .adv_w = 146, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 727, .adv_w = 146, .box_w = 6, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 733, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 741, .adv_w = 146, .box_w = 6, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 750, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 758, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 766, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 774, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 782, .adv_w = 146, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 792, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 800, .adv_w = 146, .box_w = 5, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 807, .adv_w = 146, .box_w = 2, .box_h = 11, .ofs_x = 4, .ofs_y = 0},
    {.bitmap_index = 810, .adv_w = 146, .box_w = 5, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 817, .adv_w = 146, .box_w = 8, .box_h = 5, .ofs_x = 1, .ofs_y = 7}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t white_rabbit_16 = {
#else
lv_font_t white_rabbit_16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 14,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if WHITE_RABBIT_16*/

