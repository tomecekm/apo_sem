/*******************************************************************
  Menu implementation for X-Mag application
 *******************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include "mzapo_parlcd.h"
#include "mzapo_regs.h"
#include "font_types.h"

#define LCD_WIDTH 480
#define LCD_HEIGHT 320

extern void draw_pixel(int x, int y, uint16_t color);
extern void clear_frame_buffer(uint16_t color);
extern void update_display(unsigned char *parlcd_mem_base);

// Funkce pro převod HSV na RGB pro LCD
unsigned int hsv2rgb_lcd(int hue, int saturation, int value) {
    hue = (hue % 360);    
    float f = ((hue % 60) / 60.0);
    int p = (value * (255 - saturation)) / 255;
    int q = (value * (255 - (saturation * f))) / 255;
    int t = (value * (255 - (saturation * (1.0 - f)))) / 255;
    unsigned int r, g, b;
    
    if (hue < 60) {
        r = value; g = t; b = p;
    } else if (hue < 120) {
        r = q; g = value; b = p;
    } else if (hue < 180) {
        r = p; g = value; b = t;
    } else if (hue < 240) {
        r = p; g = q; b = value;
    } else if (hue < 300) {
        r = t; g = p; b = value;
    } else {
        r = value; g = p; b = q;
    }
    
    r >>= 3;
    g >>= 2;
    b >>= 3;
    
    return (((r & 0x1f) << 11) | ((g & 0x3f) << 5) | (b & 0x1f));
}

// Funkce pro získání šířky znaku
int char_width(font_descriptor_t *fdes, int ch) {
    int width;
    if (!fdes->width) {
        width = fdes->maxwidth;
    } else {
        width = fdes->width[ch - fdes->firstchar];
    }
    return width;
}

// Funkce pro vykreslení znaku
void draw_char(int x, int y, char ch, unsigned short color, font_descriptor_t *fdes, int scale) {
    int w = char_width(fdes, ch);
    const font_bits_t *ptr;
    
    if ((ch >= fdes->firstchar) && (ch - fdes->firstchar < fdes->size)) {
        if (fdes->offset) {
            ptr = &fdes->bits[fdes->offset[ch - fdes->firstchar]];
        } else {
            int bw = (fdes->maxwidth + 15) / 16;
            ptr = &fdes->bits[(ch - fdes->firstchar) * bw * fdes->height];
        }
        
        for (int i = 0; i < fdes->height; i++) {
            font_bits_t val = *ptr;
            for (int j = 0; j < w; j++) {
                if ((val & 0x8000) != 0) {
                    // Vykreslení zvětšeného pixelu
                    for (int si = 0; si < scale; si++) {
                        for (int sj = 0; sj < scale; sj++) {
                            draw_pixel(x + scale * j + si, y + scale * i + sj, color);
                        }
                    }
                }
                val <<= 1;
            }
            ptr++;
        }
    }
}

// Funkce pro vykreslení textu
void draw_text(int x, int y, const char *text, unsigned short color, font_descriptor_t *fdes, int scale) {
    int cx = x;
    
    while (*text) {
        draw_char(cx, y, *text, color, fdes, scale);
        cx += char_width(fdes, *text) * scale;
        text++;
    }
}

// Funkce pro zobrazení menu
int show_menu(unsigned char *parlcd_mem_base, unsigned char *mem_base) {
    // Vyčistit frame buffer
    clear_frame_buffer(0x0000);
    
    // Nastavení fontů
    font_descriptor_t *title_font = &font_winFreeSystem14x16;
    font_descriptor_t *menu_font = &font_rom8x16;
    
    // Barvy
    unsigned short title_color = hsv2rgb_lcd(210, 255, 255); // Modrá
    unsigned short start_color = hsv2rgb_lcd(120, 255, 255); // Zelená
    unsigned short quit_color = hsv2rgb_lcd(0, 255, 255);    // Červená
    
    // Pozice textu - X-MAG více ve středu
    int title_x = 70 + (LCD_WIDTH - strlen("X-MAG") * title_font->maxwidth * 5) / 2;
    int title_y = 80;
    int start_x = (LCD_WIDTH - strlen("START") * menu_font->maxwidth * 3) / 2;
    int start_y = 180;
    int quit_x = (LCD_WIDTH - strlen("QUIT") * menu_font->maxwidth * 3) / 2;
    int quit_y = 230;
    
    // Hlavní smyčka menu
    while (1) {
        // Vyčistit frame buffer
        clear_frame_buffer(0x0000);
        
        // Vykreslit název - větší velikost (scale 5)
        draw_text(title_x, title_y, "X-MAG", title_color, title_font, 5);
        
        // Vykreslit položky menu - větší velikost (scale 3)
        draw_text(start_x, start_y, "START", start_color, menu_font, 3);
        draw_text(quit_x, quit_y, "QUIT", quit_color, menu_font, 3);
        
        // Aktualizovat displej
        update_display(parlcd_mem_base);
        
        // Čtení hodnot knobu a tlačítek
        uint32_t r = *(volatile uint32_t*)(mem_base + SPILED_REG_KNOBS_8BIT_o);
        
        // Kontrola tlačítek
        if (r & 0x2000000) { // Červené tlačítko - START
            return 1; // Pokračovat do hlavní aplikace
        }
        
        if (r & 0x4000000) { // Zelené tlačítko - QUIT
            return 0; // Ukončit aplikaci
        }

    }
    
    return 0;
}