#ifndef _VIDEO_DRIVER_H_
#define _VIDEO_DRIVER_H_

#include <stdint.h>

typedef struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
} Color;
extern Color WHITE;
extern Color BLACK;

void plusScale();
void minusScale();
uint16_t getRealCharWidth();
uint16_t getRealCharHeight();

void vDriver_prints(const char *str, Color fnt, Color bgd);
void vDriver_print(char c, Color fnt, Color bgd);
void vDriver_newline();
void vDriver_backspace(Color fnt, Color bgd);
void vDriver_clear();

void vDriver_printDec(uint64_t value, Color fnt, Color bgd);
void vDriver_printHex(uint64_t value, Color fnt, Color bgd);
void vDriver_printBin(uint64_t value, Color fnt, Color bgd);
void vDriver_printBase(uint64_t value, uint32_t base, Color fnt, Color bgd);

void vDriver_drawCursor();
void vDriver_drawRectangle(int x, int y, int x2, int y2, Color color);
void vDriver_setPixel(uint16_t x, uint16_t y, Color color);

/* Getters for screen */
uint16_t vDriver_getWidth(void);
uint16_t vDriver_getHeight(void);
uint32_t vDriver_getFrameBuffer(void);
uint8_t vDriver_getPixelWidth(void);
uint16_t vDriver_getPitch(void);

#endif