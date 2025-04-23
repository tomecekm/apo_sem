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
#include "kote.c"  // Include the image data

#define LCD_WIDTH 480
#define LCD_HEIGHT 320
#define MAGNIFICATION 2

// Global frame buffer
unsigned short *fb;
// Source image buffer
unsigned short *source_buffer;

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

// Function to load the image into source buffer
void load_image_to_buffer() {
    source_buffer = (unsigned short *)malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(unsigned short));

    // Clear source buffer first
    for (int ptr = 0; ptr < LCD_WIDTH * LCD_HEIGHT; ptr++) {
        source_buffer[ptr] = 0;
    }

    // Copy image data to center of source buffer
    int start_x = (LCD_WIDTH - kote_png_width) / 2;
    int start_y = (LCD_HEIGHT - kote_png_height) / 2;

    for (int y = 0; y < kote_png_height; y++) {
        for (int x = 0; x < kote_png_width; x++) {
            int dest_x = start_x + x;
            int dest_y = start_y + y;
            if (dest_x >= 0 && dest_x < LCD_WIDTH && dest_y >= 0 && dest_y < LCD_HEIGHT) {
                source_buffer[dest_x + LCD_WIDTH * dest_y] = kote_png[x + y * kote_png_width];
            }
        }
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

            // Get color from source buffer (with bounds checking)
            uint16_t color = 0;
            if (src_x >= 0 && src_x < LCD_WIDTH && src_y >= 0 && src_y < LCD_HEIGHT) {
                color = source_buffer[src_x + LCD_WIDTH * src_y];
            }

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

    // Load image into source buffer
    load_image_to_buffer();

    // Map the LED peripheral
    unsigned char *led_mem_base = map_phys_address(SPILED_REG_BASE_PHYS, SPILED_REG_SIZE, 0);
    if (led_mem_base == NULL) {
        printf("ERROR: Failed to map LED peripheral\n");
        free(fb);
        free(source_buffer);
        return 1;
    }

    // Map the LCD peripheral
    unsigned char *parlcd_mem_base = map_phys_address(PARLCD_REG_BASE_PHYS, PARLCD_REG_SIZE, 0);
    if (parlcd_mem_base == NULL) {
        printf("ERROR: Failed to map LCD peripheral\n");
        free(fb);
        free(source_buffer);
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
    free(source_buffer);
    serialize_unlock();

    return 0;
}