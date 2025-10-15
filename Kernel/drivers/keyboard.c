#include "keyboard.h"
#include "time.h"
#include <tty.h>
#include <stdint.h>

unsigned char notChar = 0;
static char retChar = 0;
static int shift = 0;
static int capsLock = 0;
static int ctrl = 0;

static const char keyMapL[] = {

    0,
    27,
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    '0',
    '-',
    '=',
    '\b',
    '\t',
    'q',
    'w',
    'e',
    'r',
    't',
    'y',
    'u',
    'i',
    'o',
    'p',
    '[',
    ']',
    '\n',
    0,
    'a',
    's',
    'd',
    'f',
    'g',
    'h',
    'j',
    'k',
    'l',
    ';',
    '\'',
    '`',
    0,
    '\\',
    'z',
    'x',
    'c',
    'v',
    'b',
    'n',
    'm',
    ',',
    '.',
    '/',
    0,
    '*',
    0,
    ' ',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    38,
    0,
    '-',
    37,
    0,
    39,
    '+',
    0,
    40,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

};

static const char keyMapU[] = {

    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0

};

static const char *const keyMap[] = {keyMapL, keyMapU};

/*
 * 29: left cntrl
 * 42: left shift
 * 54: right shift
 * 55: no tengo numpad
 * 56: left alt
 * 58: caps lock
 * 59 - 68: F1-F10
 * 69: numLock
 * 70: scrollLock
 *
 * Up: 0x48
 * Left: 0x4B
 * Right: 0x4D
 * Down: 0x50
 */

void keyboard_handler(uint8_t keyPressed)
{
    notChar = keyPressed;

    // shift pressed
    if (notChar == 0x2A || notChar == 0x36)
    {
        shift = 1;
    }
    // shift not pressed
    if (notChar == 0xAA || notChar == 0xB6)
    {
        shift = 0;
    }
    // capsLock
    if (notChar == 0x3A)
    {
        capsLock = (capsLock + 1) % 2;
    }
    // ctrl pressed
    if (notChar == 0x1D)
    {
        ctrl = 1;
    }
    // ctrl released
    if (notChar == 0x9D)
    {
        ctrl = 0;
    }

    char ascii = getCharFromKeyboard();
    if (ascii != 0)
    {
        tty_push_char(tty_default(), ascii);
    }
}

char getCharFromKeyboard()
{
    // soltar tecla
    if (notChar > 0x80 || notChar == 0x0F)
    {
        retChar = 0;
    }
    else if (notChar == 0x48 || notChar == 0x50)
    {
        retChar = notChar;
    }
    else
    {
        retChar = keyMap[shift][notChar];
    }

    if (ctrl && retChar != 0)
    {
        if (retChar >= 'a' && retChar <= 'z')
        {
            retChar = retChar - 'a' + 1;
        }
        else if (retChar >= 'A' && retChar <= 'Z')
        {
            retChar = retChar - 'A' + 1;
        }
    }

    return retChar;
}

void setCeroChar()
{
    notChar = 0;
}

unsigned char getNotChar()
{
    return notChar;
}
