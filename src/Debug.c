
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>


static	int		first_time = 1;


void DPrintf(char *format, ...)
{
	va_list	valist;
	char	string[256];
	FILE	*fp;

	va_start(valist, format);
	vsprintf(string, format, valist);
	va_end(valist);

	if (first_time)
		fp = fopen("c:\\sound_debug.txt", "wb");
	else
		fp = fopen("c:\\sound_debug.txt", "a");

	fprintf(fp, "%s", string);

	fclose(fp);

	first_time = 0;
}