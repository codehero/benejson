
#include <stdio.h>
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

static unsigned s_last_keylen;
static unsigned s_last_strlen;

int usercb(const bnj_state* state, const uint8_t* buff){
	static char buffer[1024];
	unsigned st = state->stack[state->depth - state->depth_change];
	printf("Call depth : %d,%d,%x;", state->depth, state->depth_change, state->stack[state->depth]);

	printf("Len %u;", state->digit_count);
	printf("Dec %d;", state->_decimal_offset);
	printf("Cnt %d;", state->vi);
	printf("\n >>> ");


	if(buff){
		unsigned i;

		for(i = 0; i < state->vi; ++i){

			if(st & BNJ_OBJECT){
				bnj_stpkeycpy(buffer, state->v + i, buff);
				printf("  Key %s;", buffer);

				if(state->user_ctx->key_set){
					printf("Key enum %d;", state->v[i].key_enum);
				}
			}

			printf("Type:%x;", state->v[i].type);
			if(!(state->v[i].type & BNJ_VFLAG_KEY_FRAGMENT)){
				switch(state->v[i].type & BNJ_TYPE_MASK){
					case BNJ_STRING:
						bnj_stpcpy8(buffer, state->v + i, buff);
						printf("Val %s;", buffer);
						break;

					case BNJ_NUMERIC:
						printf("Sig %lu;", state->v[i].significand_val);
						printf("Exp %d;", state->v[i].exp_val);
						printf("Float %.10G;", bnj_float(state->v + i));
						printf("Double %.20lG;", bnj_double(state->v + i));
						break;

					case BNJ_SPECIAL:
						printf("special;");
						break;

					default:
						break;
				}
			}
			printf("\n >>> ");
		}

		if(i < 16 && state->v[i].type & BNJ_VFLAG_VAL_FRAGMENT){
			printf("FRAGMENT!!!\n");
		}

	}
	printf("\n");

	/* Check for empty */
	return 0;
}


const char* keys[] = {
 "aba",
 "abbcc",
 "abc",
 "abcq",
 "abczef",
 "def",
 "ghi",
 "xyz"
};

int main(int argc, const char* argv[]){
	uint32_t stackbuff[128];
	bnj_val values[16];
	bnj_state mstate;
	bnj_ctx ctx = {
		.user_cb = usercb,
		.key_set = keys,
		.key_set_length = 8,
	};

	if(argc < 2){
		fprintf(stderr, "Please specify fragment amount!\n");
		return 1;
	}

	bnj_state_init(&mstate, stackbuff, 128);
	mstate.v = values;
	mstate.vlen = 16;

	mstate.user_ctx = &ctx;

	char* endptr;
	unsigned buffsize = strtol(argv[1], &endptr, 10);

	s_last_strlen = 0;
	s_last_keylen = 0;
	while(1){
		uint8_t buff[buffsize];
		int ret = read(0, buff, buffsize);
		printf("read %d bytes\n", ret);

		if(ret == 0)
			return 0;

		if(ret < 0)
			return 1;

		const uint8_t* res = bnj_parse(&mstate, buff, ret);
		printf("Stopped at %lu, char=%c\n", res - buff, *res);
		if(mstate.flags & BNJ_ERROR_MASK){
			printf("Error\n");
			return 1;
		}
	}

	if(!(mstate.flags & BNJ_ERROR_MASK))
		usercb(&mstate, NULL);

	return 0;
}
