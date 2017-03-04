//lets fake some stuff we are not building in gcc

#include <string.h>
#include <stdio.h>

void usleep(int a) {
	/* Do nothing!*/
}

char* strdupa(char* s) {
	if (s)
		fprintf(stderr,  "TODO:Leaking string '%s'\n",s);
	return _strdup(s);
	/* YES, this LEAKS memory, but these are small strings */
}

void* strsep(char* a, char* b) {
	/* Seems to only be used for vcd files.  For now, its disabled*/
	return NULL;
}
