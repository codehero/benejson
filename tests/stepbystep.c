/* Copyright (c) 2010 David Bender assigned to Benegon Enterprises LLC
 * See the file LICENSE for full license information. */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>

#include <benejson/benejson.h>

int main(int argc, const char* argv[]){
	uint32_t stackbuff[128];
	bnj_val value;
	bnj_state mstate;
	bnj_ctx ctx = {
		.user_cb = NULL,
		.key_set = NULL,
		.key_set_length = 0,
	};
	unsigned go = 1;
	int retval = 0;
	unsigned line_count = 0;

	bnj_state_init(&mstate, stackbuff, 128);
	mstate.v = &value;
	mstate.vlen = 1;

	mstate.user_ctx = &ctx;

	while(go){
		uint8_t buff[2048];
		int ret = read(0, buff, 2048);

		/* Read EOF before parsing a json. */
		if(ret == 0)
			return 3;

		if(ret < 0)
			return 2;

		printf("[\n");
		const uint8_t* end = buff + ret;
		const uint8_t* cur = buff;
		while(cur != end){
			const uint8_t* res = bnj_parse(&mstate, cur, 1);
			/* Print out state */
			printf("{\"b\":\"0x%02x\", \"dc\":%3i, \"vi\":%3u, \"d\":%4u, \"dig\":%2u, "
				"\"f\":\"0x%08x\", \"doff\":%2d, \"sv\":%20u, "
				"\"se\":%6u, \"st\":\"0x%02x\", \"kl\":%3u,"
				"\"dv\":\"0x%08x\",\"c\":%u},\n",
				*cur, mstate.depth_change, mstate.vi, mstate.depth, mstate.digit_count,
				mstate.flags, mstate._decimal_offset, mstate._paf_significand_val,
				mstate._paf_exp_val, mstate._paf_type, mstate._key_len,
				mstate.stack[mstate.depth], res - cur);
			++line_count;

			/* Assert things that should not happen char by char. */
			assert(mstate.vi < 2);

			if(mstate.flags & BNJ_ERROR_MASK){
				go = 0;
				retval = 1;
				break;
			}
			if(BNJ_SUCCESS == mstate.flags){
				go = 0;
				break;
			}
			cur = res;
		}
		printf("null]");
	}

	return retval;
}
