/* Copyright (c) 2010 David Bender assigned to Benegon Enterprises LLC
 * See the file LICENSE for full license information. */

#include <climits>
#include <assert.h>
#include "pull.hh"

enum {
	/* Parse remaining date in buffer. */
	PARSE_ST,

	/* Pull Parser depth lags C parser depth. */
	DEPTH_LAG_ST,

	/* Values to read. */
	VALUE_ST
};


static const char* s_error_msgs[BNJ_ERROR_COUNT] = {
	"Extra comma",
	"No comma",
	"Invalid char after {",
	"Stack overflow",
	"Bad char in value",
	"Digit missing",
	"Extra decimal point",
	"Max exponent exceeded",
	"No exponent digit",
	"Invalid hex character",
	"Invalid escape character",
	"Invalid character",
	"Invalid char in reserved word",
	"List/map mismatch",
	"Missing colon",
	"Invalid UTF-8 char",
	"Invalid UTF-16 surrogate encoding"
};

static const char* s_type_strings[] = {
	"numeric",
	"special",
	"array",
	"map",
	"string"
};

static void s_throw_key_error(const BNJ::PullParser& p, unsigned key_enum){
	const bnj_val& val = p.GetValue();
	char buffer[1024];
	char* x;
	x = stpcpy(buffer, "Key mismatch: expected ");
	x = stpcpy(x, p.c_ctx().key_set[key_enum]);
	x = stpcpy(x, ", found ");
	bnj_stpkeycpy(x, &val, p.Buff());
	throw BNJ::PullParser::input_error(buffer, p.FileOffset(val));
}

static void s_throw_type_error(const BNJ::PullParser& p, unsigned type){
	const bnj_val& val = p.GetValue();
	char buffer[1024];
	char* x;
	x = stpcpy(buffer, "Type mismatch: expected ");
	x = stpcpy(x, s_type_strings[type]);
	x = stpcpy(x, ", got ");
	x = stpcpy(x, s_type_strings[val.type & BNJ_TYPE_MASK]);
	throw BNJ::PullParser::input_error(buffer, p.FileOffset(val));
}

static char* s_compose_9(char* out, unsigned number){
	/* Write 0-padded 9-digit number. */
	*out = '@';
	for(char* y = out + 9; y != out; --y){
		*y = '0' + (number % 10);
		number /= 10;
	}

	out += 10;
	*out = ':';
	++out;

	return out;
}

BNJ::PullParser::Reader::~Reader() throw(){
}

BNJ::PullParser::input_error::input_error(const char* blurb,
	unsigned file_offset)
{
	char* x = s_compose_9(_msg, file_offset);

	/* Copy as many bytes of blurb as possible. */
	x = stpncpy(x, blurb, 127 - (x - _msg));
	*x = '\0';
}

BNJ::PullParser::invalid_value::invalid_value(const char* blurb,
	const PullParser& p)
{
	const bnj_val& val = p.GetValue();

	/* Reserve 1 char in buff for '\0', and 1 for ' ' */
	char* x = s_compose_9(_msg, p.FileOffset(val));

	/* Copy key value if necessary. */
	if(val.key_length){
		x = stpcpy(x, "Key: ");
		x = bnj_stpnkeycpy(x, 254 - (x - _msg), &val, p.Buff());
		*x = ' ';
		++x;
	}

	/* Copy as many bytes of blurb as possible. */
	x = stpncpy(x, blurb, 254 - (x - _msg));
	*x = '\0';
}

/* Only initialize the read state here to NULL values. */
BNJ::PullParser::PullParser(unsigned maxdepth, uint32_t* stack_space)
	: _parser_state(ST_NO_DATA), _buffer(NULL), _len(0), _reader(NULL)
{
	/* Will not operate with a callback. */
	_ctx.user_cb = NULL;
	_ctx.key_set = NULL;
	_ctx.key_set_length = 0;

	/* Allocate the state stack. */
	bnj_state_init(&_pstate, stack_space, maxdepth);
}

BNJ::PullParser::~PullParser(){
}

unsigned BNJ::PullParser::ChunkRead8(char* dest, unsigned destlen,
	unsigned key_enum)
{
	if(_val_idx >= _val_len)
		throw input_error("No chunk string data!", _total_pulled);

	/* out will always point to first empty char. */
	uint8_t* out = (uint8_t*)dest;
	unsigned out_remaining = destlen;
	bnj_val& val = _valbuff[_val_idx];

	/* Verify enum and type. */
	if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
		s_throw_key_error(*this, key_enum);
	if(bnj_val_type(&val) != BNJ_STRING)
		s_throw_type_error(*this, BNJ_STRING);

	while(out_remaining > _utf8_remaining || !dest){
		bnj_val& val = _valbuff[_val_idx];
		bool completed = !bnj_incomplete(&_pstate, &val);

		/* Can fit entire string fragment in destination. */
		if(dest)
			out = bnj_stpncpy8(out, &val, _utf8_remaining + 1, Buff());
		_utf8_remaining = 0;

		/* If value is not a fragment, then have read the whole value. */
		if(completed)
			return out - (uint8_t*)dest;

		/* Decrement out_remaining bytes, but add back in null terminator. */
		out_remaining = destlen - (out - (uint8_t*)dest) + 1;

		/* Just finished reading data from fragment, so must be at end
		 * of unparsed data. Refill the entire buffer. */
		FillBuffer(_len);

		/* Pull will do the work of updating the buffer here.
		 * Jump directly to PARSE_ST so Pull() will not jump back here! */
		_state = PARSE_ST;
		State s = Pull(_ctx.key_set, _ctx.key_set_length);
		assert(ST_DATUM == s);
	}

	/* Save room for null terminator. */
	--out_remaining;

	/* Copy fragmented char if applicable. */
	if(val.significand_val != BNJ_EMPTY_CP){
		out = bnj_utf8_char((uint8_t*)dest, out_remaining,
			(uint32_t)val.significand_val);
		if((uint8_t*)dest != out){
			unsigned written = out - (uint8_t*)dest;
			out_remaining -= written;
			_utf8_remaining -= written;
		}
		else{
			*out = '\0';
			return out - (uint8_t*)dest;
		}
	}

	/* Only copy up to minimum of len and content length. */
	const uint8_t* x = Buff() + val.strval_offset;
	const uint8_t* b = x;
	uint8_t* res = bnj_json2utf8(out, out_remaining, &b);

	/* Update bytes remaining in the input buffer. */
	_offset += b - x;
	_utf8_remaining -= res - out;
	
	/* Return number of bytes written to output. */
	return res - (uint8_t*)dest;
}

void BNJ::PullParser::Begin(uint8_t* buffer, unsigned len, Reader* reader) throw(){
	_buffer = buffer;
	_len = len;
	_reader = reader;
	_total_parsed = 0;
	_total_pulled = 0;

	/* Reset state. */
	_depth = 0;
	_first_empty = 0;
	_val_idx = 0;
	_val_len = 0;

	/* Initialize here since _offset uses _first_unparsed as a default. */
	_first_unparsed = 0;
	_utf8_remaining = 0;

	_state = PARSE_ST;
	_parser_state = ST_BEGIN;
}

unsigned BNJ::PullParser::FileOffset(const bnj_val& v) const throw(){
	/* FIXME! */
	return _total_pulled + v.strval_offset;
}

/* NOTE the approach here (reading phase, parsing phase) is the not most
 * latency reducing approach. Its probably not even the approach that would
 * maximize bandwidth. However it is an easy approach. */
BNJ::PullParser::State BNJ::PullParser::Pull(char const * const * key_set,
	unsigned key_set_length)
{

	/* For now, always set new user keys.
	 * FIXME: Is there a better spot for this assignment? */
	_ctx.key_set = key_set;
	_ctx.key_set_length = key_set_length;

	/* If incoming on value, then user must have gotten value on last Pull().
	 * Advance to next value. If at end, parse more data. */
	if(VALUE_ST == _state){
		assert(_val_idx < _val_len);
		bnj_val* tmp = _valbuff + _val_idx;
		/* If user ignored fragmented string just parse through it.
		 * ONLY STRINGS should make it here.
		 * Call ChunkRead8() until nothing left.. */
		if(bnj_incomplete(&_pstate, tmp)){
			assert(bnj_val_type(tmp) == BNJ_STRING);
			while(ChunkRead8(NULL, 0));
		}

		/* Transition to depth lag state in case depth changed. */
		++_val_idx;
		if(_val_len == _val_idx)
			_state = DEPTH_LAG_ST;
	}

	/* No fragments entering the switch loop. */
	unsigned frag_key_len = 0;

	while(true){
		switch(_state){
			case PARSE_ST:
				{
					/* Parser is at end state when depth is 0 AND not at begin state. */
					if(!_depth && _parser_state != ST_BEGIN){
						_parser_state = ST_NO_DATA;
						return _parser_state;
					}

					/* If no more bytes to parse, then go to read state. */
					if(_first_empty == _first_unparsed){
						/* Completely read all the bytes in the buffer. */
						FillBuffer(_len);
					}

					/* Assign output values. */
					_pstate.v = _valbuff;
					_pstate.vlen = 4;
					_val_idx = 0;
					_val_len = 0;

					/* Set offset to where parsing begins. Parse data.
					 * Update parsed counter. */
					_total_pulled = _total_parsed;
					const uint8_t* res = bnj_parse(&_pstate, &_ctx,
						_buffer + _first_unparsed, _first_empty - _first_unparsed);
					_total_parsed += res - (_buffer + _first_unparsed);

					/* Abort on error. */
					if(_pstate.flags & BNJ_ERROR_MASK)
						throw input_error(s_error_msgs[_pstate.flags - BNJ_ERROR_MASK],
							_total_parsed);

					if(!frag_key_len){
						_offset = _first_unparsed;
					}
					else{
						/* Restore key length to first value.
						 * Increment since more chars may have been added. */
						_pstate.v->key_length += frag_key_len;

						/* Bias any other values read in.
						 * Start point moves away from offset. */
						for(unsigned i = 0; i <= _pstate.vi; ++i){
							_pstate.v[i].key_offset += frag_key_len;
							_pstate.v[i].strval_offset += frag_key_len;
						}
						_pstate.v->key_offset += 0;

						frag_key_len = 0;
						_offset = 0;
					}

					/* Advance first unparsed to where parsing ended. */
					_first_unparsed = res - _buffer;

					/* If read any semblance of values, then switch to value state. */
					if(_pstate.vi){
						_val_len = _pstate.vi;
						_state = VALUE_ST;
						break;
					}

					/* Otherwise must be lagging c parser depth.*/
					/* FALLTHROUGH */
					_state = DEPTH_LAG_ST;
				}

			/* Catch up to c parser's depth. */
			case DEPTH_LAG_ST:
				if(_depth < _pstate.depth){
					++_depth;
					_parser_state = (_pstate.stack[_depth] & BNJ_OBJECT) ? ST_MAP : ST_LIST;
					return _parser_state;
				}
				else if(_depth > _pstate.depth){
					_parser_state = (_pstate.stack[_depth] & BNJ_OBJECT) ? ST_ASCEND_MAP
						: ST_ASCEND_LIST;
					--_depth;
					return _parser_state;
				}

				/* Otherwise, parser depth caught up to c parser depth.
				 * Parse more data. */
				_state = PARSE_ST;
				break;

			case VALUE_ST:
				{
					bnj_val* tmp = _valbuff + _val_idx;
					unsigned type = bnj_val_type(tmp);

					/* Remember number of UTF-8 characters.
					 * ChunkRead*() will need this initialized. */
					if(type == BNJ_STRING)
						_utf8_remaining = bnj_strlen8(tmp);

					/* If key is incomplete or non-string value incomplete,
					 * then attempt to read full value. */
					if(bnj_incomplete(&_pstate, tmp)){

						/* If string value then ChunkRead*() will handle fragmentation. */
						if(type == BNJ_STRING){
							_state = VALUE_ST;
							_parser_state = ST_DATUM;
							return ST_DATUM;
						}

						/* Move fragment from near buffer end to buffer start
						 * so FillBuffer() appends to fragment.
						 * Note only string value fragments are 'large' fragments.
						 * Since string fragments are filtered out at this point, the
						 * fragment shift should not be very expensive. */

						/* Bias key offsets before shifting fragment.
						 * Note strval_offset only applies to BNJ_STRING. */
						tmp->key_offset += _offset;
						unsigned length = _len;
						uint8_t* start = bnj_fragcompact(tmp, _buffer, &length);
						_first_empty = start - _buffer;

						/* Remember key offset and length. */
						frag_key_len = tmp->key_length;

						/* Fill buffer and switch to parsing state. */
						FillBuffer(length + _first_empty);
						_state = PARSE_ST;
						break;
					}

					/* Otherwise have a piece of data ready to return to the user. */
					_parser_state = ST_DATUM;

					/* If beginning map or list, then lagging depth. */
					if(BNJ_ARR_BEGIN == type || BNJ_OBJ_BEGIN == type){
						_state = DEPTH_LAG_ST;
						break;
					}

					return _parser_state;
				}
				break;

			/* Should not be any other states. */
			default:
				assert(0);
		}
	}
}

BNJ::PullParser::State BNJ::PullParser::Up(void){
	/* Loop until parser depth goes above destination depth. Drop the data. */
	const unsigned dest_depth = _depth - 1;
	while(dest_depth < _depth)
		Pull();
	return _parser_state;
}

/* Updates the following fields:
 * -_first_empty
 * -_first_unparsed
 * -_buffer
 * */
void BNJ::PullParser::FillBuffer(unsigned end_bound){
	_first_empty %= _len;
	_first_unparsed = _first_empty;
	while(_first_empty < end_bound){
		int bytes_read =
			_reader->Read(_buffer + _first_empty, end_bound - _first_empty);

		/* FIXME Handle signal interruption. */
		if(bytes_read < 0)
			throw std::runtime_error("PullParser::Pull Read error.");

		if(0 == bytes_read)
			break;
		_first_empty += bytes_read;
	}
}

unsigned BNJ::GetKey(char* dest, unsigned destlen, const PullParser& p){
	const bnj_val& val = p.GetValue();
	if(dest){
		if(val.key_length > destlen)
			throw PullParser::input_error("Key value overlong!", p.FileOffset(val));
		return bnj_stpkeycpy(dest, &val, p.Buff()) - dest;
	}
	return 0;
}

void BNJ::Get(unsigned& u, const PullParser& p, unsigned key_enum){
	const bnj_val& val = p.GetValue();
	if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
		s_throw_key_error(p, key_enum);
	if(bnj_val_type(&val) != BNJ_NUMERIC)
		s_throw_type_error(p, BNJ_NUMERIC);

	if(val.exp_val)
		throw PullParser::invalid_value("Non-integral numeric value!", p);

	/* Check sign. */
	if(val.type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND)
		throw PullParser::invalid_value("Must be nonnegative!", p);

	u = val.significand_val;
}

void BNJ::Get(int& ret, const PullParser& p, unsigned key_enum){
	const bnj_val& val = p.GetValue();
	if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
		s_throw_key_error(p, key_enum);
	if(bnj_val_type(&val) != BNJ_NUMERIC)
		s_throw_type_error(p, BNJ_NUMERIC);

	if(val.exp_val)
		throw PullParser::input_error("Non-integral numeric value!", p.FileOffset(val));

	if(val.significand_val > LONG_MAX)
		throw PullParser::input_error("Out of range integer!", p.FileOffset(val));

	ret = val.significand_val;
	if(val.type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND)
		ret = -ret;
}


void BNJ::Get(float& f, const PullParser& p, unsigned key_enum){
	const bnj_val& val = p.GetValue();
	if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
		s_throw_key_error(p, key_enum);

	unsigned t = bnj_val_type(&val);
	if(BNJ_NUMERIC == t){
		f = bnj_float(&val);
	}
	else if(BNJ_SPECIAL == t && val.significand_val > BNJ_SPC_NULL){
		if(BNJ_SPC_NAN == t)
			f = NAN;
		else
			f = (val.type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND) ? -INFINITY: INFINITY;
	}
	else
		s_throw_type_error(p, BNJ_NUMERIC);
}

void BNJ::Get(double& d, const PullParser& p, unsigned key_enum){
	const bnj_val& val = p.GetValue();
	if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
		s_throw_key_error(p, key_enum);

	unsigned t = bnj_val_type(&val);
	if(BNJ_NUMERIC == t){
		d = bnj_double(&val);
	}
	else if(BNJ_SPECIAL == t && val.significand_val > BNJ_SPC_NULL){
		if(BNJ_SPC_NAN == t)
			d = NAN;
		else
			d = (val.type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND) ? -INFINITY: INFINITY;
	}
	else
		s_throw_type_error(p, BNJ_NUMERIC);
}

void BNJ::Get(bool& b, const PullParser& p, unsigned key_enum){
	const bnj_val& val = p.GetValue();
	if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
		s_throw_key_error(p, key_enum);

	unsigned t = bnj_val_type(&val);
	if(BNJ_SPECIAL == t){
		if(BNJ_SPC_FALSE == val.significand_val){
			b = false;
			return;
		}
		else if(BNJ_SPC_TRUE == val.significand_val){
			b = true;
			return;
		}
	}
	s_throw_type_error(p, BNJ_SPECIAL);
}


void BNJ::VerifyNull(const PullParser& p, unsigned key_enum){
	const bnj_val& val = p.GetValue();
	if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
		s_throw_key_error(p, key_enum);
	if(bnj_val_type(&val) != BNJ_SPECIAL
		|| val.significand_val != BNJ_SPC_NULL)
	{
		s_throw_type_error(p, BNJ_SPECIAL);
	}
}

void BNJ::VerifyList(const PullParser& p, unsigned key_enum){
	if(key_enum != 0xFFFFFFFF){
		const bnj_val& val = p.GetValue();
		if(val.key_enum != key_enum)
			s_throw_key_error(p, key_enum);
	}
	if(p.GetState() != PullParser::ST_LIST)
		throw PullParser::input_error("Expected List!", p.FileOffset(p.GetValue()));
}

void BNJ::VerifyMap(const PullParser& p, unsigned key_enum){
	if(key_enum != 0xFFFFFFFF){
		const bnj_val& val = p.GetValue();
		if(val.key_enum != key_enum)
			s_throw_key_error(p, key_enum);
	}
	if(p.GetState() != PullParser::ST_MAP)
		throw PullParser::input_error("Expected Map!", p.FileOffset(p.GetValue()));
}
