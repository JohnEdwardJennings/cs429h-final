#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define BUFSIZE 1024

int main() {
	size_t bufsize = BUFSIZE;
	char* linebuf = malloc(BUFSIZE);
	bool preactive = false;
	bool active = true;
	while (true) {
		ssize_t length = getline(&linebuf, &bufsize, stdin);
		if (length < 0) {
			printf("Error reading testing output (in concise.c)");
			free(linebuf);
			exit(-1);
		}
		if (active && !strcmp("", linebuf)) {
			free(linebuf);
			return 0;		
		}
		if (!strcmp("Region of Interest Statistics", linebuf)) {
			preactive = true;	
		}
		if (preactive && !strcmp("", linebuf)) {
			active = true;
		}
		if (active) {
			printf("%s\n", linebuf);
		}
	}
	free(linebuf);
	return -1;
}
