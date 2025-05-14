/*******************************************************************
  LED line animation for X-Mag application
 *******************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "mzapo_regs.h"

// Function to animate LED line
void animate_led_line(unsigned char *mem_base) {
    uint32_t val_line = 1;
    
    printf("Starting LED line animation\n");
    
    // Animate LED line from right to left
    printf("LED moving left\n");
    for (int i = 0; i <= 30; i++) {
        *(volatile uint32_t*)(mem_base + SPILED_REG_LED_LINE_o) = val_line;
        val_line <<= 1;
        usleep(100000); // 100ms delay
        
        // Reset when we reach the end
        if (val_line == 0) {
            val_line = 0x80000000; // Nejvyšší bit pro zpáteční cestu
        }
    }
    
    // Animate LED line from left to right
    printf("LED moving right\n");
    for (int i = 0; i <= 30; i++) {
        *(volatile uint32_t*)(mem_base + SPILED_REG_LED_LINE_o) = val_line;
        val_line >>= 1;
        usleep(100000); // 100ms delay
    }
    
    // Clear LED line at the end
    *(volatile uint32_t*)(mem_base + SPILED_REG_LED_LINE_o) = 0;
    printf("LED animation complete\n");
}

// Function to update LED line based on magnification level
void update_led_magnification(unsigned char *mem_base, int mag_factor) {
    // Ensure mag_factor is within range 2-14
    if (mag_factor < 2) mag_factor = 2;
    if (mag_factor > 14) mag_factor = 14;
    
    // Calculate how many LEDs to light up (0-30)
    // Map magnification range 2-14 to LED range 0-30
    int leds_to_light = (mag_factor - 2) * 31 / 12;
    
    // Create bit pattern with appropriate number of 1s from left
    uint32_t val_line = 0;
    for (int i = 0; i <= leds_to_light; i++) {
        val_line |= (1 << i);
    }
    
    // Update LED line register
    *(volatile uint32_t*)(mem_base + SPILED_REG_LED_LINE_o) = val_line;

}
