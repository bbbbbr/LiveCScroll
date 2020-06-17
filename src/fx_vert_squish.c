#include <gbdk/platform.h>
#include <gb/cgb.h>
#include <types.h>
#include <stdint.h>
#include <stdbool.h>

#include "input.h"


// Scanline position double buffer
// uint8_t y_map[2][144]; // 0 - 143 | 144 - 153 (vblank)
uint8_t __at(0xD000) y_map[2][256] = {{0}, {0}}; // Compiler will allocate and check space if initialed. ?? Does it require full initialized?
uint8_t stat_prev_vals;

volatile uint8_t * p_buf_cur_frame = NULL;
volatile uint8_t * p_buf_next_frame = NULL;
volatile uint8_t cur_buf = 0;

#define SCANLINE_MIN 0U
#define SCANLINE_MAX 143U
#define SCANLINE_HIDEVAL (SCANLINE_MAX + 1U) // Will use graphics data on this scanline on HW map for blank/hidden screen
#define SCANLINE_SHOWALLVAL (SCANLINE_MIN)



// ============ VERITCAL STRETCH EFFECT ============

#define FX_SQUISH_SHIFTAMT 8

#define FX_DIR_INC  4
#define FX_DIR_DEC -4

#define FX_SQUISH_Y_INC_MIN 1U
#define FX_SQUISH_Y_INC_MAX (uint8_t)((255 / FX_DIR_INC) * FX_DIR_INC)    //255U - 8U // 252U // Should not exceed 256 + FX_DIR_INC


volatile bool fx_continue;
int8_t   y_fx_dir;
uint8_t  y_line_inc;
uint16_t y_line_calc;
// static uint16_t  y_line_inc;


const uint8_t SPEED_LUT[] = {3,3,4,4, 5,5,5,5};  // speed adjustments based on current stretch amount to create a more linear feel

void y_map_buffers_init(void) {
    p_buf_cur_frame = &(y_map[0][0]);
    p_buf_next_frame = &(y_map[1][0]);
    cur_buf = 1;
}

// ===========================================


void lcd_isr_register(int_handler f_vbl_isr) {

    // Add Hblank ISR and enable LCD hblank interrupt
    disable_interrupts();
    STAT_REG = STAT_REG | 0x08;     // Enable HBLANK (Mode 1) STAT ISR
    add_VBL(f_vbl_isr);
    // add_LCD(h_lcd_isr);          // LCD ISR is in separate ASM file
    // add_LCD(nowait_int_handler); // Disable wait on VRAM state before returning from SIO interrupt
    set_interrupts(VBL_IFLAG | LCD_IFLAG);
    IF_REG = 0x00;                  // Clear any pending interrupts before re-enabling interrupts
    enable_interrupts();

}

void lcd_isr_unregister(int_handler f_vbl_isr) {

    // Remove HBlank ISR, turn off LCD scanline interrupt
    disable_interrupts();
    STAT_REG = STAT_REG ^ 0x08;    // De-Enable HBLANK (Mode 1) STAT ISR
    remove_VBL(f_vbl_isr);
    // remove_LCD(nowait_int_handler);
    // remove_LCD(h_lcd_isr);      // LCD ISR is in separate ASM file
    set_interrupts(VBL_IFLAG);
    IF_REG = 0x00;                 // Clear any pending interrupts before re-enabling interrupts
    enable_interrupts();
}



void vblank_isr_swap_and_first_scy_from_buf(void) {

    if (cur_buf) {
        p_buf_cur_frame  = &y_map[1][0];
        p_buf_next_frame = &y_map[0][0];
    } else {
        p_buf_cur_frame  = &y_map[0][0];
        p_buf_next_frame = &y_map[1][0];
    }
    cur_buf ^= 0X01U;


    // Load up value for first hblank
    SCY_REG = p_buf_cur_frame[SCANLINE_MIN];

    // Reset for the new frame
    y_line_calc = 0x0000U;

#define SPEED_PERCEP_LINEAR
#define SPEED_EVERY_OTHER_FRAME

    // Update every other frame
    #ifdef SPEED_EVERY_OTHER_FRAME
    if (sys_time & 0x01) // Every other frame
    #endif
    {
        #ifdef SPEED_PERCEP_LINEAR
            // Non-linear speed ramp to give a more linear perceptual stretch result (non-squished was sliding in too slow)
            if (y_fx_dir > 0)
                y_line_inc += SPEED_LUT[(y_line_inc >> 5)];
            else
                y_line_inc -= SPEED_LUT[(y_line_inc >> 5)];
        #else
            // Use this for Linear adjustment, but non-linear effect perception
            y_line_inc += y_fx_dir;
        #endif

        // Reverse direction when needed, exit after first loop is completed
        if (y_line_inc <= FX_SQUISH_Y_INC_MIN) y_fx_dir = FX_DIR_INC;
        // else if (y_line_inc >=FX_SQUISH_Y_INC_MAX) y_fx_dir = FX_DIR_DEC;
        else if (y_line_inc >=FX_SQUISH_Y_INC_MAX) {
            y_fx_dir = FX_DIR_DEC;
            fx_continue = FALSE;
            //while(1);
        }
    }

}



// Hide all scanlines by setting them to show the designated "hidden" scanline value
void calc_hideall(uint8_t * p_buf) {
    int c;
    for (c = 0; c <= SCANLINE_MAX; c++) {
        // To display a given scanline at the current scanline by scrolling
        // the viewport on the y axis, the calc is:
        // scy_value_to_use = desired_line_y - current_line_y
        p_buf[c] = SCANLINE_HIDEVAL - c;
    }
}


// Show all scanlines with their normal value
void calc_showall(uint8_t * p_buf) {
    int c;
    for (c = 0; c <= SCANLINE_MAX; c++) {
        p_buf[c] = SCANLINE_SHOWALLVAL;
    }
}


// Pre-calculate the next frame to prime the buffer
void fx_vert_squish_prep(void) {

    uint8_t c;

    for (c=0;c < 144; c++) {
        p_buf_next_frame[c] = p_buf_cur_frame[c] = (y_line_calc >> FX_SQUISH_SHIFTAMT) - c;
        y_line_calc += y_line_inc;
    }
}


void fx_vert_squish(void) {

    fx_continue = true;
    // y_line_inc = FX_SQUISH_Y_INC_MIN;
    // y_fx_dir = FX_DIR_INC;
    y_line_inc = FX_SQUISH_Y_INC_MAX; // 255U + FX_DIR_DEC; // FX_SQUISH_Y_INC_MAX;
    y_fx_dir = FX_DIR_DEC;
    cur_buf = 0;

    // Make sure a couple values are initialized before the ISR starts
    y_line_calc = 0x0000U;
    y_map[1][0x90] = 0x00; // Dummy value for last hblank
    y_map[0][0x90] = 0x00;


    // Initialze pointers to double buffered Y scanline line maps
    y_map_buffers_init();
    fx_vert_squish_prep();

    // // prep the next frame to show entire background
    calc_showall(p_buf_cur_frame);
    calc_showall(p_buf_next_frame);
    // calc_hideall(p_buf_cur_frame);
    // calc_hideall(p_buf_next_frame);

    // Prep for first frame
//    vblank_isr_swap_and_first_scy_from_buf();

    // Enable ISR
    while (1) {
                UPDATE_KEYS();
        if (KEY_TICKED(J_A | J_B | J_START))
            break;

    }

    lcd_isr_register(vblank_isr_swap_and_first_scy_from_buf);


    // Mostly a idle loop while the VBlank and STAT HBlank ISR handle the updates
    while (fx_continue) {

        // Buffers get swapped by a second VBlank ISR
        wait_vbl_done();

        UPDATE_KEYS();
        if (KEY_TICKED(J_A | J_B | J_START))
            break;
    }

    lcd_isr_unregister(vblank_isr_swap_and_first_scy_from_buf);
}

