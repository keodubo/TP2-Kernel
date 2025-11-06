#include <sys_calls.h>
#include <stdint.h>
#include <userlib.h>
#include <sh.h>

int main() {
	welcome();

	sh_loop();

	return 0;
} 
