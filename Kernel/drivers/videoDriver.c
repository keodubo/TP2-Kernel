#include "videoDriver.h"
#include "font.h"
#include "time.h"

Color WHITE = {255,255,255};
Color BLACK = {0,0,0};
const uint16_t WIDTH_FONT = 9;
const uint16_t HEIGHT_FONT = 16;
uint16_t cursorOn = 0;

struct vbe_mode_info_structure {
    uint16_t attributes;
    uint8_t window_a;
    uint8_t window_b;
    uint16_t granularity;
    uint16_t window_size;
    uint16_t segment_a;
    uint16_t segment_b;
    uint32_t win_func_ptr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t w_char;
    uint8_t y_char;
    uint8_t planes;
    uint8_t bpp;                
    uint8_t banks;
    uint8_t memory_model;   
    uint8_t bank_size;          
    uint8_t image_pages;
    uint8_t reserved0;

    uint8_t red_mask;
    uint8_t red_position;
    uint8_t green_mask;
    uint8_t green_position;
    uint8_t blue_mask;
    uint8_t blue_position;
    uint8_t reserved_mask;
    uint8_t reserved_position;
    uint8_t direct_color_attributes;

    uint32_t framebuffer;           
    uint32_t off_screen_mem_off;
    uint16_t off_screen_mem_size;   
    uint8_t reserved1[206];
} __attribute__ ((packed));         
struct vbe_mode_info_structure* screenInfo = (void*)0x5C00;


/* draws a char in screen in the given coordinates, then sets the coordinates where the next char should be drawn */
static void drawChar (int x, int y, unsigned char c,Color fntColor, Color bgColor);
/* Moves everything up one line */
static void scrollUp ();
/* get a number in dec from another base */
static uint32_t uintToBase(uint64_t value, char * buffer, uint32_t base);
/* Gets the pixel pointer in screen from the (x,y) coordinate*/
static uint32_t* getPixelPtr(uint16_t x, uint16_t y);

// Establecer un factor de escala predeterminado
uint8_t pixelScale = 1;

// Aumentar el factor de escala para aumentar el tamaño de un carácter
void plusScale() {
    if (pixelScale < 5) {
        pixelScale++;
    }
}

// Disminuir el factor de escala para reducir el tamaño de un carácter
void minusScale() {
    if (pixelScale > 1) {
        pixelScale--;
    }
}

// Obtener el ancho real de un carácter según el factor de escala actual
uint16_t getRealCharWidth() {
    return WIDTH_FONT * pixelScale;
}

// Obtener el alto real de un carácter según el factor de escala actual
uint16_t getRealCharHeight() {
    return HEIGHT_FONT * pixelScale;
}



//////////////////////////////////////////////////////////////////////////////////
/* screen coordinates where the next char will be written */
uint16_t cursorX = 0;
uint16_t cursorY = 0;

static char buffer[64] = { '0' };


void vDriver_prints(const char *str, Color fnt, Color bgd){
    for (int i = 0 ; str[i] != '\0'; i++ ){
        vDriver_print(str[i], fnt, bgd);
    }
}

void vDriver_print(const char c, Color fnt, Color bgd){
    switch (c) {
        case '\n':
            vDriver_newline();
        break;
        case '\b':
            vDriver_backspace(fnt, bgd);
        break;
        case '\0':
            /* nada, no imprime nada */
        break;
        default:
            drawChar(cursorX, cursorY , c , fnt , bgd);
        break;
    }
}

void vDriver_newline(){
    cursorX = 0;
    cursorY += HEIGHT_FONT* pixelScale;

    if (cursorY + HEIGHT_FONT*pixelScale > screenInfo->height){
        cursorY -= HEIGHT_FONT*pixelScale;
        scrollUp();
    }
}

void vDriver_backspace(Color fnt, Color bgd){
    if (cursorX >= WIDTH_FONT*pixelScale){
        cursorX -= WIDTH_FONT*pixelScale;
    } else {
        cursorX = 0;
    }
    drawChar(cursorX, cursorY , ' ' , fnt , bgd);
    cursorX -= WIDTH_FONT*pixelScale;
}

void vDriver_drawCursor(){
    int cx, cy;
    Color fntColor = cursorOn ? BLACK : WHITE;
    Color bgColor = cursorOn ? BLACK : WHITE;
    //mascara de bits para saber que color imprimo a pantalla, si pertenece a caracter o a fondo
    int mask[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

    const unsigned char *glyph;
    if (cursorOn){
        glyph = font_bitmap + 16 * (' ' - 32);
        cursorOn = 0;
    } else {
        glyph = font_bitmap + 16 * (' ' - 32);
        cursorOn = 1;
    }

// Chequeo que no sea el final de línea, ni el final de la pantalla
    if (cursorX >= screenInfo->width) {
        cursorX = 0;
        if (cursorY + getRealCharHeight() > screenInfo->height) {
            cursorY -= getRealCharHeight();
            scrollUp();
        } else {
            cursorY += getRealCharHeight();
        }
    }

    for (cy = 0; cy < 16; cy++) {
        for (cx = 0; cx < 8; cx++) {
            // Uso el factor de escala
            for (int i = 0; i < pixelScale; i++) {
                for (int j = 0; j < pixelScale; j++) {
                    vDriver_setPixel(cursorX + (8 - cx) * pixelScale + i, cursorY + cy * pixelScale + j, glyph[cy] & mask[cx] ? fntColor : bgColor);
                }
            }
        }
    }

}

void vDriver_clear() {
    Color color = BLACK;
    Color* pixel = (Color*) ((uint64_t)screenInfo->framebuffer);

    //recorro todos los pixeles de la pantalla y los pongo del color que quiero
    for (uint32_t len = (uint32_t)screenInfo->width * screenInfo->height; len; len--, pixel++){
        *pixel = color;
    }

    //seteo los cursores en el inicio (arriba a la izquierda)
    cursorX = 0;
    cursorY = 0;
}

static void drawChar(int x, int y, unsigned char c, Color fntColor, Color bgColor) {
    (void)x;
    (void)y;
    //con estos indices recorro el caracter
    int cx, cy;
    //mascara de bits para saber que color imprimo a pantalla, si pertenece a caracter o a fondo
    int mask[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
    //puntero a donde inician los datos de mi caracter, hago (c-32 pues los caracteres imprimibles arrancan en el 32), lo multiplico por 16 pues es lo que ocupa
    //un caracter en el bitmap
    const unsigned char *glyph = font_bitmap + 16 * (c - 32);

    // Chequeo que no sea el final de línea, ni el final de la pantalla
    if (cursorX >= screenInfo->width) {
        cursorX = 0;
        if (cursorY + getRealCharHeight() > screenInfo->height) {
            cursorY -= getRealCharHeight();
            scrollUp();
        } else {
            cursorY += getRealCharHeight();
        }
    }

    for (cy = 0; cy < 16; cy++) {
        for (cx = 0; cx < 8; cx++) {
            // Uso el factor de escala
            for (int i = 0; i < pixelScale; i++) {
                for (int j = 0; j < pixelScale; j++) {
                    vDriver_setPixel(cursorX + (8 - cx) * pixelScale + i, cursorY + cy * pixelScale + j, glyph[cy] & mask[cx] ? fntColor : bgColor);
                }
            }
        }
    }

    cursorX += getRealCharWidth();
}



static void scrollUp (){
    Color* pixel, *next;
    for (int i = 0 ; i < cursorY + HEIGHT_FONT*pixelScale ; i++){
        for (int j = 0 ; j < screenInfo->width ; j++){
            pixel = (Color *) getPixelPtr(j,i);
            next = (Color *) getPixelPtr(j,i+HEIGHT_FONT*pixelScale);
            *pixel = *next;
        }
    }
}


void vDriver_printDec(uint64_t value, Color fnt, Color bgd){
    vDriver_printBase(value, 10,fnt,bgd);
}

void vDriver_printHex(uint64_t value, Color fnt, Color bgd){
    vDriver_printBase(value, 16,fnt,bgd);
}

void vDriver_printBin(uint64_t value, Color fnt, Color bgd){
    vDriver_printBase(value, 2,fnt,bgd);
}

void vDriver_printBase(uint64_t value, uint32_t base, Color fnt, Color bgd){
    uintToBase(value, buffer, base);
    for (int i = 0 ; buffer[i] != '\0' ; i++ ){
        vDriver_print(buffer[i], fnt, bgd);
    }
}


static uint32_t uintToBase(uint64_t value, char * buffer, uint32_t base){
    char *p = buffer;
    char *p1, *p2;
    uint32_t digits = 0;

    //Calculate characters for each digit
    do{
        uint32_t remainder = value % base;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'A' - 10;
        digits++;
    }while (value /= base);

    // Terminate string in buffer.
    *p = 0;

    //Reverse string in buffer.
    p1 = buffer;
    p2 = p - 1;
    while (p1 < p2){
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
    return digits;
}




//te paso una coordenada de la pantalla en (x,y) y te devuelvo la direccion de la pantalla que representa ese pixel
static uint32_t* getPixelPtr(uint16_t x, uint16_t y) {
    uint8_t pixelwidth = screenInfo->bpp/8;     //la cantidad de bytes hasta el siguiente pixel a la derecha (bpp: BITS per px)
    uint16_t pixelHeight = screenInfo->pitch;   //la cantidad de bytes hasta el pixel hacia abajo

    uintptr_t pixelPtr = (uintptr_t)(screenInfo->framebuffer) + (x * pixelwidth) + (y * pixelHeight);
    return (uint32_t*)pixelPtr;
}



//pinto un pixel de la pantalla con el color que quiero, 
void vDriver_setPixel(uint16_t x, uint16_t y, Color color) {
    if (x >= screenInfo->width || y >= screenInfo->height)
        return;

    Color* pixel = (Color*) getPixelPtr(x, y);
    *pixel = color;
}


//dibujo un cuadrado
void vDriver_drawRectangle(int x, int y, int w, int h, Color color){
    Color * pixel;

    for (int i = 0 ; i < h ; i++){
        pixel = (Color*) getPixelPtr(x,y+i);
        for (int j = 0 ; j < w ; j++, pixel++){
            *pixel = color;
        }
    }
}


uint16_t vDriver_getWidth(void) {
    return screenInfo->width;
}

uint16_t vDriver_getHeight(void) {
    return screenInfo->height;
}

uint32_t vDriver_getFrameBuffer(void) {
    return screenInfo->framebuffer;
}

uint8_t vDriver_getPixelWidth(void){
    return screenInfo->bpp;
}

uint16_t vDriver_getPitch(void){
    return screenInfo->pitch;
}
