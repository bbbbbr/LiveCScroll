#include <gbdk/platform.h>
#include <gb/cgb.h>
#include <types.h>
#include <stdint.h>

#include "input.h"

// ============ DIAGONAL SLIDE IN EFFECT ============

static uint8_t scroll_x_amount;
static uint8_t y_start_effect;
static int8_t  y_start_effect_dir;
static uint8_t y_end_effect;

// #define EFFECT_Y_LINE_MAX      143U
// #define EFFECT_START_Y          16U
#define SCX_INVERT_WAVE_BITS  0xFAU

#define Y_LINE_MIN (0U)//(1 + 1)
#define Y_LINE_MAX (144U - 1U)



void scanline_isr_horiz_waves(void) {


    // Hide image by showing Line after bottom of screen when effect is at zero
    if (!y_end_effect) {
        SCY_REG = 144U - LY_REG;
    }
    else if (LY_REG > y_start_effect) {
        // Don't start until line [y_start_effect]

        // Horizontal waves effect

        // For every line after [y_start_effect] scroll
        // left or right by the number of lines since
        // [y_start_effect] * 4, alternate left/right
        // every other line.
        scroll_x_amount += 5U; // Useful values: 2U - 8U

        if (LY_REG & 0x01U)
            // equiv to: SCX_REG = (LY_REG - y_start_effect) << 2;
            SCX_REG = scroll_x_amount;
        else
            // equiv to: SCX_REG = 255U - ((LY_REG - y_start_effect) << 2);
            SCX_REG = 255U - scroll_x_amount;

        // Hide background after end effect line is reached
        if (LY_REG > y_end_effect)
            SCY_REG = 144U - LY_REG;
    }
}





void fx_horiz_waves(void) {

    scroll_x_amount    = 0;
    y_start_effect     = 1;
    y_start_effect_dir = 1;
    y_start_effect = Y_LINE_MAX;
    y_start_effect_dir = -1;
    y_end_effect = 254;

    // Add Hblank isr and enable interrupts
    disable_interrupts();
    STAT_REG = STAT_REG | 0x08;  // Enable HBLANK (Mode 1) STAT ISR
    add_LCD(scanline_isr_horiz_waves);
    add_LCD(nowait_int_handler); // Disable wait on VRAM state before returning from SIO interrupt
    set_interrupts(VBL_IFLAG | LCD_IFLAG);
    enable_interrupts();

    while (1) {
        wait_vbl_done();

        // At the start of every frame...
        SCY_REG = 0;          // Reset scroll to zero
        SCX_REG = 0;
        scroll_x_amount = 0;  // Reset scroll offset to zero

        if (sys_time & 0x01) {
            y_start_effect += y_start_effect_dir;
            if ((y_start_effect) && (y_start_effect < 128)) {
                y_end_effect += y_start_effect_dir;
                y_end_effect += y_start_effect_dir;
            }

            // Reverse direction as needed
            if (y_start_effect <= Y_LINE_MIN) {
                y_start_effect_dir = 1;
                for (uint8_t c = 0; c < 45; c++) {
                    wait_vbl_done(); // short delay between rolling back down
                    SCY_REG = 0;          // Reset scroll to zero
                    SCX_REG = 0;
                    scroll_x_amount = 0U;
                }
            }
            else if (y_start_effect >= Y_LINE_MAX) {
                y_start_effect_dir = -1;
                break;
                // for (uint8_t c = 0; c < 90; c++) {
                //     wait_vbl_done(); // short delay between rolling back up
                //     SCY_REG = 0;          // Reset scroll to zero
                //     SCX_REG = 0;
                //     scroll_x_amount = 0U;
                // }
            }
        }

        // // Break after intial run is complete
        // if (y_start_effect >= Y_LINE_MAX) break;

        UPDATE_KEYS();
        if (KEY_TICKED(J_A | J_B | J_START))
            break;
    }

    // Disable HBlank interrupt
    disable_interrupts();
    STAT_REG = STAT_REG ^ 0x08;  // De-Enable HBLANK (Mode 1) STAT ISR
    remove_LCD(nowait_int_handler);
    remove_LCD(scanline_isr_horiz_waves);
    set_interrupts(VBL_IFLAG);
    enable_interrupts();

    // Wait until button pressed before exiting
//    waitpadticked_lowcpu(J_A | J_B | J_START, NULL);
}

