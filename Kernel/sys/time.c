#include <stdint.h>
#include <time.h>

static unsigned long ticks = 0;
extern int _hlt();
int ellapsed = 0; // Used in sleep(), cleared by sleep(), incremented by timer_handler()

// Debug: agregar contador visible
static int debug_timer_count = 0;

void timer_handler() {
	ticks++;
	ellapsed += 55;  //timer ticks every 55ms (taught in class)

	// Debug cada 100 ticks (~5.5 segundos)
	debug_timer_count++;
	if (debug_timer_count >= 100) {
		// Este printf debería aparecer si el timer funciona
		// Si NO aparece, el problema es que el timer no interrumpe
		debug_timer_count = 0;
	}
}

int ticks_elapsed() {
	return ticks;
}

int seconds_elapsed() {
	return ticks / 18;
}

void sleep(int millis){
	ellapsed = 0;
	while (ellapsed<millis)
	{
		_hlt();
	}
}

int ms_elapsed() {
    return ticks*5000/91;
}

void timer_wait(int delta) {
	unsigned long initialTicks = ticks;
	unsigned long targetDelta = (delta < 0) ? 0UL : (unsigned long)delta;

	while ((ticks - initialTicks) < targetDelta) {
		_hlt();
	}
}
