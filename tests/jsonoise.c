#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>

#include <string.h>

unsigned s_max_depth = 0;

enum {
	/* Character is invalid . */
	CINV = 0x1,
	/* Character is whitespace. */
	CWHI = 0x2,
	/* Hexadecimal digit. */
	CHEX = 0x4,
	/* Escape character. */
	CESC = 0x8,

	/* Character begins list or map. */
	CBEG = 0x10,

	/* Character ends list or map. */
	CEND = 0x20
};

static const uint8_t s_lookup[256] = {
	CINV, CINV, CINV, CINV, CINV, CINV, CINV, CINV,
	CINV|CWHI, CINV|CWHI, CINV|CWHI, CINV|CWHI, CINV|CWHI, CINV, CINV, CINV,
	CINV, CINV, CINV, CINV, CINV, CINV, CINV, CINV,
	CINV|CWHI, CINV, CINV, CINV, CINV, CINV, CINV, CINV,
	CWHI, 0, CESC, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, CESC,
	CHEX, CHEX, CHEX, CHEX, CHEX, CHEX, CHEX, CHEX,
	CHEX, CHEX, 0, 0, 0, 0, 0, 0,
	0, CHEX, CHEX, CHEX, CHEX, CHEX, CHEX, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, CBEG, CESC, CEND, 0, 0,
	0, CHEX, CESC|CHEX, CHEX, CHEX, CHEX, CESC|CHEX, 0,
	0, 0, 0, 0, 0, 0, CESC, 0,
	0, 0, CESC, 0, CESC, 0, 0, 0,
	0, 0, 0, CBEG, 0, CEND, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	CINV, CINV, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, CINV, CINV, CINV,
	CINV, CINV, CINV, CINV, CINV, CINV, CINV, CINV
};

void print_randstr(unsigned minlen){
	unsigned len = (unsigned)(rand()) & 0xFF;
	if(len < minlen)
		len = minlen;

	unsigned count = 0;
	printf("\"");

	/* Print UTF-8 string or escape coded characters. */
	while(count < len){
		unsigned cpoint = (unsigned)(rand()) % 0x10FFFF;
		char out[5];

		/* Avoid second half of surrogate pair. */
		if(cpoint >= 0xD800 && cpoint <= 0xDFFF)
			continue;

		if(cpoint >= 0x10000){
			out[0] = 0xF0 | (cpoint >> 18);
			out[1] = 0x80 | ((cpoint >> 12) & 0x3F);
			out[2] = 0x80 | ((cpoint >> 6) & 0x3F);
			out[3] = 0x80 | (cpoint & 0x3F);
			out[4] = 0;
			count += 4;
		}
		else if(cpoint >= 0x800){
			out[0] = 0xE0 | (cpoint >> 12);
			out[1] = 0x80 | ((cpoint >> 6) & 0x3F);
			out[2] = 0x80 | (cpoint & 0x3F);
			out[3] = 0;
			count += 3;
		}
		else if(cpoint >= 0x80){
			out[0] = 0xC0 | (cpoint >> 6);
			out[1] = 0x80 | (cpoint & 0x3F);
			out[2] = 0;
			count += 2;
		}
		else{
			if(s_lookup[cpoint] & (CINV | CESC))
				continue;
			out[0] = cpoint & 0xFF;
			out[1] = 0;
			count += 1;
		}
		printf("%s", out);
	}
	printf("\"");
}

void print_val(unsigned depth){
	if(depth == s_max_depth)
		return;

	int r = (unsigned)(rand()) % s_max_depth;
	r -= depth;

	/* If r <= 0, print scalar value. */
	if(r <= 0){
		unsigned t = (unsigned)(rand()) % 3;
		switch(t){
			/* Print integer */
			case 0:
				printf("%i", rand());
				break;

			/* Print reserved */
			case 1:
				{
					static const char* s_reserved[] = {
						"false",
						"true",
						"null",
						"NaN",
						"Infinity",
						"-Infinity"
					};
					unsigned idx = (unsigned)(rand()) % 6;
					printf("%s", s_reserved[idx]);
				}
				break;

			/* Print float */
			case 2:
				{
					int n = rand();
					int d = rand();
					float f = ((float)n) / ((float)d);
					printf("%f", f);
				}
				break;

			/* Print string */
			case 3:
				print_randstr(0);
				break;

			default:
				break;
		}
	}
	else{
		unsigned i;
		printf((r & 1) ? "{" : "[");
		/* Skip first comma. */
		for(i = 0; i < r; ++i){
			if(i)
				printf(",");
			/* Print at least a single char for a key... */
			if(r & 1){
				print_randstr(1);
				printf(":");
			}
			print_val(depth + 1);
		}
		printf((r & 1) ? "}" : "]");
	}
}

int main(int argc, const char* argv[]){

	if(argc < 2){
		fprintf(stderr, "Usage %s MAXDEPTH\n", argv[0]);
		return 1;
	}

	/* Parse maximum depth. */
	char* endptr;
	s_max_depth = strtol(argv[1], &endptr, 10);

	if (endptr == argv[1]) {
		fprintf(stderr, "No digits were found\n");
		return 1;
	}

	/* Seed rng. */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);

	print_val(0);

	return 0;
}
