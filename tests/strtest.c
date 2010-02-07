#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include <string.h>

#include <benejson/benejson.h>

struct good_test {
	const char* json;
	uint32_t cp[64];
	uint8_t  utf8[64];
	unsigned cp1_count;
	unsigned cp2_count;
	unsigned cp3_count;
	unsigned cp4_count;
};

unsigned s_compare_good(unsigned i, const struct good_test* gt,
	const bnj_val* v, const char* p)
{
	uint8_t buffer[1024];
	unsigned retval = 0;
	/* Compare code page values. */
	if(gt->cp1_count != v->cp1_count){
		fprintf(stdout, p, i, 1, gt->cp1_count, v->cp1_count);
		retval = 1;
	}
	if(gt->cp2_count != v->cp2_count){
		fprintf(stdout, p, i, 2, gt->cp2_count, v->cp2_count);
		retval = 1;
	}
	if(gt->cp3_count != v->cp3_count){
		fprintf(stdout, p, i, 3, gt->cp3_count, v->cp3_count);
		retval = 1;
	}
	if(gt->cp4_count != v->exp_val){
		fprintf(stdout, p, i, 4, gt->cp4_count, v->exp_val);
		retval = 1;
	}

	/* Verify UTF-8 conversion. */
	uint8_t* res = bnj_stpcpy8(buffer, v, (uint8_t*)gt->json);
	size_t diff = res - buffer;
	unsigned expect = strlen(gt->utf8);
	if(diff != expect){
		fprintf(stdout, "UTF-8 length failure: %u expected %u, got %u\n",
			i, (unsigned)diff, expect);
		retval = 1;
	}
	else{
		/* Bitwise compare. */
		if(memcmp(buffer, gt->utf8, expect)){
			fprintf(stdout, "UTF-8 content failure: %u, len %u\n", i, expect);
			for(unsigned k = 0; k < expect; ++k)
				fprintf(stdout, "%2x %2x\n", buffer[k], gt->utf8[k]);

			retval = 1;
		}
	}
	return retval;
}

static const char* s_bad_values[] = {
	/* Tests 0-7 */
	/* Invalid 2 byte overlong encodings. */
	"\"abc \xC0\x80 abc\"", //0
	"\"abc \xC1\xBF abc\"", //127

	/* Invalid 3 byte overlong encodings. */
	"\"abc \xE0\x80\x80 abc\"", //0
	"\"abc \xE0\x9F\xBF abc\"", //2047

	/* Invalid 4 byte overlong encodings. */
	"\"abc \xF0\x80\x80\x80 abc\"", //0
	"\"abc \xF0\x8F\x8F\xBF abc\"", //65535

	/* Invalid bytes 0xFE,0xFF */
	"\"abc \xFE abc\"",
	"\"abc \xFF abc\"",

	/* Tests 8-13 */
	/* Invalid 4,5,6 byte encodings. */
	"\"abc \xF5\x82\x80\x80\x80 abc\"",
	"\"abc \xF7\x82\x80\x80\x80 abc\"",
	"\"abc \xF8\x82\x80\x80\x80\x80 abc\"",
	"\"abc \xFB\x82\x80\x80\x80\x80 abc\"",
	"\"abc \xFC\x82\x80\x80\x80\x80\x80 abc\"",
	"\"abc \xFD\x82\x80\x80\x80\x80\x80 abc\"",

	/* Tests 14-19 */
	/* Bad continuation bytes. */
	"\"abc \xC8\xCF abc\"",

	"\"abc \xE0\xCF\xBF abc\"",
	"\"abc \xE0\xAF\xCF abc\"",

	"\"abc \xF0\xCF\x9F\x8F abc\"",
	"\"abc \xF0\x9F\xCF\x8F abc\"",
	"\"abc \xF0\x9F\x8F\xCF abc\"",

	/* Tests 20-25 */
	/* Insufficient number of continuation bytes. */
	"\"abc \xC8 abc\"",

	"\"abc \xE0 abc\"",
	"\"abc \xE0\xAF abc\"",

	"\"abc \xF0 abc\"",
	"\"abc \xF0\x9F abc\"",
	"\"abc \xF0\x9F\x8F abc\"",

	/* Tests 26-33 */
	/* Invalid escape encodings. */
	"\"abc \\u abc\"",
	"\"abc \\u0 abc\"",
	"\"abc \\u00 abc\"",
	"\"abc \\u000 abc\"",
	"\"abc \\u0000\\u abc\"",
	"\"abc \\u0000\\u0 abc\"",
	"\"abc \\u0000\\u00 abc\"",
	"\"abc \\u0000\\u000 abc\"",

	/* Tests 34-37 */
	/* First valid surrogate, then invalid. */
	"\"abc \\uD800\\uDBFF abc\"",
	"\"abc \\uD800\\uE000 abc\"",
	"\"abc \\uDBFF\\uDBFF abc\"",
	"\"abc \\uDBFF\\uE000 abc\"",

	/* Tests 38-41 */
	/* Incorrectly place second surrogate. */
	"\"abc \\uD7FF\\uDC00 abc\"",
	"\"abc \\uE000\\uDC00 abc\"",
	"\"abc \\uDC00\\uDC00 abc\"",
	"\"abc \\uDFFF\\uDC00 abc\"",

	/* Start with continuation char.*/
	"\"abc \x8F abc\"",
};

static unsigned s_bad_stop[]= {
	/* Invalid 2 byte overlong encodings. */
	4 + 1,
	4 + 1,

	/* Invalid 3 byte overlong encodings. */
	4 + 3,
	4 + 3,

	/* Invalid 4 byte overlong encodings. */
	4 + 4,
	4 + 4,

	/* Invalid bytes 0xFE,0xFF */
	4 + 1,
	4 + 1,

	/* Invalid 4,5,6 byte encodings. */
	4 + 1,
	4 + 1,
	4 + 1,
	4 + 1,
	4 + 1,
	4 + 1,

	/* Bad continuation bytes. */
	4 + 2,

	4 + 2,
	4 + 3,

	4 + 2,
	4 + 3,
	4 + 4,

	/* Insufficient number of continuation bytes. */
	4 + 2,

	4 + 2,
	4 + 3,

	4 + 2,
	4 + 3,
	4 + 4,

	/* Invalid escape encodings. */
	4 + 3,
	4 + 4,
	4 + 5,
	4 + 6,
	4 + 9,
	4 + 10,
	4 + 11,
	4 + 12,

	/* First valid surrogate, then invalid. */
	4 + 10,
	4 + 9,
	4 + 10,
	4 + 9,

	/* Second surrogate first. */
	4 + 12,
	4 + 12,
	4 + 6,
	4 + 6,

	/* Start with continuation char.*/
	4 + 1,
};

struct good_test s_good[] = {
	{
		"\"abc \xC2\x80 abc\"",
		{'a','b','c',' ',0x80,' ','a','b','c'},
		{'a','b','c',' ',0xC2, 0x80,' ','a','b','c'},
		8,
		1,
		0,
		0,
	},

	{
		"\"abc \xC8\xBF abc\"",
		{'a','b','c',' ',0x23F,' ','a','b','c'},
		{'a','b','c',' ',0xC8, 0xBF, ' ','a','b','c'},
		8,
		1,
		0,
		0,
	},

	{
		"\"abc \xDF\xBF abc\"",
		{'a','b','c',' ',0x7FF,' ','a','b','c'},
		{'a','b','c',' ',0xDF, 0xBF, ' ','a','b','c'},
		8,
		1,
		0,
		0,
	},

	{
		"\"abc \xE0\xA0\x80 abc\"",
		{'a','b','c',' ', 0x0800,' ','a','b','c'},
		{'a','b','c',' ', 0xE0, 0xA0, 0x80,' ','a','b','c'},
		8,
		0,
		1,
		0,
	},

	{
		"\"abc \xE3\xBF\xBF abc\"",
		{'a','b','c',' ',0x3FFF,' ','a','b','c'},
		{'a','b','c',' ',0xE3, 0xBF, 0xBF, ' ','a','b','c'},
		8,
		0,
		1,
		0,
	},

	{
		"\"abc \xEF\xBF\xBF abc\"",
		{'a','b','c',' ', 0xFFFF,' ','a','b','c'},
		{'a','b','c',' ', 0xEF, 0xBF, 0xBF,' ','a','b','c'},
		8,
		0,
		1,
		0,
	},

	{
		"\"abc \xC8\xBF\xE3\xBF\xBF abc\"",
		{'a','b','c',' ',0x23F,0x3FFF,' ','a','b','c'},
		{'a','b','c',' ',0xC8, 0xBF, 0xE3, 0xBF, 0xBF,' ','a','b','c'},
		8,
		1,
		1,
		0,
	},

	{
		"\"abc \\u3111\xE3\xBF\xBF abc\"",
		{'a','b','c',' ',0x3111,0x3FFF,' ','a','b','c'},
		{'a','b','c',' ', 0xE3, 0x84, 0x91, 0xE3, 0xBF, 0xBF, ' ','a','b','c'},
		8,
		0,
		2,
		0,
	},

	{
		"\"abc \\u3111\xF4\x8F\xBF\xBF abc\"",
		{'a','b','c',' ', 0x3111, 0x10FFFF,' ','a','b','c'},
		{'a','b','c',' ', 0xE3, 0x84, 0x91, 0xF4, 0x8f, 0xBF, 0xBF,' ','a','b','c'},
		8,
		0,
		1,
		1,
	},

	{
		"\"abc \\uD800\\uDC00\\uDBFF\\uDFFF abc\"",
		{'a','b','c',' ',0x10000, 0x10FFFF,' ','a','b','c'},
		{'a','b','c',' ',0xF0, 0x90, 0x80, 0x80, 0xF4, 0x8F, 0xBF, 0xBF, ' ','a','b','c'},
		8,
		0,
		0,
		2,
	},

	{
		"\"abc \\u3111\xF4\x8F\xBF\xBF\\uD800\\uDC00\\uDBFF\\uDFFF abc\"",
		{'a','b','c',' ', 0x3111, 0x10FFFF,0x10000, 0x10FFFF,' ','a','b','c'},
		{'a','b','c',' ', 0xE3, 0x84, 0x91, 0xF4, 0x8f, 0xBF, 0xBF, 0xF0, 0x90, 0x80, 0x80, 0xF4, 0x8F, 0xBF, 0xBF, ' ','a','b','c'},
		8,
		0,
		1,
		3,
	},

};


int main(int argc, const char* argv[]){
	uint32_t stackbuff[128];
	bnj_val values[16];
	bnj_state mstate;
	bnj_ctx ctx = {
		.user_cb = NULL,
		.key_set = NULL,
		.key_set_length = 0,
	};
	unsigned i;
	const unsigned bad_length = sizeof(s_bad_values) / sizeof(const char*);
	const unsigned good_length = sizeof(s_good) / sizeof(struct good_test);
	unsigned succeeded, failed;

	assert(sizeof(s_bad_stop) / sizeof(unsigned) == bad_length);

	succeeded = 0;
	failed = 0;
	for(i = 0; i < bad_length; ++i){
		bnj_state_init(&mstate, stackbuff, 128);
		mstate.v = values;
		mstate.vlen = 16;

		mstate.user_ctx = &ctx;

		const uint8_t* res =
			bnj_parse(&mstate, (uint8_t*)s_bad_values[i], strlen(s_bad_values[i]));

		/* Error should be detected. Verify correct detection. */
		if(mstate.flags & BNJ_ERROR_MASK){
			size_t diff = res - (uint8_t*)s_bad_values[i];
			if(s_bad_stop[i] != diff){
				fprintf(stdout,
					"Bad Test %u, Incorrect detection at %u should be %u, raw char was %hx\n",
					i, (unsigned)diff, s_bad_stop[i], *res);
				++failed;
			}
			else{
				++succeeded;
			}
		}
		else{
			fprintf(stdout, "Bad Test %u, Failed to detect error\n", i);
			++failed;
		}
	}
	fprintf(stdout, "Bad Tests total: %u, succeeded: %u, failed %u\n",
		bad_length, succeeded, failed);

	/* Fragmenting test. */
	succeeded = 0;
	failed = 0;
	for(i = 0; i < bad_length; ++i){
		unsigned maxlen = strlen(s_bad_values[i]);
		bnj_state_init(&mstate, stackbuff, 128);
		mstate.v = values;
		mstate.vlen = 16;

		mstate.user_ctx = &ctx;

		for(unsigned x = 0; x < maxlen; ++x){
			const uint8_t* res =
				bnj_parse(&mstate, (uint8_t*)s_bad_values[i] + x, 1);

			/* Error should be detected. Verify correct detection. */
			if(mstate.flags & BNJ_ERROR_MASK){
				size_t diff = res - (uint8_t*)s_bad_values[i];
				if(s_bad_stop[i] != diff){
					fprintf(stdout,
						"Bad Frag Test %u, Incorrect detection at %u should be %u, raw char was %hx\n",
						i, (unsigned)diff, s_bad_stop[i], *res);
					++failed;
				}
				else{
					++succeeded;
				}
				break;
			}
			else if(x == maxlen - 1){
				fprintf(stdout, "Bad Frag Test %u, Failed to detect error\n", i);
				++failed;
				break;
			}
		}
	}

	fprintf(stdout, "Bad Frag Tests total: %u, succeeded: %u, failed %u\n",
		bad_length, succeeded, failed);

	/* Good test. */
	const char cp_str[] =
		"Good Test %u, Mismatched cp%u count expected %u, got %u\n";
	const char cp_frag_str[] =
		"Good Frag Test %u, Mismatched cp%u count expected %u, got %u\n";
	succeeded = 0;
	failed = 0;
	for(i = 0; i < good_length; ++i){
		bnj_state_init(&mstate, stackbuff, 128);
		mstate.v = values;
		mstate.vlen = 16;

		mstate.user_ctx = &ctx;

		const uint8_t* res =
			bnj_parse(&mstate, (uint8_t*)s_good[i].json, strlen(s_good[i].json));

		/* Error should be detected. Verify correct detection. */
		if(mstate.flags & BNJ_ERROR_MASK){
			size_t diff = res - (uint8_t*)s_good[i].json;
			fprintf(stdout, "Good Test %u, Detected error %x at %u\n", i,
				mstate.flags, (unsigned) diff);
			++failed;
		}
		else{
			if(s_compare_good(i, s_good + i, values, cp_str))
				++failed;
			else
				++succeeded;
		}
	}

	fprintf(stdout, "Good Tests total: %u, succeeded: %u, failed %u\n",
		good_length, succeeded, failed);

	/* Fragmenting good test. */
	succeeded = 0;
	failed = 0;
	for(i = 0; i < good_length; ++i){
		unsigned maxlen = strlen(s_good[i].json);
		unsigned expect = strlen(s_good[i].utf8);
		bnj_state_init(&mstate, stackbuff, 128);
		mstate.v = values;
		mstate.vlen = 16;

		mstate.user_ctx = &ctx;

		unsigned cp1_count = 0;
		unsigned cp2_count = 0;
		unsigned cp3_count = 0;
		unsigned cp4_count = 0;

		/* Will copy string as we get it into this buffer. */
		uint8_t cpbuffer[64];
		uint8_t* w = cpbuffer;

		for(unsigned q = 0; q < 64; ++q)
			cpbuffer[q] = 0;

		/* First do one by one. */
		for(unsigned x = 0; x < maxlen; ++x){
			const uint8_t* res =
				bnj_parse(&mstate, (uint8_t*)s_good[i].json + x, 1);
			size_t diff = res - (uint8_t*)s_good[i].json;

			/* Error should be detected. Verify correct detection. */
			if(mstate.flags & BNJ_ERROR_MASK){
				fprintf(stdout, "Good Frag Test %u, Detected error %x at %u\n", i,
					mstate.flags, (unsigned) diff);
				++failed;
				break;
			}
			else if(x == maxlen - 1){
				if(memcmp(cpbuffer, s_good[i].utf8, expect)){
					unsigned k;
					fprintf(stdout, "UTF-8 content failure: %u, len %u\n", i, expect);
					for(k = 0; k < expect; ++k)
						fprintf(stdout, "%2x %2x\n", cpbuffer[k], s_good[i].utf8[k]);
					while(k < w - cpbuffer){
						fprintf(stdout, "%2x\n", cpbuffer[k]);
						++k;
					}
					++failed;
				}
				else{
					++succeeded;
				}
				break;
			}

			cp1_count += values->cp1_count;
			cp2_count += values->cp2_count;
			cp3_count += values->cp3_count;
			cp4_count += values->exp_val;
			
			w = bnj_stpcpy8(w, values, (uint8_t*)s_good[i].json + x);
		}
	}

	fprintf(stdout, "Good Frag Tests total: %u, succeeded: %u, failed %u\n",
		good_length, succeeded, failed);

	return 0;
}
