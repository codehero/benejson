
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>

#include <benejson/benejson.h>

static unsigned s_last_keylen;
static unsigned s_last_strlen;

int usercb(const bnj_state* state, bnj_ctx* ctx, const uint8_t* buff){
	return 0;
}

int main(int argc, const char* argv[]){
	uint32_t stackbuff[128];
	bnj_val values[16];
	bnj_state mstate;
	bnj_ctx ctx = {
		.user_cb = usercb,
		.user_data = NULL,
		.key_set = NULL,
		.key_set_length = 0,
	};

	if(argc < 2){
		fprintf(stderr, "Please specify fragment amount!\n");
		return 1;
	}

	bnj_state_init(&mstate, stackbuff, 128);
	mstate.v = values;
	mstate.vlen = 16;

	char* endptr;
	unsigned buffsize = strtol(argv[1], &endptr, 10);

	unsigned long offset = 0;
	s_last_strlen = 0;
	s_last_keylen = 0;
	while(1){
		uint8_t buff[buffsize];
		int ret = read(0, buff, buffsize);

		if(ret == 0)
			return 0;

		if(ret < 0)
			return 1;

		const uint8_t* res = bnj_parse(&mstate, &ctx, buff, ret);
		offset += res - buff;
		if(mstate.flags & BNJ_ERROR_MASK){
			printf("Stopped at %lx, char=%hhx, err=%x\n", offset, *res, mstate.flags);
			printf("Error\n");
			return 1;
		}
	}

	if(!(mstate.flags & BNJ_ERROR_MASK))
		usercb(&mstate, &ctx, NULL);

	return 0;
}
