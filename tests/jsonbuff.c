
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>

#include <benejson/benejson.h>

/* The most important variables
 *
 * -depth_change
 * -vi
 * -depth
 * -stack[depth - depth_change]
 *
 * */

static unsigned s_width = 80;
static unsigned curout = 0;

int usercb(const bnj_state* state, const uint8_t* buff){
	//printf("Call depth : %d,%d;", state->depth, state->depth_change);

	//printf("Len %u;", state->digit_count);
	//printf("Dec %d;", state->_decimal_offset);
	//printf("Cnt %d;", state->vi);
	//printf("\n");

	if(buff){
		unsigned i;
		for(i = 0; i < state->vi; ++i){
			if(state->v[i].key_length){
				unsigned limit = state->v[i].key_offset;
				while(curout < limit - 1){
					printf(" ");
					++curout;
				}
				printf("K");
				++curout;

				limit += state->v[i].key_length;
				while(curout < limit){
					printf("-");
					++curout;
				}
				printf("k");
				++curout;
			}

			switch(state->v[i].type & BNJ_TYPE_MASK){
				case BNJ_UTF_8:
					{
						unsigned limit = state->v[i].strval_offset;
						while(curout < limit - 1){
							printf(" ");
							++curout;
						}
						printf("S");
						++curout;

						limit += state->v[i].exp_val;
						while(curout < limit){
							printf("-");
							++curout;
						}
						printf("s");
						++curout;
					}
					break;

				case BNJ_UTF_16:
				case BNJ_UTF_32:
					break;

				case BNJ_NUMERIC:
					break;

				case BNJ_SPECIAL:
					break;

				default:
					break;
			}
		}
	}

	/* Check for empty */
	return 0;
}

int main(int argc, const char* argv[]){
	uint32_t stackbuff[128];
	bnj_val values[16];
	bnj_state mstate;
	bnj_ctx ctx = {
		.user_cb = usercb,
	};

	bnj_state_init(&mstate, stackbuff, 128);
	mstate.v = values;
	mstate.vlen = 16;
	mstate.user_ctx = &ctx;

	uint8_t *buff = malloc(s_width + 1);
	while(1){
		curout = 0;
		/* Read data from stdin. */
		int ret = read(0, buff, s_width);
		if(ret == 0)
			return 0;
		if(ret < 0)
			return 1;

		const uint8_t* res = bnj_parse(&mstate, buff, ret);
		if(mstate.flags & BNJ_ERROR_MASK){
			printf("Error\n");
			return 1;
		}

		/* Replace \n with ' ' */
		for(unsigned k = 0; k < s_width; ++k)
			if(buff[k] == '\n')
				buff[k] = ' ';

		/* Null terminate and print what was read(). */
		buff[ret] = '\0';
		printf("\n%s\n", buff);
	}

	if(!(mstate.flags & BNJ_ERROR_MASK))
		usercb(&mstate, NULL);

	free(buff);
	return 0;
}
