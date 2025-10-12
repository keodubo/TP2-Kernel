#include <stdint.h>
#include <string.h>
#include "lib.h"
#include <moduleLoader.h>
#include <naiveConsole.h>
#include "videoDriver.h"
#include "keyboard.h"
#include "idtLoader.h"
#include "time.h"
#include "interrupts.h"
#include "memory_manager.h"
#include "process.h"



extern uint8_t text;
extern uint8_t rodata;
extern uint8_t data;
extern uint8_t bss;
extern uint8_t endOfKernelBinary;
extern uint8_t endOfKernel;

extern void _hlt();

static const uint64_t PageSize = 0x1000;

static void * const sampleCodeModuleAddress = (void*)0x400000;
static void * const sampleDataModuleAddress = (void*)0x500000;

typedef int (*EntryPoint)();

static void userland_bootstrap(int argc, char** argv);


void clearBSS(void * bssAddress, uint64_t bssSize)
{
	memset(bssAddress, 0, bssSize);
}

void * getStackBase()
{
	return (void*)(
		(uint64_t)&endOfKernel
		+ PageSize * 8				//The size of the stack itself, 32KiB
		- sizeof(uint64_t)			//Begin at the top of the stack
	);
}

void * initializeKernelBinary()
{
	void * moduleAddresses[] = {
		sampleCodeModuleAddress,
		sampleDataModuleAddress
	};

	loadModules(&endOfKernelBinary, moduleAddresses);

	clearBSS(&bss, &endOfKernel - &bss);
	return getStackBase();
}


int main()
{	
	load_idt();

	// Inicializar el memory manager
	// El heap empieza despues del stack (32KB despues de endOfKernel)
	void* heap_start = (void*)((uint64_t)&endOfKernel + PageSize * 8);
	size_t heap_size = 1024 * 1024; // 1MB de heap
	mm_init(heap_start, heap_size);

	process_init();

	setCeroChar();

	process_create(userland_bootstrap, 0, NULL, "shell", DEFAULT_PRIORITY, 1);

	process_start();

    while(1) _hlt();
    return 0;
}

static void userland_bootstrap(int argc, char** argv) {
	(void)argc;
	(void)argv;

	((EntryPoint)sampleCodeModuleAddress)();

	process_exit_current();
}

