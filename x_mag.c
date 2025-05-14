/*******************************************************************
  X-Mag implementation for MicroZed based MZ_APO board
  Menu working, zoom working. 7.5.2025
 *******************************************************************/

#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include "mzapo_parlcd.h"
#include "mzapo_phys.h"
#include "mzapo_regs.h"
#include "serialize_lock.h"
#include "kote.c"
#include "font_types.h"
#include "menu.c"
#include "led.c"

#define LCD_WIDTH 480
#define LCD_HEIGHT 320
#define MAGNIFICATION 15

// Deklarace externích funkcí z menu.c a led.c
extern int show_menu(unsigned char *parlcd_mem_base, unsigned char *mem_base);
extern void animate_led_line(unsigned char *mem_base);
extern void update_led_magnification(unsigned char *mem_base, int mag_factor);

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
    // Zajistit minimální faktor zvětšení
    if (mag_factor < 2) mag_factor = 2;
    
    // Calculate the size of the area to magnify
    int mag_width = LCD_WIDTH / mag_factor;
    int mag_height = LCD_HEIGHT / mag_factor;

    // Calculate the starting point for sampling the source image
    int start_x = center_x - (mag_width / 2);
    int start_y = center_y - (mag_height / 2);

    // Ensure start positions are within bounds
    start_x = (start_x < 0) ? 0 : (start_x >= LCD_WIDTH - mag_width) ? LCD_WIDTH - mag_width : start_x;
    start_y = (start_y < 0) ? 0 : (start_y >= LCD_HEIGHT - mag_height) ? LCD_HEIGHT - mag_height : start_y;

    // Debug výpis pro kontrolu
    printf("Magnifying area: start_x=%d, start_y=%d, width=%d, height=%d, mag=%d\n", 
           start_x, start_y, mag_width, mag_height, mag_factor);
    
    // Draw magnified pixels
    for (int y = 0; y < mag_height; y++) {
        for (int x = 0; x < mag_width; x++) {
            int src_x = start_x + x;
            int src_y = start_y + y;
            
            // Kontrola hranic
            if (src_x < 0 || src_x >= LCD_WIDTH || src_y < 0 || src_y >= LCD_HEIGHT) {
                continue;
            }

            // Get color from source buffer
            uint16_t color = source_buffer[src_x + LCD_WIDTH * src_y];

            // Draw magnified pixel
            for (int dy = 0; dy < mag_factor; dy++) {
                for (int dx = 0; dx < mag_factor; dx++) {
                    int dest_x = x * mag_factor + dx;
                    int dest_y = y * mag_factor + dy;
                    draw_pixel(dest_x, dest_y, color);
                }
            }
        }
    }
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

    unsigned char *parlcd_mem_base = map_phys_address(PARLCD_REG_BASE_PHYS, PARLCD_REG_SIZE, 0);
    if (parlcd_mem_base == NULL) {
        printf("ERROR: Failed to map LCD peripheral\n");
        free(fb);
        free(source_buffer);
        return 1;
    }

    // Run LED animation before initializing LCD
    animate_led_line(mem_base);

    // Initialize the LCD
    parlcd_hx8357_init(parlcd_mem_base);

    // Show menu and get result
    int continue_app = show_menu(parlcd_mem_base, mem_base);
    
    // If user selected QUIT or pressed blue button, exit
    if (!continue_app) {
        printf("User selected to quit from menu\n");
        
        // Clear screen before exit
        clear_frame_buffer(0x0000);
        update_display(parlcd_mem_base);
        
        // Cleanup
        free(fb);
        free(source_buffer);
        serialize_unlock();
        
        return 0;
    }

    // Setup timing
    struct timespec loop_delay = {
        .tv_sec = 0,
        .tv_nsec = 150 * 1000 * 1000  // 150ms
    };

    printf("Starting main loop\n");

    // Main loop
    while (1) {
        // Read knob values directly from register
        uint32_t r = *(volatile uint32_t*)(mem_base + SPILED_REG_KNOBS_8BIT_o);

        // Check for blue button press (exit condition)
        if (r & 0x1000000) {
            printf("Blue button pressed - exiting\n");
            break;
        }

        // Extract knob positions
        int blue_val = 255 - (r & 0xff);                  // X position (blue knob)
        int green_val = (r >> 8) & 0xff;          // Y position (green knob)
        int red_val = (r >> 16) & 0xff;           // Magnification (red knob)

        // Debug print
        printf("Raw register value: 0x%08x\n", r);
        printf("Knob values - Blue: %d, Green: %d, Red: %d\n", blue_val, green_val, red_val);

        // Calculate positions and magnification
        int center_x = (blue_val * LCD_WIDTH) / 255;
        int center_y = (green_val * LCD_HEIGHT) / 255;
        int mag_factor = 2 + (red_val * (MAGNIFICATION - 2)) / 255;  // Maps 0-255 to 2-MAGNIFICATION

        // Update LED line based on magnification level
        update_led_magnification(mem_base, mag_factor);

        // Debug print calculated values
        printf("Calculated positions - X: %d, Y: %d, Mag: %d\n", center_x, center_y, mag_factor);

        // Clear frame buffer
        clear_frame_buffer(0x0000);

        // Draw magnified area
        draw_magnified_area(center_x, center_y, mag_factor);

        // Update display
        update_display(parlcd_mem_base);

        // Wait before next update
        clock_nanosleep(CLOCK_MONOTONIC, 0, &loop_delay, NULL);
    }

    printf("Exiting main loop\n");

    // Clear screen before exit
    clear_frame_buffer(0x0000);
    update_display(parlcd_mem_base);

    // Cleanup
    free(fb);
    free(source_buffer);
    serialize_unlock();

    return 0;
}
