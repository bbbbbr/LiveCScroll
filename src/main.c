#include <gb/gb.h>
#include <gb/cgb.h> // Include cgb functions
#include "asm/types.h"

#include "input.h"

#include "../res/bg_wowdog_tiles.h"

#include "../res/bg_wowdog_map.h"
#include "../res/bg_wowdog_tiles.h"

#include "fx_horiz_waves.h"
#include "fx_diag_yrepeats.h"
#include "fx_vert_squish.h"
#include "fx_unroll.h"


#define BG_PAL_0    0x00
#define BG_PAL_1    0x01
#define BG_PAL_2    0x02
#define BG_PAL_3    0x03
#define BG_PAL_4    0x04
#define BG_PAL_5    0x05
#define BG_PAL_6    0x06
#define BG_PAL_7    0x07

// Palettes 0..3
const UWORD bg_sky_palette_lo[] = {
    bg_wowdog_tilesCGBPal0c0, bg_wowdog_tilesCGBPal0c1, bg_wowdog_tilesCGBPal0c2, bg_wowdog_tilesCGBPal0c3,
    bg_wowdog_tilesCGBPal1c0, bg_wowdog_tilesCGBPal1c1, bg_wowdog_tilesCGBPal1c2, bg_wowdog_tilesCGBPal1c3,
    bg_wowdog_tilesCGBPal2c0, bg_wowdog_tilesCGBPal2c1, bg_wowdog_tilesCGBPal2c2, bg_wowdog_tilesCGBPal2c3,
    bg_wowdog_tilesCGBPal3c0, bg_wowdog_tilesCGBPal3c1, bg_wowdog_tilesCGBPal3c2, bg_wowdog_tilesCGBPal3c3,
};

const UWORD bg_sky_palette_hi[] = {
    bg_wowdog_tilesCGBPal4c0, bg_wowdog_tilesCGBPal4c1, bg_wowdog_tilesCGBPal4c2, bg_wowdog_tilesCGBPal4c3,
    bg_wowdog_tilesCGBPal5c0, bg_wowdog_tilesCGBPal5c1, bg_wowdog_tilesCGBPal5c2, bg_wowdog_tilesCGBPal5c3,
    bg_wowdog_tilesCGBPal6c0, bg_wowdog_tilesCGBPal6c1, bg_wowdog_tilesCGBPal6c2, bg_wowdog_tilesCGBPal6c3,
    bg_wowdog_tilesCGBPal7c0, bg_wowdog_tilesCGBPal7c1, bg_wowdog_tilesCGBPal7c2, bg_wowdog_tilesCGBPal7c3,
};


unsigned char vbl_count;

void init_gfx(void);

void init_gfx(void) {

    // Load color palettes
    set_bkg_palette(BG_PAL_0, 4, bg_sky_palette_lo); // UBYTE first_palette, UBYTE nb_palettes, UWORD *rgb_data
    set_bkg_palette(BG_PAL_4, 4, bg_sky_palette_hi); // UBYTE first_palette, UBYTE nb_palettes, UWORD *rgb_data

    // Load tiles (background + window)
    // set_bkg_data(0, 19, bg_wowdog_tiles);
    // set_bkg_data(20, 120, bg_wowdog_tiles);
    set_bkg_data(0, 240, bg_wowdog_tiles);

    // Load BG tile attribute map
    VBK_REG = 1;
    // set_bkg_tiles(0, 0, 20, 18, bg_sky_mapPLN1);
    set_bkg_tiles(0, 0, 20, 18, bg_wowdog_mapPLN1);

    // Load BG tile map
    VBK_REG = 0;
    // set_bkg_tiles(0, 0, 20, 18, bg_sky_mapPLN0);
    set_bkg_tiles(0, 0, 20, 18, bg_wowdog_mapPLN0);

    SHOW_BKG;
    DISPLAY_ON;
}

void main() {

    init_gfx();

    while(1) {

        UPDATE_KEYS();
        if (KEY_TICKED(J_A)) {
            fx_repeat_last_y();
        }
        else if (KEY_TICKED(J_B)) {
            fx_horiz_waves();
        }
        else if (KEY_TICKED(J_START)) {
            fx_unroll();
        }
        else if (KEY_TICKED(J_UP)) {
            fx_horiz_waves();
            fx_unroll();
            fx_repeat_last_y();
        }

        // fx_vert_squish();

        wait_vbl_done();
    }
}
