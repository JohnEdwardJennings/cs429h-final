#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define BUFSIZE 4096

int main() {
	size_t bufsize = BUFSIZE;
	char* linebuf = malloc(BUFSIZE);
	bool preactive = false;
	bool active = false;
	while (true) {
		ssize_t length = getline(&linebuf, &bufsize, stdin);
		if (length < 0) {
			printf("Error reading testing output (in concise.c)");
			free(linebuf);
			exit(-1);
		}
		if (active) {
			if (!strcmp("\n", linebuf)) {
				free(linebuf);
				return 0;
			}
			printf("%s", linebuf);
		}
		if (preactive) {
			active = true;	
		}
		if (!strcmp("Region of Interest Statistics\n", linebuf)) {
			preactive = true;	
		}
	}
	free(linebuf);
	return -1;
}
