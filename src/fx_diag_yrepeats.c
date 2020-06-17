#include <gbdk/platform.h>
#include <gb/cgb.h>
#include <types.h>
#include <stdint.h>

#include "input.h"

// ============ DIAGONAL SLIDE IN EFFECT ============

static int8_t   y_scroll_offset;
static uint8_t  y_start_effect;
static int8_t   y_start_effect_dir;
#define Y_LINE_MIN (0U)//(1 + 1)
#define Y_LINE_MAX (144U - 1U)
#define Y_SCROLL_MIN -128U

// #define START_Y 16
#define SCX_SCY_XOR_BITS 0xFA // 0xFA nice effect //0x05 ok effect
// #define SCX_SCY_AND_BITS 0x1A // 0x0A ok effect



void scanline_isr_repeat_last_y(void) {

    // Scroll in the Y direction by -1 every 1 scanlines
    // after the effect start line is reached

    // That keeps the previous scanline repeating every time

    // Hide image by showing Line after bottom of screen when effect is at zero
    if (!y_start_effect) {
        SCY_REG = 144U - LY_REG;
    }
    else if (LY_REG > y_start_effect) {

        // Update the scroll Y register with the current offset
        // It's the equivalent of : // SCY_REG = (y_start_effect - LY_REG);
        y_scroll_offset--;
        //if (y_scroll_offset > 0)
            SCY_REG = y_scroll_offset;

        // Vary the amount of veritcal offset based on the current scanline numbers low bits
        #ifdef SCX_SCY_AND_BITS
            SCX_REG = SCY_REG & SCX_SCY_AND_BITS;
        #else
            SCX_REG = SCY_REG ^ SCX_SCY_XOR_BITS;
        #endif
    }
}


void fx_repeat_last_y(void) {

    y_scroll_offset    = 0;
    y_start_effect     =  Y_LINE_MAX;// 0 + START_Y;  // Start with effect at bottom of screen
    y_start_effect_dir = -1; // Start with rolling effect up toward top

    // Add Hblank isr and enable interrupts
    disable_interrupts();
    STAT_REG = STAT_REG | 0x08;  // Enable HBLANK (Mode 1) STAT ISR
    add_LCD(scanline_isr_repeat_last_y);
    add_LCD(nowait_int_handler); // Disable wait on VRAM state before returning from SIO interrupt
    set_interrupts(VBL_IFLAG | LCD_IFLAG);
    enable_interrupts();

    while (1) {

        wait_vbl_done();

        // At the start of every frame...
        SCY_REG = 0;          // Reset scroll to zero
        SCX_REG = 0;
        y_scroll_offset = 0;  // Reset scroll offset to zero
        if (sys_time & 0x01) {
            y_start_effect += y_start_effect_dir;

            // Reverse direction as needed
            if (y_start_effect <= Y_LINE_MIN) {
                y_start_effect_dir = 1;
                for (uint8_t c = 0; c < 45; c++) {
                    wait_vbl_done(); // short delay between rolling back down
                    SCY_REG = 0;          // Reset scroll to zero
                    SCX_REG = 0;
                    y_scroll_offset = 0;  // Reset scroll offset to zero
                }
            }
            else if (y_start_effect >= Y_LINE_MAX) {
                y_start_effect_dir = -1;
                break;
                // for (uint8_t c = 0; c < 90; c++) {
                //     wait_vbl_done(); // short delay between rolling back up
                //     SCY_REG = 0;          // Reset scroll to zero
                //     SCX_REG = 0;
                //     y_scroll_offset = 0;  // Reset scroll offset to zero
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
    remove_LCD(scanline_isr_repeat_last_y);
    set_interrupts(VBL_IFLAG);
    enable_interrupts();

    // Wait until button pressed before exiting
//    waitpadticked_lowcpu(J_A | J_B | J_START, NULL);
}

