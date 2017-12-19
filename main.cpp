#include "swfparser.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	if(argc <= 1) {
		printf("No file specified.");
		return 1;
	}
	
	FILE *swffile = fopen(argv[1], "r");
	if(swffile == NULL) {
		printf("File could not be opened.");
		return 2;
	}
	fseek(swffile, 0, SEEK_END);
	uint32_t datalen = ftell(swffile);
	rewind(swffile);
	uint8_t *data = (uint8_t*)malloc((datalen+1)*sizeof(uint8_t));
	fread(data, datalen, 1, swffile);

	SWF::Parser parser;
	parser.parse_swf_data(data, datalen);
	
	fclose(swffile);
	
	printf("Done.");
	return 0;
}
