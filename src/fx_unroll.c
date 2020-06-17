#include <gbdk/platform.h>
#include <gb/cgb.h>
#include <types.h>
#include <stdint.h>

#include "input.h"

// ============ DIAGONAL SLIDE IN EFFECT ============

static uint8_t  first_loop;

static uint8_t  y_before_end_of_unroll;
static uint8_t  y_scroll_offset;
static uint8_t  y_counter;
static int8_t   y_counter_dir;
static uint8_t  y_start_effect;
#define Y_UNROLL_SIZE 20U
#define Y_COUNT_MIN 1U
#define Y_COUNT_MAX 143U //(143 + Y_UNROLL_SIZE)
#define Y_COUNT_FINISH_UNROLL (143U - Y_UNROLL_SIZE)
#define Y_SCREEN_MAX 143U


void scanline_isr_unroll(void) {

    // Once the Y line of starting the effect is reached
    // Show the unroll region flipped in the Y direction
    // This is done by scrolling to end of unroll at first, and
    // then scrolling it backward once each new scanline
    // (requires SCY subtracting 2 lines per scanline to scroll upwards)

    // If it's before the end of the unroll effect, go ahead
    if (y_before_end_of_unroll) {

        // SCY is only> 0 in the unroll region
        if (SCY_REG) {
            SCY_REG -= 2;
            // End of effect reached, turn off unrolling for rest of frame
            if (SCY_REG == 0) {
                y_before_end_of_unroll = 0;
            }
        }
        else if (LY_REG == y_start_effect) {
            // Start of unroll region, set initial SCY value
            // to 2X
            SCY_REG = y_scroll_offset << 1;
        }
    } else {
        // Once past the end of the unroll region
        // Scroll each scanline past to show scanline 144
        // in order to hide the rest of the screen image
        // (expecting that 145 on the map is set to empty)
        SCY_REG = 144U - LY_REG;
    }
}


void fx_unroll(void) {

    first_loop = 0;

    y_before_end_of_unroll = 1;
    y_scroll_offset  = Y_UNROLL_SIZE;

    // == To start with image hidden ==
    // y_counter = Y_COUNT_MIN;
    // y_counter_dir = 1; // Start with rolling effect up toward top

    // == To start with image visible ==
    y_counter = Y_SCREEN_MAX;
    y_counter_dir = -1; // Start with rolling effect up toward top

    // Add Hblank isr and enable interrupts
    disable_interrupts();
    STAT_REG = STAT_REG | 0x08;  // Enable HBLANK (Mode 1) STAT ISR
    add_LCD(scanline_isr_unroll);
    add_LCD(nowait_int_handler); // Disable wait on VRAM state before returning from SIO interrupt
    set_interrupts(STAT_REG | LCD_IFLAG);
    enable_interrupts();

    while (1) {

        // At the start of every frame...
        __critical {
            SCY_REG = 0;          // Reset scroll to zero
            y_before_end_of_unroll = 1;
        }

        // Handle roll effect appearing at top of screen
        // (hold effect start at first line and unroll size
        //  from zero up to full size, then release start line)
        if (y_counter < Y_UNROLL_SIZE) {
            y_scroll_offset = y_counter;
            y_start_effect = 1;
        }
        // Handle roll effect finishing at bottom of screen
        else if (y_counter >= Y_COUNT_FINISH_UNROLL) {
            y_scroll_offset = (Y_UNROLL_SIZE - 1) - (y_counter - Y_COUNT_FINISH_UNROLL);
            y_start_effect = y_counter - y_scroll_offset + 1;
        }
        else {
            y_scroll_offset = Y_UNROLL_SIZE;
            // y_start_effect = y_counter - Y_UNROLL_SIZE + 1;
            y_start_effect = y_counter - y_scroll_offset + 1;
        }

        // Don't change start effect line every frame
        // if ((sys_time & 0x0F) == 0x0F) // Every 16th frame
        // if ((sys_time & 0x07) == 0x07) // Every 8th frame
        // if ((sys_time & 0x03) == 0x03) // Every 4th frame
        // if (sys_time & 0x01)           // Every 2nd frame
        {
            y_counter += y_counter_dir;

            // Reverse direction as needed
            if (y_counter <= Y_COUNT_MIN) {
                y_counter_dir = 1;
                for (uint8_t c = 0; c < 45; c++) wait_vbl_done(); // short delay between rolling back up
            }
            else if (y_counter >= Y_COUNT_MAX) {
                y_counter_dir = -1;
                break;
                //for (uint8_t c = 0; c < 90; c++) wait_vbl_done(); // short delay between rolling back up
            }
        }

        UPDATE_KEYS();
        if (KEY_TICKED(J_A | J_B | J_START))
            break;

        // Turn the display on after the very first loop
        if (!first_loop) {
            SHOW_BKG;
            DISPLAY_ON;
            first_loop++;
        }

        wait_vbl_done();
    }

    // Disable HBlank interrupt
    disable_interrupts();
    STAT_REG = STAT_REG ^ 0x08;  // De-Enable HBLANK (Mode 1) STAT ISR
    remove_LCD(nowait_int_handler);
    remove_LCD(scanline_isr_unroll);
    set_interrupts(VBL_IFLAG);
    enable_interrupts();

    // Wait until button pressed before exiting
//    waitpadticked_lowcpu(J_A | J_B | J_START, NULL);
}

