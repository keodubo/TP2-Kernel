#include <stdint.h>
#include "videoDriver.h"
#include "time.h"



#define ZERO_ID 0
#define INVAL_OPCODE_ID 6

extern void reset();
extern int _hlt(void);

Color red = {30,30,255};
Color white = {255,255,255};
Color black = {0,0,0};


const char* registers[18] = {
    "RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "RBP", "RSP", "R8 ", "R9 ", "R10", "R11", "R12", "R13", "R14", "R15", "RIP", "RFLAGS "
};

static void uint64ToHex(uint64_t n, char buf[16]) {
    int i=15;
    do {
        int digit = n % 16;
        buf[i] = (digit < 10 ? '0' : ('A' - 10)) + digit;
        n /= 16;
    } while(i-- != 0);
}


void exception_handler(int exception, const uint64_t register_data[17]) {
	vDriver_prints("\nERROR: ",red,black);
	char hexbuf[19];
    hexbuf[0] = '0';
    hexbuf[1] = 'x';
    hexbuf[18] = '\0';

	if (exception == ZERO_ID){
		vDriver_prints("zero division detected\n",white, black);
	} else if (exception == INVAL_OPCODE_ID){
		vDriver_prints("invalid op code detected\n", white, black);
	}

	for (int i = 0; i < 16; i++) {
        vDriver_prints(registers[i],white,black);
        vDriver_prints(": ",white,black);
        uint64ToHex(register_data[i], hexbuf+2);
        vDriver_prints(hexbuf,white,black);
        if (i % 4 == 3)
            vDriver_newline();
        else
            vDriver_prints("   ",white,black);
    }

    reset();
}
