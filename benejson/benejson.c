/* Copyright (c) 2010 David Bender assigned to Benegon Enterprises LLC
 * See the file LICENSE for full license information. */

//#include <stdio.h>
#include <assert.h>
#include "benejson.h"

#define TOP3 ((SIGNIFICAND)(0x7) << (sizeof(SIGNIFICAND) * 8 - 3))
#define SETSTATE(x,s) x = s

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

enum {
	/** @brief OK to read comma, used in stack context. */
	BNJ_EXPECT_COMMA = 0x8
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

/*  */
enum {
	BNJ_VALUE_START = 1,
	BNJ_INTERSTITIAL,
	BNJ_INTERSTITIAL2,
	BNJ_INTERSTITIAL3,
	BNJ_STRING_ST,

	BNJ_SIGNIFICAND,
	BNJ_EXP_SIGN,
	BNJ_EXPONENT,
	BNJ_RESERVED,
	BNJ_VALUE_END,

	BNJ_STR_ESC,
	BNJ_STR_U0,
	BNJ_STR_U1,
	BNJ_STR_U2,
	BNJ_STR_U3,
	BNJ_STR_SURROGATE,
	BNJ_STR_SURROGATE_1,
	BNJ_STR_SURROGATE_2,
	BNJ_STR_SURROGATE_3,

	BNJ_STR_UTF3,
	BNJ_STR_UTF2,
	BNJ_STR_UTF1,

	BNJ_END_VALUE,
	BNJ_END_VALUE2,
	BNJ_COLON,
	BNJ_MINUS,
	BNJ_OTHER_VALS,
	BNJ_STRING_START,

	BNJ_COUNT_STATES
};

/* Determine rest of string. */
static const char* s_reserved_arr[BNJ_COUNT_SPC] = {
	"alse",
	"rue",
	"ull",
	"aN",
	"nfinity"
};

static uint8_t s_hex(uint8_t c){
	return (c <= '9') ? c - '0' : (c & 0xDF) -'7';
}

static inline void s_reset_state(bnj_state* state){
	state->depth_change = 0;
	state->vi = 0;
	state->v[0].type = 0;
	state->v[0].key_length = 0;
	state->v[0].key_enum = 0;
}

static inline void s_match_key(bnj_state* state, uint8_t target){
	const char* const *const key_set = state->user_ctx->key_set;
	uint16_t* key_enum = &(state->v[state->vi].key_enum);
	const uint16_t* key_length = &(state->_key_len);
	/* Adjust minimum if necessary. */
	if(target !=
			key_set[*key_enum][*key_length])
	{
		/* Binary search for least index with matching char. */
		unsigned idx_high = state->_key_set_sup;
		do{
			unsigned mid = (*key_enum + idx_high) >> 1;
			if(target > key_set[mid][*key_length]){
				/* Move up idx */
				*key_enum = mid + 1;
			}
			else{ 
				/* If satisfies max condition, update max. */
				if(target < key_set[mid][*key_length])
					state->_key_set_sup = mid;
				idx_high = mid;
			}
		} while(idx_high != *key_enum);

		/* If no match, then error. */
		if(state->_key_set_sup == *key_enum)
			return;
	}

	/* Adjust maximum if necessary. */
	if(target != key_set[state->_key_set_sup - 1][*key_length]){
		/* May as well decrement supremum, since the if() just failed. */
		--state->_key_set_sup;

		/* Binary search for greatest index with matching char. */
		unsigned max_match = *key_enum;
		do{
			unsigned mid = (state->_key_set_sup + max_match) >> 1;
			/* If mid > target, lower supremum. Otherwise raise lower bound. */
			if(target < key_set[mid][*key_length])
				state->_key_set_sup = mid;
			else
				max_match = mid;
		} while(max_match != (state->_key_set_sup - 1));
	}
}

bnj_state* bnj_state_init(bnj_state* ret, uint32_t* stack, uint32_t stack_length){
	uint8_t* i = (uint8_t*) ret;
	uint8_t* end = i + sizeof(bnj_state);
	/* Zero the memory. */
	while(i != end)
		*(i++) = 0;

	/* Set stack length. */
	ret->stack_length = stack_length;
	ret->stack = stack;
	stack[0] = 0;

	/* Initialize first state. */
	ret->flags = BNJ_VALUE_START;
	ret->_cp_fragment = BNJ_EMPTY_CP;

	return ret;
}

const uint8_t* bnj_parse(bnj_state* state, const uint8_t* buffer, uint32_t len){
	const uint8_t* i = buffer;
	const uint8_t * const end = buffer + len;

	/* The purpose of first_cp_frag is to track the placement of string fragments
	 * with respect to the buffer. The buffer can have two possible fragments:
	 * -one at the front
	 * -one at the back
	 * If there is no fragment at the front, _cp_fragment will be EMPTY.
	 * Then there is no fragment to remember at the beginning, and
	 * curval (state->v really) if it is a string, will not have to remember a 
	 * fragment in significand_val.
	 *
	 * If _cp_fragment is not EMPTY, then when parsing of the string is complete
	 * that curval (state->v) will have the fragment stored at its beginning.
	 * first_cp_frag will then be cleared. The _cp_fragment will contain the
	 * end fragment.*/
	uint32_t first_cp_frag = state->_cp_fragment;

	bnj_val* curval = state->v;
	s_reset_state(state);

	/* PAF restore. Only restore exp_val if NOT a string type. */
	curval->key_enum = state->_paf_key_enum;
	curval->type = state->_paf_type;
	curval->exp_val = (bnj_val_type(curval) != BNJ_STRING)
		? state->_paf_exp_val : 0;
	curval->cp1_count = 0;
	curval->cp2_count = 0;
	curval->cp3_count = 0;

	/* Copy significand value directly if code point fragment value is not
	 * set to valid char. Otherwise copy fragmented char into it. */
	curval->significand_val = (bnj_val_type(curval) != BNJ_STRING) ?
		state->_paf_significand_val : BNJ_EMPTY_CP;

	/* Offsets are always zeroed. */
	curval->key_offset = 0;
	curval->strval_offset = 0;

	/* Initialize parsing state. */
	while(i != end){
		const unsigned st = state->flags;
		const unsigned ctx = state->stack[state->depth];

		switch(st){

			/* INTERSTITIAL zone:
			 * Notes about BNJ_INTERSTITIAL*:
			 *  Can only get here if parser entered a map or list, so
			 *  state->depth > 0
			 *
			 *  if statement ordering:
			 *  Assume that most JSON is compact, so test for ',' and ':' values first.
			 *  */
			case BNJ_INTERSTITIAL:
				{
					if(',' == *i){
						if(!(ctx & BNJ_EXPECT_COMMA)){
							SETSTATE(state->flags, BNJ_ERR_EXTRA_COMMA);
							return i;
						}
						state->stack[state->depth] &= ~BNJ_EXPECT_COMMA;
						++i;

						/* If values saved to state->v and negative depth change,
						 * return those values to the user. */
						if(state->depth_change < 0){
							if(state->user_ctx->user_cb){
								state->user_ctx->user_cb(state, buffer);
								s_reset_state(state);
								curval = state->v;
							}
							else{
								/* Otherwise jump out of the function to return data. */
								SETSTATE(state->flags, BNJ_INTERSTITIAL2);
								return i;
			case BNJ_INTERSTITIAL2:
								SETSTATE(state->flags, BNJ_INTERSTITIAL);
							}
						}

						/*  */
						state->stack[state->depth] |= BNJ_VAL_INCOMPLETE;
						if(ctx & BNJ_OBJECT)
							state->stack[state->depth] |= BNJ_KEY_INCOMPLETE;

						break;
					}
					else if(s_lookup[*i] & CEND){
						/* FIXME ensure list/map match against the stack! */

						/* If positive depth change, call user cb. */
						if(state->depth_change > 0){
							if(state->user_ctx->user_cb){
								state->user_ctx->user_cb(state, buffer);
								s_reset_state(state);
								curval = state->v;
							}
							else{
								/* Otherwise jump out of the function to return data. */
								SETSTATE(state->flags, BNJ_INTERSTITIAL3);
								return i;
			case BNJ_INTERSTITIAL3:
								SETSTATE(state->flags, BNJ_INTERSTITIAL);
							}
						}

						/* Now consume the character. */
						++i;

						/* Decrement depth. */
						--state->depth_change;
						--state->depth;

						/* Terminate on reaching 0 depth.*/
						if(0 == state->depth){
							SETSTATE(state->flags, BNJ_SUCCESS);
							if(state->user_ctx->user_cb){
								state->user_ctx->user_cb(state, buffer);
								s_reset_state(state);
							}
							return i;
						}

						/* Complete value. */
						state->stack[state->depth] &= ~BNJ_VAL_INCOMPLETE;

						break;
					}
					else if(s_lookup[*i] & CWHI){
						++i;
						break;
					}

					/* FALLTHROUGH to value start. */
					SETSTATE(state->flags, BNJ_VALUE_START);
				}
				/* End INTERSTITIAL zone, */

				/* Error if did not see comma. */
				if(state->stack[state->depth] & BNJ_EXPECT_COMMA){
					SETSTATE(state->flags, BNJ_ERR_NO_COMMA);
					return i;
				}

				/* Value Type Resolution zone. */
			case BNJ_VALUE_START:

				/* First check for start of string. */
				if('"' == *i){
					/* Alert user to depth change before reporting values.*/
					if(state->depth_change){
						if(state->user_ctx->user_cb){
							state->user_ctx->user_cb(state, buffer);
							s_reset_state(state);
							curval = state->v;
						}
						else{
							/* Otherwise jump out of the function to return data. */
							SETSTATE(state->flags, BNJ_STRING_START);
							return i;
						}
					}

			case BNJ_STRING_START:
					SETSTATE(state->flags, BNJ_STRING_ST);
					++i;

					/* If expecting key, initialize key matching state. */
					if(state->stack[state->depth] & BNJ_KEY_INCOMPLETE){
						curval->type = BNJ_VFLAG_KEY_FRAGMENT;
						curval->key_enum = 0;
						curval->key_length = 0;
						curval->key_offset = i - buffer;
						state->_key_set_sup = state->user_ctx->key_set_length;
						state->_key_len = 0;
					}
					else {
						/* Otherwise this is a string value. */
						curval->type = BNJ_STRING | BNJ_VFLAG_VAL_FRAGMENT;
						curval->strval_offset = i - buffer;

						/* Zero character count and byte count. */
						curval->cp1_count = 0;
						curval->cp2_count = 0;
						curval->cp3_count = 0;
						curval->exp_val = 0;

						/* "empty" char value is high bit set. */
						curval->significand_val = BNJ_EMPTY_CP;
					}
					break;
				}
				else if(s_lookup[*i] & CWHI){
					/* Skip over whitespace. */
					++i;
					break;
				}
				else if(state->stack[state->depth] & BNJ_KEY_INCOMPLETE){
					/* No other character may follow a '{' */
					SETSTATE(state->flags, BNJ_ERR_MAP_INVALID_CHAR);
					return i;
				}
				else if(s_lookup[*i] & CBEG){
					/* Look for start of list/map. */
					/* Avoid stack overflow. */
					if(state->depth == state->stack_length - 1){
						SETSTATE(state->flags, BNJ_ERR_STACK_OVERFLOW);
						return i;
					}

					/* Infer type. */
					unsigned type = (*i == '[') ? BNJ_ARRAY : BNJ_OBJECT;
					++i;

					assert(state->depth_change >= 0);

					/* Increment depth. */
					++state->depth_change;
					++state->depth;

					/* If this is an object, initialize with incomplete key read. */
					state->stack[state->depth] = type;
					if(BNJ_OBJECT == type)
						state->stack[state->depth] |= BNJ_KEY_INCOMPLETE;

					/* If there is a key on current depth, add it to value list. */
					if(curval->key_length){
						curval->type = (BNJ_OBJECT == type) ? BNJ_OBJ_BEGIN : BNJ_ARR_BEGIN;
						++state->vi;
						curval = state->v + state->vi;
						curval->key_length = 0;
					}

					/* Comma is OK to see at old depth. */
					if(state->depth - 1)
						state->stack[state->depth - 1] |= BNJ_EXPECT_COMMA;

					SETSTATE(state->flags, BNJ_INTERSTITIAL);
					break;
				}
				else{
					SETSTATE(state->flags, BNJ_MINUS);
				}

				/* Alert user to depth change before reporting values.*/
				if(state->depth_change){
					if(state->user_ctx->user_cb){
						state->user_ctx->user_cb(state, buffer);
						s_reset_state(state);
						curval = state->v;
					}
					else{
						/* Otherwise jump out of the function to return data. */
						return i;
					}
				}

			case BNJ_MINUS:
				if('-' == *i){
					curval->type |= BNJ_VFLAG_NEGATIVE_SIGNIFICAND;
					++i;
					if(i == end){
						SETSTATE(state->flags, BNJ_OTHER_VALS);
						break;
					}
				}

			case BNJ_OTHER_VALS:
				/* Record beginning of data value here. */
				curval->strval_offset = i - buffer;

				curval->significand_val = 0;
				curval->exp_val = 0;

				switch(*i){
					case 'I':
						++curval->significand_val;
					case 'N':
						++curval->significand_val;
					case 'n':
						++curval->significand_val;
					case 't':
						++curval->significand_val;
					case 'f':
						{
							/* Only Infinity may have a negative. */
							if((curval->type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND)
									&& curval->significand_val != BNJ_SPC_INFINITY)
							{
								SETSTATE(state->flags,  BNJ_ERR_BAD_VALUE_CHAR);
								return i;
							}

							/* Set new state. */
							SETSTATE(state->flags,  BNJ_RESERVED);
							curval->type |= BNJ_SPECIAL | BNJ_VFLAG_VAL_FRAGMENT;

							/* For internal use, abuse exp_val as character counter. */
						}
						++i;
						break;


					default:
						/* Assume numeric. */
						if(*i < '0' || *i > '9'){
							/* Any nonnumeral fails. */
							SETSTATE(state->flags,  BNJ_ERR_BAD_VALUE_CHAR);
							return i;
						}

						SETSTATE(state->flags,  BNJ_SIGNIFICAND);
						curval->type |= BNJ_VFLAG_VAL_FRAGMENT | BNJ_NUMERIC;
						state->digit_count = 0;
						state->_decimal_offset = -1;
				}
				break;

			/* Significand section. */
			case BNJ_SIGNIFICAND:
				if(*i >= '0' && *i <= '9'){
					/* state->sig = 8 * state->sig + (2 * state->sig + digit) */

					/* compute major addend */
					SIGNIFICAND tmp = curval->significand_val << 1;
					tmp += *i - '0';
					SIGNIFICAND tmp2 = curval->significand_val << 3;
					++i;

					/* Addition overflow check. */
					if(SIGNIFICAND_MAX - tmp > tmp2){
						curval->significand_val = tmp + tmp2;
					}
					else{
						/* FIXME: Some kind of support for bignumbers. */
					}

					/* Increment digit count. */
					++state->digit_count;
					break;
				}

				/* Make sure at least one digit read by now. */
				if(!state->digit_count){
					SETSTATE(state->flags,  BNJ_ERR_NUM_DIGIT_MISSING);
					return i;
				}

				/* upper/lower case 'E' begins exponent. */
				if('E' == (*i & 0xDF)){
					/* Make sure if there is a decimal there are digits between. */
					if(state->_decimal_offset != -1){
						if(state->digit_count == state->_decimal_offset){
							SETSTATE(state->flags,  BNJ_ERR_NUM_DIGIT_MISSING);
							return i;
						}
					}

					/* Abusing _key_set_sup for holding exponent digit count. */
					state->_key_set_sup = 0;

					/* Proceed to check sign. */
					SETSTATE(state->flags, BNJ_EXP_SIGN);

					++i;
				}
				else if('.' == *i){
					/* Error if decimal pops up again. */
					if(-1 != state->_decimal_offset){
						SETSTATE(state->flags,  BNJ_ERR_EXTRA_DECIMAL);
						return i;
					}

					/* Record decimal offset. */
					state->_decimal_offset = state->digit_count;
					++i;
				}
				else {
					/* Assume end of integer. Modify exponent with decimal point. */
					if(-1 != state->_decimal_offset)
						curval->exp_val = -(state->digit_count - state->_decimal_offset);
					SETSTATE(state->flags,  BNJ_END_VALUE);
				}
				break;
			/* End significand section. */

			case BNJ_EXP_SIGN:
				if('-' == *i){
					/* Record negative exponent. */
					curval->type |= BNJ_VFLAG_NEGATIVE_EXPONENT;
					++i;
				}
				else if('+' == *i){
					/* Positive exponent. Just skip over it.*/
					++i;
				}

				SETSTATE(state->flags, BNJ_EXPONENT);
				break;

			case BNJ_EXPONENT:
				if(*i >= '0' && *i <= '9'){
					/* Add to exponent value. */
					curval->exp_val *= 10;
					curval->exp_val += *i - '0';
					if(curval->exp_val > ROUGH_MAX_EXPONENT){
						SETSTATE(state->flags,  BNJ_ERR_MAX_EXPONENT);
						return i;
					}
					++i;
					++state->_key_set_sup;
					break;
				}

				/* Must have at least one exponent digit. */
				if(!state->_key_set_sup){
					SETSTATE(state->flags,  BNJ_ERR_NO_EXP_DIGIT);
					return i;
				}

				/* Assume end of exponent. Sign adjustment. */
				if(curval->type & BNJ_VFLAG_NEGATIVE_EXPONENT)
					curval->exp_val = -curval->exp_val;

				/* Modify exponent with decimal point. */
				if(-1 == state->_decimal_offset)
					state->_decimal_offset = state->digit_count;
				curval->exp_val -= state->digit_count - state->_decimal_offset;
				SETSTATE(state->flags,  BNJ_END_VALUE);
				break;


			/* String section. */
			case BNJ_STRING_ST:
				{
					if(s_lookup[*i] & CINV){
						SETSTATE(state->flags, BNJ_ERR_INVALID);
						return i;
					}
					else if(*i & 0x80 && !(curval->type & BNJ_VFLAG_KEY_FRAGMENT)){
						/* Process first byte. */
						state->_cp_fragment = *i;
						if((*i & 0xF8) == 0xF0){
							/* CINV flag set on invalid 0xF5...*/

							/* 4 byte code point. Will shift by 18 bits, store 21 high bits. */
							state->_cp_fragment &= 0x07;
							state->_cp_fragment |= 0xFFFFF800;
							SETSTATE(state->flags, BNJ_STR_UTF3);
						}
						else if((*i & 0xF0) == 0xE0){
							/* 3 byte code point. Will shift by 12 bits, store 14 high bits. */
							state->_cp_fragment &= 0x0F;
							state->_cp_fragment |= 0xFFFC0000;
							SETSTATE(state->flags, BNJ_STR_UTF2);
						}
						else if((*i & 0xE0) == 0xC0){
							/* NOTE: 0xC0-0xC1 are already marked invalid in table,
							 * so don't bother testing here. */

							/* 2 byte code point. Will shift by 6 bits, store 7 high bits. */
							state->_cp_fragment &= 0x1F;
							state->_cp_fragment |= 0xFE000000;
							SETSTATE(state->flags, BNJ_STR_UTF1);
						}
						else{
							/* Error. */
							SETSTATE(state->flags, BNJ_ERR_UTF_8);
							return i;
						}

						/* Advance. If at end, return and switch back to
						 * appropriate parse point. */
						++i;
						if(i == end)
							return i;
						break;

						/* Process third to last byte. */
			case BNJ_STR_UTF3:
						if((*i & 0xC0) != 0x80){
							SETSTATE(state->flags, BNJ_ERR_UTF_8);
							return i;
						}

						/* Add payload bits to the fragment.
						 * If at end save state to return for next char.
						 * Otherwise, if processing beginning of continuing fragment,
						 * ignore this char (by advancing offset) when copied later. */
						state->_cp_fragment <<= 6;
						state->_cp_fragment |= *i & 0x3F;
						++i;
						if(i == end){
							SETSTATE(state->flags, BNJ_STR_UTF2);
							return i;
						}

						/* Process penultimate byte. */
			case BNJ_STR_UTF2:
						if((*i & 0xC0) != 0x80){
							SETSTATE(state->flags, BNJ_ERR_UTF_8);
							return i;
						}
						state->_cp_fragment <<= 6;
						state->_cp_fragment |= *i & 0x3F;
						++i;
						if(i == end){
							SETSTATE(state->flags, BNJ_STR_UTF1);
							return i;
						}

						/* Process last byte. */
			case BNJ_STR_UTF1:
						if((*i & 0xC0) != 0x80){
							SETSTATE(state->flags, BNJ_ERR_UTF_8);
							return i;
						}
						state->_cp_fragment <<= 6;
						state->_cp_fragment |= *i & 0x3F;

						/* Check for overlong encodings. */
						switch(state->_cp_fragment >> 29){
							/* 4 byte encoding. */
							case 0x7:
								if((state->_cp_fragment & 0xFFFFFF) < 0x10000){
									SETSTATE(state->flags, BNJ_ERR_UTF_8);
									return i;
								}
								break;

							/* 3 byte encoding. */
							case 0x6:
								if((state->_cp_fragment & 0xFFFFFF) < 0x800){
									SETSTATE(state->flags, BNJ_ERR_UTF_8);
									return i;
								}
								break;

							/* 2 byte encoding. Nothing to do. */
							case 0x4:
								break;

								/* No other values should appear. */
							default:
								assert(0);
						}
						state->_cp_fragment &= 0xFFFFFF;

						if(state->_cp_fragment >= 0xD800 && state->_cp_fragment <= 0xDFFF){
							/* MAYBE, should I really reject this? a wstrcpy function
							 * could just convert this to the appropriate character value. */
							SETSTATE(state->flags, BNJ_ERR_UTF_8);
							return i;
						}

						/* Completed UTF-8 multibyte char, add appropriate count now. */
						if(state->_cp_fragment < 0x800)
							++curval->cp2_count;
						else if(state->_cp_fragment < 0x10000)
							++curval->cp3_count;
						else
							++curval->exp_val;

						/* If at the beginning of parse _cp_fragment was not BNJ_EMPTY_CP,
						 * copy the completed _cp_fragment to significand_val. */
						if(first_cp_frag != BNJ_EMPTY_CP){
							curval->strval_offset = i - buffer + 1;
							curval->significand_val = state->_cp_fragment;
							first_cp_frag = 0;
						}

						/* Reset the fragment value. */
						state->_cp_fragment = BNJ_EMPTY_CP;
						SETSTATE(state->flags, BNJ_STRING_ST);
					}
					else if(*i == '"'){
						/* If value ends, then move to end value state; otherwise
						 * parsing a key so move to INTERSTITIAL state.*/
						if(curval->type & BNJ_VFLAG_VAL_FRAGMENT){
							SETSTATE(state->flags, BNJ_END_VALUE);
						}
						else{
							/* ':' character "ends" the key. Clear key incomplete status. */
							SETSTATE(state->flags,BNJ_COLON);
							curval->type &= ~BNJ_VFLAG_KEY_FRAGMENT;
							state->stack[state->depth] &= ~BNJ_KEY_INCOMPLETE;
							if(state->_key_set_sup == curval->key_enum)
								curval->key_enum = state->user_ctx->key_set_length;
						}
					}
					else if(*i == '\\' && !(curval->type & BNJ_VFLAG_KEY_FRAGMENT)){
						/* Escape sequence, initialize fragment to 0. */
						state->_cp_fragment = 0;
						++i;
						if(i == end){
							SETSTATE(state->flags, BNJ_STR_ESC);
							break;
						}


			case BNJ_STR_ESC:
						if('u' == *i){

							/* Starting new UTF-16 code point, so reset cp frag.
							 * However, if processed a first surrogate half, ensure
							 * that we begin a second half. */
							if(1){
								SETSTATE(state->flags, BNJ_STR_U0);
							}
							else{
			case BNJ_STR_SURROGATE:
								if(*i != '\\'){
									SETSTATE(state->flags, BNJ_ERR_UTF_SURROGATE);
									return i;
								}
								++i;
								if(i == end){
									SETSTATE(state->flags, BNJ_STR_SURROGATE_1);
									return i;
								}

			case BNJ_STR_SURROGATE_1:
								if(*i != 'u'){
									SETSTATE(state->flags, BNJ_ERR_UTF_SURROGATE);
									return i;
								}
								++i;
								if(i == end){
									SETSTATE(state->flags, BNJ_STR_SURROGATE_2);
									return i;
								}

			case BNJ_STR_SURROGATE_2:
								if(s_hex(*i) != 0xD){
									SETSTATE(state->flags, BNJ_ERR_UTF_SURROGATE);
									return i;
								}
								++i;
								if(i == end){
									SETSTATE(state->flags, BNJ_STR_SURROGATE_3);
									return i;
								}

			case BNJ_STR_SURROGATE_3:
								if(s_hex(*i) < 0xC){
									SETSTATE(state->flags, BNJ_ERR_UTF_SURROGATE);
									return i;
								}

								/* Keep lower two bits from this value. */
								state->_cp_fragment <<= 2;
								state->_cp_fragment |= s_hex(*i) & 0x3;

								/* Add in 0x10000 (0x100 here since still reading last byte.) */
								state->_cp_fragment += 0x100;
								SETSTATE(state->flags, BNJ_STR_U2);
							}
							++i;

			case BNJ_STR_U0:
			case BNJ_STR_U1:
			case BNJ_STR_U2:
			case BNJ_STR_U3:
							while(i != end){
								if(!(s_lookup[*i] & CHEX)){
									state->flags = BNJ_ERR_INV_HEXESC;
									return i;
								}

								state->_cp_fragment <<= 4;
								state->_cp_fragment |= s_hex(*i);
								++i;

								if(BNJ_STR_U3 != state->flags){
									++state->flags;
									continue;
								}

								/* If not beginning of surrogate pair, then complete char. */
								if(state->_cp_fragment < 0xD800
									|| state->_cp_fragment >= 0xDC00)
								{
									/* Ensure this is not second half of surrogate. */
									if(state->_cp_fragment < 0xD800
										|| state->_cp_fragment >= 0xE000)
									{
										/* Increment appropriate cp count. */
										if(state->_cp_fragment < 0x80)
											++curval->cp1_count;
										else if(state->_cp_fragment < 0x800)
											++curval->cp2_count;
										else if(state->_cp_fragment < 0x10000)
											++curval->cp3_count;
										else
											++curval->exp_val;

										/* If at beginning of parse _cp_fragment was not BNJ_EMPTY_CP,
										 * copy the completed _cp_fragment to significand_val. */
										if(first_cp_frag != BNJ_EMPTY_CP){
											curval->strval_offset = i - buffer + 1;
											curval->significand_val = state->_cp_fragment;
											first_cp_frag = 0;
										}

										/* Reset the fragment value. */
										state->_cp_fragment = BNJ_EMPTY_CP;

										SETSTATE(state->flags, BNJ_STRING_ST);
									}
									else{
										/* Invalid placement of second surrogate half. */
										SETSTATE(state->flags, BNJ_ERR_UTF_SURROGATE);
										return --i;
									}
								}
								else{
									/* Keep only 10 bits. */
									state->_cp_fragment &= 0x3FF;
									SETSTATE(state->flags, BNJ_STR_SURROGATE);
								}
								break;
							}
							break;
						}
						else{
							if(!(s_lookup[*i] & CESC)){
								SETSTATE(state->flags, BNJ_ERR_INV_ESC);
								return i;
							}

							/* If at beginning of parse _cp_fragment was not BNJ_EMPTY_CP,
							 * copy the completed _cp_fragment to significand_val. */
							if(first_cp_frag != BNJ_EMPTY_CP){
								switch(*i){
									case '"':
										curval->significand_val = '"';
										break;
									case '\\':
										curval->significand_val = '\\';
										break;
									case '/':
										curval->significand_val = '/';
										break;
									case 'b':
										curval->significand_val = '\b';
										break;
									case 'f':
										curval->significand_val = '\f';
										break;
									case 'n':
										curval->significand_val = '\n';
										break;
									case 'r':
										curval->significand_val = '\r';
										break;
									case 't':
										curval->significand_val = '\t';
										break;
									default:
										assert(0);
								}
								curval->strval_offset = i - buffer + 1;
								first_cp_frag = 0;
							}

							/* Reset the fragment value. */
							state->_cp_fragment = BNJ_EMPTY_CP;

							/* Increase cp1 count. */
							++(curval->cp1_count);
							SETSTATE(state->flags, BNJ_STRING_ST);
						}
					}
					else{
						/* Normal character, just increment proper length. */
						if(curval->type & BNJ_VFLAG_KEY_FRAGMENT){
							/* Do enum check. */
							if(state->user_ctx->key_set
									&& (state->_key_set_sup != curval->key_enum))
							{
								s_match_key(state, *i);
							}
							++(curval->key_length);
							++(state->_key_len);
						}

						/* Increase both cp1 and character count. */
						++(curval->cp1_count);
					}

					/* Character accepted, increment i. */
					++i;
				}
				break;

			case BNJ_RESERVED:
				{
					/* Attempt to compare rest of string. */
					const char* rest =
						s_reserved_arr[curval->significand_val] + curval->exp_val;
					while(*rest && i != end){
						if(*rest != *i){
							SETSTATE(state->flags , BNJ_ERR_RESERVED);
							return i;
						}
						++rest;
						++i;
					}

					if(!*rest){
						SETSTATE(state->flags , BNJ_END_VALUE);
					}
					else{
						/* Continue later, update value state. */
						/* For internal use, copy length into value as well. */
						curval->exp_val = rest - s_reserved_arr[curval->significand_val];
					}
				}
				break;

			case BNJ_END_VALUE:
				{
					/* Clear fragment and incomplete status. */
					state->v[state->vi].type &= ~BNJ_VFLAG_VAL_FRAGMENT;
					state->stack[state->depth] &= ~BNJ_VAL_INCOMPLETE;

					/* Call user cb. */
					++state->vi;
					if(state->vi == state->vlen){
						if(state->user_ctx->user_cb){
							state->user_ctx->user_cb(state, buffer);
							s_reset_state(state);
						}
						else{
							SETSTATE(state->flags, (0 == state->depth) ? BNJ_SUCCESS : BNJ_END_VALUE2);
							return i;
						}
					}
					curval = state->v + state->vi;
					curval->key_length = 0;

			case BNJ_END_VALUE2:
					curval->type = 0;

					/* If depth == 0, then done parsing. */
					if(0 == state->depth){
						SETSTATE(state->flags, BNJ_SUCCESS);
						return i;
					}

					/* OK to read comma now. */
					state->stack[state->depth] |= BNJ_EXPECT_COMMA;

					/* Otherwise nonempty. */
					SETSTATE(state->flags , BNJ_INTERSTITIAL);
				}
				break;


			case BNJ_COLON:
				if(':' == *i){
					SETSTATE(state->flags, BNJ_INTERSTITIAL);
					++i;
				}
				else if(s_lookup[*i] & CWHI){
					++i;
				}
				else{
					SETSTATE(state->flags, BNJ_ERR_MISSING_COLON);
					return i;
				}
				break;

			default:
				return i;

			/* end switch */
		}
		/* end while */
	}

	/* Ensure user sees fragment. */
	if(bnj_incomplete(state, curval))
		++state->vi;

	/* Ensure unseen values pushed to user if cb. */
	if(state->vi){
		if(state->user_ctx->user_cb){
			state->user_ctx->user_cb(state, buffer);
		}

		/* PAF save. */
		state->_paf_key_enum = curval->key_enum;
		state->_paf_type = curval->type;
		state->_paf_exp_val = curval->exp_val;
		state->_paf_significand_val = curval->significand_val;
	}
	return i;
}

uint8_t* bnj_fragcompact(bnj_val* frag, uint8_t* buffer, uint32_t* len){
	unsigned begin = 0;
	/* Check incomplete key. Implies no value read. */
	if(frag->type & BNJ_VFLAG_KEY_FRAGMENT){
		/* Move key to the beginning of the buffer.
		 * ASSUMING no overlap between src and dst!! */
		memcpy(buffer, buffer + frag->key_offset, frag->key_length);

		/* Shift offset to beginning of buffer. */
		frag->key_offset = 0;
		begin = frag->key_length;
	}

	/* Check incomplete value. If a string copy to beginning of buffer. */
	if(frag->type & BNJ_VFLAG_VAL_FRAGMENT){
		if((frag->type & BNJ_TYPE_MASK) == BNJ_STRING){
			/* String ends at end of buffer, so length = (end - offset). */
			unsigned slen = *len - frag->strval_offset;
			memcpy(buffer + begin, buffer + frag->strval_offset, slen);

			/* Advance new buffer begin by string length. */
			begin += slen;
		}
	}

	*len -= begin;
	return buffer + begin;
}

uint8_t* bnj_json2utf8(uint8_t* dst, size_t destlen, const uint8_t** buff){
	const uint8_t* end = dst + destlen;
	const uint8_t* x = *buff;
	while(dst != end){
		if(*x != '\\'){
			if(!(*x & 0x80)){
				*dst = *x;
				++x;
				++dst;
			}
			else {
				/* UTF-8 start, check for sufficient space. */
				if((*x & 0xF8) == 0xF0){
					if(end - dst < 4){
						*buff = x;
						return dst;
					}
					dst[0] = x[0];
					dst[1] = x[1];
					dst += 2;
					x += 2;
				}
				else if((*x & 0xF0) == 0xE0){
					if(end - dst < 3){
						*buff = x;
						return dst;
					}
					dst[0] = x[0];
					++dst;
					++x;
				}
				else {
					if(end - dst < 2){
						*buff = x;
						return dst;
					}
				}
				dst[0] = x[0];
				dst[1] = x[1];
				dst += 2;
				x += 2;
			}
		}
		else{
			++x;
			switch(*x){
				case '"':
					*dst = '"';
					break;
				case '\\':
					*dst = '\\';
					break;
				case '/':
					*dst = '/';
					break;
				case 'b':
					*dst = '\b';
					break;
				case 'f':
					*dst = '\f';
					break;
				case 'n':
					*dst = '\n';
					break;
				case 'r':
					*dst = '\r';
					break;
				case 't':
					*dst = '\t';
					break;
				case 'u':
					{
						const uint8_t* rollback = x - 1;
						++x;
						unsigned v = s_hex(*x);
						v <<= 4; ++x;
						v |= s_hex(*x);
						v <<= 4; ++x;
						v |= s_hex(*x);
						v <<= 4; ++x;
						v |= s_hex(*x);
						++x;

						if(v >= 0xD800 && v < 0xDC00){
							/* Skip over already read '\', 'u', and first hex */
							x += 3;

							/* Keep only lower 10 bits. */
							v &= 0x3FF;

							/* Apply highest 2 bits. */
							v <<= 2;
							v |= s_hex(*x) & 0x3;

							v <<= 4; ++x;
							v |= s_hex(*x);
							v <<= 4; ++x;
							v |= s_hex(*x);
							++x;

							v += 0x10000;
						}

						/* Write UTF-8 char.
						 * If not enough space in dst, back off and return. */
						uint8_t* res = bnj_utf8_char(dst, end - dst, v);
						if(dst != res)
							dst = res;
						else{
							*buff = rollback;
							return dst;
						}

					}
					continue;

				default:
					/* Should never reach here. */
					assert(0);
					return NULL;
			}
			++dst;
			++x;
		}
	}

	*buff = x;
	return dst;
}

inline uint8_t* bnj_stpncpy8(uint8_t* dst, const bnj_val* src, size_t len,
	const uint8_t* buff)
{
	if(len){
		size_t clen = bnj_strlen8(src);
		if(clen){
			/* Save space for null terminator. */
			--len;

			/* Copy fragmented char if applicable. */
			if(src->significand_val != BNJ_EMPTY_CP){
				uint8_t* res =
					bnj_utf8_char(dst, len, (uint32_t)src->significand_val);
				if(dst != res){
					len -= res - dst;
					clen -= res - dst;
					dst = res;
				}
				else{
					*dst = '\0';
					return dst;
				}
			}

			/* Only copy up to minimum of len and content length. */
			const uint8_t* b = buff + src->strval_offset;
			dst = bnj_json2utf8(dst, (clen < len) ? clen : len, &b);
		}
		*dst = '\0';
	}
	return dst;
}

#ifdef BNJ_WCHAR_SUPPORT
/* TODO Implement wide character functions. */
wchar_t* bnj_wcpncpy(wchar_t* dst, const bnj_val* src, size_t len, const uint8_t* buff){
	return NULL;
}
#endif
