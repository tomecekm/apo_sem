/*******************************************************************
  X-Mag implementation for MicroZed based MZ_APO board
 *******************************************************************/

#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "mzapo_parlcd.h"
#include "mzapo_phys.h"
#include "mzapo_regs.h"
#include "serialize_lock.h"

#define LCD_WIDTH 480
#define LCD_HEIGHT 320
#define MAGNIFICATION 2


unsigned short *fb;

// Function to draw a pixel to frame buffer
void draw_pixel(int x, int y, uint16_t color) {
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
        fb[x + LCD_WIDTH * y] = color;
    }
}

// Function to update the entire display from frame buffer
void update_display(unsigned char *parlcd_mem_base) {
    parlcd_write_cmd(parlcd_mem_base, 0x2c);
    for (int ptr = 0; ptr < LCD_WIDTH * LCD_HEIGHT; ptr++) {
        parlcd_write_data(parlcd_mem_base, fb[ptr]);
    }
}

// Function to clear the frame buffer
void clear_frame_buffer(uint16_t color) {
    for (int ptr = 0; ptr < LCD_WIDTH * LCD_HEIGHT; ptr++) {
        fb[ptr] = color;
    }
}

// Function to draw magnified area
void draw_magnified_area(int center_x, int center_y) {
    int mag_width = LCD_WIDTH / MAGNIFICATION;
    int mag_height = LCD_HEIGHT / MAGNIFICATION;
    int start_x = center_x - (mag_width / 2);
    int start_y = center_y - (mag_height / 2);

    // Draw magnified pixels
    for (int y = 0; y < mag_height; y++) {
        for (int x = 0; x < mag_width; x++) {
            int src_x = start_x + x;
            int src_y = start_y + y;
            
            // Example color pattern (you can modify this)
            uint16_t color = ((src_x & 0x1F) << 11) | ((src_y & 0x3F) << 5) | (0x1F);
            
            // Draw magnified pixel
            for (int dy = 0; dy < MAGNIFICATION; dy++) {
                for (int dx = 0; dx < MAGNIFICATION; dx++) {
                    int dest_x = x * MAGNIFICATION + dx;
                    int dest_y = y * MAGNIFICATION + dy;
                    draw_pixel(dest_x, dest_y, color);
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    /* Serialize execution of applications */
    if (serialize_lock(1) <= 0) {
        printf("System is occupied\n");
        if (1) {
            printf("Waiting\n");
            serialize_lock(0);
        }
    }

    // Allocate frame buffer
    fb = (unsigned short *)malloc(LCD_HEIGHT * LCD_WIDTH * sizeof(unsigned short));
    if (fb == NULL) {
        printf("ERROR: Failed to allocate frame buffer\n");
        return 1;
    }

    // Map the LED peripheral
    unsigned char *led_mem_base = map_phys_address(SPILED_REG_BASE_PHYS, SPILED_REG_SIZE, 0);
    if (led_mem_base == NULL) {
        printf("ERROR: Failed to map LED peripheral\n");
        free(fb);
        return 1;
    }

    // Map the LCD peripheral
    unsigned char *parlcd_mem_base = map_phys_address(PARLCD_REG_BASE_PHYS, PARLCD_REG_SIZE, 0);
    if (parlcd_mem_base == NULL) {
        printf("ERROR: Failed to map LCD peripheral\n");
        free(fb);
        return 1;
    }

    // Initialize the LCD
    parlcd_hx8357_init(parlcd_mem_base);

    // Clear screen to black
    clear_frame_buffer(0x0000);
    update_display(parlcd_mem_base);

    // Setup timing
    struct timespec loop_delay = {
        .tv_sec = 0,
        .tv_nsec = 150 * 1000 * 1000  // 150ms
    };

    // Initial position
    int center_x = LCD_WIDTH / 2;
    int center_y = LCD_HEIGHT / 2;

    // Main loop
    while (1) {
        // Read knob values
        int r = *(volatile uint32_t*)(led_mem_base + SPILED_REG_KNOBS_8BIT_o);
        
        // Check for exit condition (blue button)
        if ((r & 0x7000000) != 0) {
            break;
        }
        
        // Update position based on knobs
        center_x = ((r & 0xff) * LCD_WIDTH) / 256;
        center_y = (((r >> 8) & 0xff) * LCD_HEIGHT) / 256;
        
        // Clear frame buffer
        clear_frame_buffer(0x0000);
        
        // Draw magnified area
        draw_magnified_area(center_x, center_y);
        
        // Update display
        update_display(parlcd_mem_base);
        
        // Wait for next frame
        clock_nanosleep(CLOCK_MONOTONIC, 0, &loop_delay, NULL);
    }

    // Clear screen before exit
    clear_frame_buffer(0x0000);
    update_display(parlcd_mem_base);

    // Cleanup
    free(fb);
    serialize_unlock();

    return 0;
}