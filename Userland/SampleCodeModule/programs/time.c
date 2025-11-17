#include <sys_calls.h>
#include <userlib.h>

#define GMT_OFFSET 3  // Offset para ajustar zona horaria

// Obtiene las horas del RTC (Real Time Clock)
int getHours()
{
	return sys_getHours();
}

// Obtiene los minutos del RTC
int getMinutes()
{
	return sys_getMinutes();
}

// Obtiene los segundos del RTC
int getSeconds()
{
	return sys_getSeconds();
}

// Imprime la hora actual en formato HH:MM:SS
// Ajusta la hora restando GMT_OFFSET para la zona horaria local
void getTime()
{
	int hours, minutes, seconds;

	hours = getHours();
	minutes = getMinutes();
	seconds = getSeconds();

	printc('\n');
	printDec(hours - GMT_OFFSET);  // Ajustar por zona horaria
	printc(':');
	printDec(minutes);
	printc(':');
	printDec(seconds);
}
