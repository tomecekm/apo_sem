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
    
    // Animate LED line from left to right
    printf("LED moving right\n");
    for (int i = 0; i <= 30; i++) {
        *(volatile uint32_t*)(mem_base + SPILED_REG_LED_LINE_o) = val_line;
        val_line <<= 1;
        usleep(100000); // 100ms delay
        
        // Reset when we reach the end
        if (val_line == 0) {
            val_line = 0x80000000; // Nejvyšší bit pro zpáteční cestu
        }
    }
    
    // Animate LED line from right to left
    printf("LED moving left\n");
    for (int i = 0; i <= 30; i++) {
        *(volatile uint32_t*)(mem_base + SPILED_REG_LED_LINE_o) = val_line;
        val_line >>= 1;
        usleep(100000); // 100ms delay
    }
    
    // Clear LED line at the end
    *(volatile uint32_t*)(mem_base + SPILED_REG_LED_LINE_o) = 0;
    printf("LED animation complete\n");
}