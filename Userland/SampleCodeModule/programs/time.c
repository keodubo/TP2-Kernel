#include <sys_calls.h>
#include <userlib.h>

#define GMT_OFFSET 3

int getHours()
{
	return sys_getHours();
}

int getMinutes()
{
	return sys_getMinutes();
}

int getSeconds()
{
	return sys_getSeconds();
}

void getTime()
{
	int hours, minutes, seconds;

	hours = getHours();
	minutes = getMinutes();
	seconds = getSeconds();

	printc('\n');
	printDec(hours - GMT_OFFSET);
	printc(':');
	printDec(minutes);
	printc(':');
	printDec(seconds);
}
