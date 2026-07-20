/*******************************************************************************
 * Size: 28 px
 * Bpp: 4
 * Opts: --font C:/Windows/Fonts/SegoeIcons.ttf --size 28 --bpp 4 --format lvgl --range 0xEA80-0xEA80 -o font_light_28.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef FONT_LIGHT_28
#define FONT_LIGHT_28 1
#endif

#if FONT_LIGHT_28

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+EA80 "" */
    0x0, 0xf9, 0xab, 0xbf, 0xdd, 0x4c, 0x1, 0xff,
    0xc0, 0x7f, 0x95, 0x3, 0x30, 0x2c, 0xfb, 0x80,
    0x7e, 0x1c, 0x81, 0x7d, 0xec, 0xc7, 0x6b, 0x8c,
    0x60, 0x80, 0x70, 0xe1, 0xa7, 0xc1, 0x0, 0x62,
    0x8f, 0x43, 0xc1, 0x0, 0xd4, 0x4d, 0x62, 0x1,
    0xf8, 0x6d, 0x8a, 0x80, 0x25, 0x43, 0x90, 0xf,
    0xfe, 0x4, 0x9a, 0x20, 0x1, 0x21, 0xc0, 0x1f,
    0xfc, 0x2e, 0x8, 0x2, 0x34, 0x20, 0xf, 0xfe,
    0x11, 0x21, 0x8b, 0x86, 0x80, 0x7f, 0xf1, 0x30,
    0x10, 0x80, 0x80, 0x3f, 0xf8, 0x86, 0x4, 0x1,
    0xff, 0xca, 0x10, 0x10, 0xf, 0xfe, 0x20, 0x80,
    0xa8, 0x60, 0x7, 0xff, 0x13, 0x1, 0x4c, 0x5c,
    0x3, 0xff, 0x88, 0xe2, 0x60, 0xc2, 0xe0, 0x1f,
    0xfc, 0x28, 0x16, 0x0, 0x48, 0xc2, 0x0, 0x7f,
    0xf0, 0x11, 0xc6, 0x40, 0x3, 0x41, 0x66, 0x1,
    0xfe, 0x3b, 0xa, 0x10, 0x9, 0x28, 0x34, 0x3,
    0xfd, 0xa1, 0x48, 0x1, 0xca, 0xc2, 0xa0, 0x1f,
    0x94, 0x5d, 0x40, 0x3e, 0xa0, 0xc0, 0xf, 0xd8,
    0x16, 0x1, 0xf8, 0xc1, 0x0, 0x3f, 0x20, 0x18,
    0x7, 0xf1, 0x97, 0xff, 0xe2, 0x40, 0xf, 0xf2,
    0x1, 0x19, 0xf8, 0x80, 0xc0, 0x3f, 0xd8, 0xb,
    0x99, 0xe5, 0xc, 0x0, 0xff, 0x20, 0x20, 0x7,
    0x90, 0x10, 0x3, 0xfe, 0x43, 0x30, 0x6, 0x33,
    0x20, 0x7, 0xff, 0x3, 0xc3, 0x33, 0xc1, 0xe0,
    0x1f, 0xfc, 0x4, 0xb3, 0x3f, 0x5a, 0x0, 0x78
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 448, .box_w = 22, .box_h = 28, .ofs_x = 3, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 60032, .range_length = 1, .glyph_id_start = 1,
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
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 1,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t font_light_28 = {
#else
lv_font_t font_light_28 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 28,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FONT_LIGHT_28*/

