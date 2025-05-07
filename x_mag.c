/*******************************************************************
  X-Mag implementation  MZ_APO board
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
#include "kote.c"

#define LCD_WIDTH 480
#define LCD_HEIGHT 320
#define MAGNIFICATION 15

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
    if (source_buffer == NULL) {
        printf("ERROR: Failed to allocate source buffer\n");
        return;
    }

    // Clear source buffer first with black color
    for (int ptr = 0; ptr < LCD_WIDTH * LCD_HEIGHT; ptr++) {
        source_buffer[ptr] = 0x0000;
    }

    // Calculate center position to place image
    int start_x = (LCD_WIDTH - kote_png_width) / 2;
    int start_y = (LCD_HEIGHT - kote_png_height) / 2;

    // Copy image data to center of source buffer
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
void draw_magnified_area(int center_x, int center_y, int mag_factor) {
    // Calculate viewing window size
    int window_width = LCD_WIDTH / mag_factor;
    int window_height = LCD_HEIGHT / mag_factor;
    
    // Calculate start position
    int start_x = center_x - (window_width / 2);
    int start_y = center_y - (window_height / 2);
    
    // Adjust start positions to prevent going beyond screen edges
    if (start_x < 0) {
        start_x = 0;
        center_x = window_width / 2;
    }
    if (start_y < 0) {
        start_y = 0;
        center_y = window_height / 2;
    }
    if (start_x + window_width > LCD_WIDTH) {
        start_x = LCD_WIDTH - window_width;
        center_x = LCD_WIDTH - window_width / 2;
    }
    if (start_y + window_height > LCD_HEIGHT) {
        start_y = LCD_HEIGHT - window_height;
        center_y = LCD_HEIGHT - window_height / 2;
    }

    // Draw magnified content
    for (int y = 0; y < window_height; y++) {
        for (int x = 0; x < window_width; x++) {
            // Get source pixel
            uint16_t color = source_buffer[(start_y + y) * LCD_WIDTH + (start_x + x)];
            
            // Calculate destination coordinates
            int dest_x_start = x * mag_factor;
            int dest_y_start = y * mag_factor;
            
            // Draw magnified pixel
            for (int my = 0; my < mag_factor && dest_y_start + my < LCD_HEIGHT; my++) {
                for (int mx = 0; mx < mag_factor && dest_x_start + mx < LCD_WIDTH; mx++) {
                    draw_pixel(dest_x_start + mx, dest_y_start + my, color);
                }
            }
        }
    }
}

// Function to animate LED line
void animate_led_line(unsigned char *mem_base) {
    uint32_t val_line = 1;
    int direction = 1; // 1 = left to right, -1 = right to left
    
    printf("Starting LED line animation\n");
    
    // Animate LED line back and forth
    for (int i = 0; i < 3; i++) { // Do 3 complete cycles
        // Move from left to right
        val_line = 1;
        while (val_line < 0x80000000) {
            *(volatile uint32_t*)(mem_base + SPILED_REG_LED_LINE_o) = val_line;
            val_line <<= 1;
            usleep(50000); // 50ms delay between shifts
        }
        
        // Move from right to left
        while (val_line > 1) {
            val_line >>= 1;
            *(volatile uint32_t*)(mem_base + SPILED_REG_LED_LINE_o) = val_line;
            usleep(50000); // 50ms delay between shifts
        }
    }
    
    printf("LED animation complete\n");
}

int main(int argc, char *argv[]) {
    printf("Starting X-Mag application\n");

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
    printf("Frame buffer allocated\n");

    // Load image into source buffer
    load_image_to_buffer();

    // Map the peripherals
    unsigned char *mem_base = map_phys_address(SPILED_REG_BASE_PHYS, SPILED_REG_SIZE, 0);
    if (mem_base == NULL) {
        printf("ERROR: Failed to map LED peripheral\n");
        free(fb);
        free(source_buffer);
        return 1;
    }

    // Run LED line animation at startup
    animate_led_line(mem_base);

    unsigned char *parlcd_mem_base = map_phys_address(PARLCD_REG_BASE_PHYS, PARLCD_REG_SIZE, 0);
    if (parlcd_mem_base == NULL) {
        printf("ERROR: Failed to map LCD peripheral\n");
        free(fb);
        free(source_buffer);
        return 1;
    }

    // Initialize the LCD
    parlcd_hx8357_init(parlcd_mem_base);

    // Setup timing
    struct timespec loop_delay = {
        .tv_sec = 0,
        .tv_nsec = 150 * 1000 * 1000  // 150ms
    };

    printf("Starting main loop\n");

    // Main loop
    while (1) {
        uint32_t knobs = *(volatile uint32_t*)(mem_base + SPILED_REG_KNOBS_8BIT_o);
        
        if (knobs & 0x1000000) {
            break;  // Exit on blue button press
        }
        // Get knob values (0-255)
        int x_val = knobs & 0xff;
        int y_val = (knobs >> 8) & 0xff;
        int zoom_val = (knobs >> 16) & 0xff;
		printf("Knobs: X=%d, Y=%d, Zoom=%d\n", x_val, y_val, zoom_val);

        // Calculate magnification (1-15)
        int mag_factor = 1 + (zoom_val * (MAGNIFICATION - 1)) / 255;
        
        // Calculate center position with bounds checking
        int window_width = LCD_WIDTH / mag_factor;
        int window_height = LCD_HEIGHT / mag_factor;
        
        // Map knob values to screen coordinates with boundary limits
        int center_x = (x_val * (LCD_WIDTH - window_width)) / 255 + window_width / 2;
        int center_y = (y_val * (LCD_HEIGHT - window_height)) / 255 + window_height / 2;

        // Clear frame buffer
        clear_frame_buffer(0x0000);

        // Draw magnified area
        draw_magnified_area(center_x, center_y, mag_factor);

        // Update display
        parlcd_write_cmd(parlcd_mem_base, 0x2c);
        for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
            parlcd_write_data(parlcd_mem_base, fb[i]);
        }

        clock_nanosleep(CLOCK_MONOTONIC, 0, &loop_delay, NULL);
    }

    printf("Exiting main loop\n");

    // Clear screen before exit
    clear_frame_buffer(0x0000);
    parlcd_write_cmd(parlcd_mem_base, 0x2c);
    for (int ptr = 0; ptr < LCD_WIDTH * LCD_HEIGHT; ptr++) {
        parlcd_write_data(parlcd_mem_base, 0);
    }

    // Cleanup
    free(fb);
    free(source_buffer);
    serialize_unlock();

    return 0;
}