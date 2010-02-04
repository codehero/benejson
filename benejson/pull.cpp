/* Copyright (c) 2010 David Bender assigned to Benegon Enterprises LLC
 * See the file LICENSE for full license information. */

#include <cstring>
#include <cstdio>
#include <climits>
#include <assert.h>
#include "pull.hh"

static const char* s_error_msgs[BNJ_ERROR_COUNT] = {
	"",
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
	"Invalid UTF-8 char"
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
	x = stpcpy(x, p.c_state().user_ctx->key_set[key_enum]);
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

BNJ::PullParser::Reader::~Reader() throw(){
}

BNJ::PullParser::input_error::input_error(const char* msg, unsigned offset) :
	std::runtime_error(msg), file_offset(offset)
{
}

/* Only initialize the read state here to NULL values. */
BNJ::PullParser::PullParser(unsigned maxdepth, uint32_t* stack_space)
	: _parser_state(ST_NO_DATA), _buffer(NULL), _len(0), _reader(NULL),
	_state(READ_ST)
{
	/* Will not operate with a callback. */
	_ctx.user_cb = NULL;
	_ctx.key_set = NULL;
	_ctx.key_set_length = 0;

	/* Allocate the state stack. */
	bnj_state_init(&_pstate, stack_space, maxdepth);

	_pstate.user_ctx = &_ctx;
}

BNJ::PullParser::~PullParser(){
}

unsigned BNJ::PullParser::Consume8(char* dest, unsigned destlen,
	unsigned key_enum)
{
	if(dest && _val_idx < _val_len){
		bnj_val& val = _valbuff[_val_idx];
		if(_utf8_remaining){
			if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
				s_throw_key_error(*this, key_enum);
			if((val.type & BNJ_TYPE_MASK) != BNJ_STRING)
				s_throw_type_error(*this, BNJ_STRING);

			const uint8_t* x = Buff();
			if(destlen >= _utf8_remaining + 1 || !dest){
				/* Can fit entire string fragment in destination. */
				uint8_t* pos = bnj_json2utf8((uint8_t*)dest, _utf8_remaining, &x);

				/* If value is not a fragment, then have read the whole value. */
				if(!(val.type & BNJ_VFLAG_VAL_FRAGMENT)){
					/* Finished value. */
					++_val_idx;

					/* Null terminate and return. */
					*pos = '\0';
					return pos - (uint8_t*)dest;
				}

				/* Just finished reading data from fragment, so must be at end
				 * of unparsed data. Refill the entire buffer. */
				FillBuffer(_len);

				/* Only try to finish current value. */
				_pstate.v = _valbuff;
				_pstate.vlen = 1;

				/* Parse data. Advance. */
				const uint8_t* res = bnj_parse(&_pstate, _buffer + _first_unparsed,
					_first_empty - _first_unparsed);
				_first_unparsed = res - _buffer;

				/* If there are chars still remaining, then keep this value. */
				_val_idx = 0;
				_utf8_remaining = bnj_strlen8(_valbuff + _val_idx);
				_val_len = (_utf8_remaining) ? 1 : 0;

				/* FIXME: Loop to fill up user buffer? */
			}
			else {
				/* Read as many bytes as possible. Subtract what was read. */
				uint8_t* pos = bnj_json2utf8((uint8_t*)dest, destlen - 1, &x);
				/* Update bytes remaining in the input buffer. */
				_offset += x - Buff();

				/* Return number of bytes written to output. */
				unsigned diff = pos - (uint8_t*)dest;
				_utf8_remaining -= diff;
				return diff;
			}
		}
	}
	return 0;
}

void BNJ::PullParser::Begin(uint8_t* buffer, unsigned len, Reader* reader) throw(){
	_buffer = buffer;
	_len = len;
	_reader = reader;
	_pull_parsed = 0;

	/* Reset state. */
	_depth = 0;
	_first_empty = 0;
	_val_idx = 0;
	_val_len = 0;

	/* Initialize here since _offset uses this as a default. */
	_first_unparsed = 0;
	_utf8_remaining = 0;

	_parser_state = ST_BEGIN;
}

unsigned BNJ::PullParser::FileOffset(const bnj_val& v) const throw(){
	return 0;
}

/* NOTE the approach here (reading phase, parsing phase) is the not most
 * latency reducing approach. Its probably not even the approach that would
 * maximize bandwidth. However it is an easy approach. */
BNJ::PullParser::State BNJ::PullParser::Pull(char const * const * key_set,
	unsigned key_set_length)
{
	/* Parser is at end state when depth is 0 and not beginning. */
	if(!_depth && _parser_state != ST_BEGIN){
		_parser_state = ST_NO_DATA;
		return _parser_state;
	}

	unsigned end_bound = _len;

	if(_val_idx < _val_len){
		const bnj_val* tmp = _valbuff + _val_idx;
		/* If value remaining then. */
		if(_val_idx < _val_len - 1){
			++_val_idx;
			++tmp;

			/* If not last value then just return. */
			if(_val_idx < _val_len - 1)
				return ST_DATUM;

			/* If value is complete just return. */
			if(!bnj_incomplete(&_pstate, tmp))
				return ST_DATUM;

			/* If string value then Consume*() will handle fragmentation. */
			if(bnj_val_type(tmp) == BNJ_STRING){
				/* Remember number of UTF-8 characters.
				 * Consume*() will need this initialized. */
				_utf8_remaining = bnj_strlen8(tmp);
				return ST_DATUM;
			}

			/* Move fragment to buffer start so FillBuffer() appends. */
			uint8_t* start = bnj_fragshift(_pstate.v, _buffer, &end_bound);
			_first_unparsed = 0;
			_first_empty = start - _buffer;
		}
		else if(bnj_incomplete(&_pstate, tmp)){
			/* If user ignored fragmented string just parse through it.
			 * ONLY STRINGS should make it here.
			 * Call Consume() until nothing left.. */
			assert(bnj_val_type(tmp) == BNJ_STRING);
			while(Consume8(NULL, 0));
		}

		/* Time to read more data... */
		_state = READ_ST;
	}
	else{
		/* Return 0 elements if pullparser depth lags parser depth.
		 * Indicate type of structure. */
		if(_depth < _pstate.depth){
			++_depth;
			_parser_state = (_pstate.stack[_depth] & BNJ_OBJECT) ? ST_MAP : ST_LIST;
			return _parser_state;
		}
		else if(_depth > _pstate.depth){
			unsigned st = _pstate.stack[_depth];
			--_depth;
			_parser_state = (st & BNJ_OBJECT) ? ST_ASCEND_MAP : ST_ASCEND_LIST;
			return _parser_state;
		}

		/* Beginning new context, pay attention to new user keys. */
		_ctx.key_set = key_set;
		_ctx.key_set_length = key_set_length;
	}

	/* Assume offset starts at first unparsed. */
	_offset = _first_unparsed;

	/* Fill all empty bytes. At most will do this twice per Pull() call. */
	if(READ_ST == _state){
reread:
		FillBuffer(end_bound);
		_state = PARSE_ST;
	}

	/* Assign output values. */
	_pstate.v = _valbuff;
	_pstate.vlen = 4;
	_val_idx = 0;
	_val_len = 0;

	/* Parse data. Adjust value read count. */
	const uint8_t* res = bnj_parse(&_pstate, _buffer + _first_unparsed,
		_first_empty - _first_unparsed);

	/* Abort on error. */
	if(_pstate.flags & BNJ_ERROR_MASK)
		throw input_error(s_error_msgs[_pstate.flags - BNJ_ERROR_MASK], 0);

	/* When one key:value is only partially read, may need to call the
	 * reader and parser again to completely interpret the value. Really want
	 * to return a complete value to the user if possible.
	 * -Do not bother to do go through with this if the value is a string, since
	 *  Consume*() handle this situation already.
	 * -Non-string values and their keys do not are smaller than _len. */
	if(1 == _pstate.vi && bnj_incomplete(&_pstate, _pstate.v)){
		if(bnj_val_type(_pstate.v) != BNJ_STRING){

			/* Check if ran out of data before finished parsing! */
			if(_first_empty != end_bound)
				throw std::runtime_error("Abrupt end of data!");

			/* Bias key offset && shift incomplete pieces to front of buffer. */
			_pstate.v->key_offset += _first_unparsed;
			uint8_t* start = bnj_fragshift(_pstate.v, _buffer, &end_bound);
			_first_empty = start - _buffer;
			end_bound -= _first_empty;
			goto reread;
		}
	}

	if(_pstate.vi){
		_val_len = _pstate.vi;
		_val_idx = 0;
		_parser_state = ST_DATUM;
	}
	else if(_depth < _pstate.depth){
		++_depth;
		_parser_state = (_pstate.stack[_depth] & BNJ_OBJECT) ? ST_MAP : ST_LIST;
	}
	else if(_depth > _pstate.depth){
		unsigned st = _pstate.stack[_depth];
		--_depth;
		_parser_state = (st & BNJ_OBJECT) ? ST_ASCEND_MAP : ST_ASCEND_LIST;
	}

	/* Advance to where parser left off. */
	_first_unparsed = res - _buffer;
	return _parser_state;
}

void BNJ::PullParser::Up(void){
	const unsigned dest_depth = _depth - 1;

	unsigned st = _pstate.stack[_depth];

	/* Loop until parser depth goes above destination depth. Drop the data. */
	while(dest_depth <= _pstate.depth)
		Pull();

	/* If at greater depth than parser then just decrement depth. */
	_depth = dest_depth;

	if(!_depth)
		_parser_state = ST_NO_DATA;
	else
		_parser_state = (st & BNJ_OBJECT) ? ST_ASCEND_MAP : ST_ASCEND_LIST;
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
			throw std::runtime_error("First key value overlong!");
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
		throw std::runtime_error("Second key must be integral!");

	/* Check sign. */
	if(val.type & BNJ_VFLAG_NEGATIVE_SIGNIFICAND)
		throw std::runtime_error("Must be nonnegative!");

	u = val.significand_val;
}

void BNJ::Get(int& ret, const PullParser& p, unsigned key_enum){
	const bnj_val& val = p.GetValue();
	if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
		s_throw_key_error(p, key_enum);
	if(bnj_val_type(&val) != BNJ_NUMERIC)
		s_throw_type_error(p, BNJ_NUMERIC);

	if(val.exp_val)
		throw std::runtime_error("Second key must be integral!");

	if(val.significand_val > LONG_MAX)
		throw std::runtime_error("Out of range integer!");

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


void BNJ::VerifyNull(const PullParser& p, unsigned key_enum){
	const bnj_val& val = p.GetValue();
	if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
		s_throw_key_error(p, key_enum);
	if(bnj_val_type(&val) != BNJ_SPC_NULL
		&& val.significand_val != BNJ_SPC_NULL)
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
		throw std::runtime_error("Third key is not list!");
}

void BNJ::VerifyMap(const PullParser& p, unsigned key_enum){
	if(key_enum != 0xFFFFFFFF){
		const bnj_val& val = p.GetValue();
		if(val.key_enum != key_enum)
			s_throw_key_error(p, key_enum);
	}
	if(p.GetState() != PullParser::ST_MAP)
		throw std::runtime_error("Third key is not map!");
}
