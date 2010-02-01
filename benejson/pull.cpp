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
	"Missing colon"
};

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

	/* Return data from buffer if available. */
	++_val_idx;
	if(_val_idx < _val_len){
		return ST_DATUM;
	}

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

	_ctx.key_set = key_set;
	_ctx.key_set_length = key_set_length;

	/* Assign output values. */
	_pstate.v = _valbuff;
	_pstate.vlen = 4;

	/* Number of values read so far. */
	unsigned vals_read = 0;
	unsigned lastkeylen = 0;
	unsigned laststrlen = 0;
	unsigned lastoffset = _len;
	unsigned end_bound = _len;
	int bytes_read;

	/* Assume offset starts at first unparsed. */
	_offset = _first_unparsed;

	/* Fill all empty bytes. At most will do this twice per Pull() call. */
reread:
	if(READ_ST == _state){
		_first_empty %= _len;
		_first_unparsed = _first_empty;
		while(_first_empty < end_bound){
			bytes_read = _reader->Read(_buffer + _first_empty, end_bound - _first_empty);

			/* FIXME Handle signal interruption. */
			if(bytes_read < 0)
				throw std::runtime_error("PullParser::Pull Read error.");

			if(0 == bytes_read)
				break;
			_first_empty += bytes_read;
		}
		_state = PARSE_ST;
	}

	/* Parse data. Adjust value read count. */
	const uint8_t* res = bnj_parse(&_pstate, _buffer + _first_unparsed,
		_first_empty - _first_unparsed);

	if(_pstate.flags & BNJ_ERROR_MASK){
		throw input_error(s_error_msgs[_pstate.flags - BNJ_ERROR_MASK], 0);
	}

	/* Assume all values are complete. If last is incomplete, decrement later. */
	vals_read += _pstate.vi;

	/* When coming from fragment, lastkeylen and laststrlen MAY be nonzero. */
	_pstate.v[0].key_length += lastkeylen;
	_pstate.v[0].strval_length += laststrlen;
	if(lastoffset != _len)
		_pstate.v[0].key_offset = lastoffset;

	uint_fast8_t t = _pstate.vi ? _pstate.v[_pstate.vi - 1].type : 0;

	/* Check if last value is fragmented. */
	/* If key fragmentation, copy partial key to buffer start. */
	if(t & (BNJ_VFLAG_KEY_FRAGMENT | BNJ_VFLAG_VAL_FRAGMENT)){
		unsigned offset;
		unsigned mlen;

		/* Must have read at least one value. */
		assert(_pstate.vi);

		/* Partial value, so decrement vals_read. */
		--vals_read;

		/* If value fragmentation, copy partial value to beginning of buffer. */
		if(t & BNJ_VFLAG_VAL_FRAGMENT){
			laststrlen = _pstate.v[_pstate.vi - 1].strval_length;
			mlen = laststrlen;
			offset = _pstate.v[_pstate.vi - 1].strval_offset;
		}
		else{
			lastkeylen = _pstate.v[_pstate.vi - 1].key_length;
			mlen = lastkeylen;
			offset = _pstate.v[_pstate.vi - 1].key_offset;
		}

		/* Make sure there is enough space at the beginning of the buffer. */
		if(mlen < _first_unparsed){
			memmove(_buffer, _buffer + offset, mlen);
			_first_empty = mlen;
		}
		/* ELSE Just return completed values. */
		else{
			/* FIXME */
			throw std::runtime_error("Internal error!");
		}

	}
	/* ELSE If key complete but value not read yet, just reiterate. */
	else if(_pstate.stack[_pstate.depth] & (BNJ_VAL_INCOMPLETE)){
		/* Partial [key:]value, so decrement vals_read. */
		if(vals_read){
			--vals_read;
			/* Save key length, offset here since this is not counted as "COMPLETED". */
			lastkeylen = _pstate.v[_pstate.vi - 1].key_length;
			if(lastkeylen)
				lastoffset = _first_unparsed + _pstate.v[_pstate.vi - 1].key_offset;
		}
	}
	/* ELSE Input buffer not filled, continue. */
	else if((vals_read < 4 && _pstate.depth == _depth) && BNJ_SUCCESS != _pstate.flags){
		/* Just read more values. */
	}
	/* OTHERWISE SUCCESS, either got all the values or the depth changed. */
	else{
		if(vals_read){
			_val_len = vals_read;
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

	/* Must have consumed all bytes read. */
	//assert(res == _buffer + _first_empty);

	/* If EOF occurred before filling buffer, give up. */
	if(_first_empty < end_bound)
		throw std::runtime_error("PullParser::Pull Unexpected EOF");

	/* Buffer full when _offset == 0. Buffer parsed but not sent to user.*/
	if(0 == _offset){
		/* FIXME */
		throw std::runtime_error("Internal error 2!");
	}

	/* Limit next read phase to the first COMPLETED key or string. */
	for(unsigned i = 0; i < vals_read; ++i){
		if(_pstate.v[i].key_length){
			end_bound = _first_unparsed + _pstate.v[i].key_offset;
			break;
		}
		else if(BNJ_STRING <= (_pstate.v[i].type & BNJ_TYPE_MASK)){
			end_bound = _first_unparsed + _pstate.v[i].strval_offset;
			break;
		}
	}

	/* Shift COMPLETED key/string offsets to coincide with buffer start. */
	for(unsigned i = 0; i < vals_read; ++i){
		if(_pstate.v[i].key_length)
			_pstate.v[i].key_offset += _first_unparsed;
		_pstate.v[i].strval_offset += _first_unparsed;
	}

	/*  */
	_state = READ_ST;
	_offset = 0;

	/* Resume from first incomplete value. */
	_pstate.v += vals_read;
	_pstate.vlen -= vals_read;

	goto reread;
}

void BNJ::PullParser::Up(void){
	//fprintf(stderr, " >>> Pull: Up %d %d\n", _depth, _pstate.depth);
	const unsigned dest_depth = _depth - 1;

	unsigned st = _pstate.stack[_depth];

	/* Loop until parser depth goes above destination depth. Drop the data. */
	while(dest_depth <= _pstate.depth){
		//fprintf(stderr, " >>> Going: Up %d %d %d\n", dest_depth, _depth, _pstate.depth);
		Pull();
	}

	/* If at greater depth than parser then just decrement depth. */
	_depth = dest_depth;

	if(!_depth)
		_parser_state = ST_NO_DATA;
	else
		_parser_state = (st & BNJ_OBJECT) ? ST_ASCEND_MAP : ST_ASCEND_LIST;

	//fprintf(stderr, " >>> Pull Final: Up %d %d\n", _depth, _pstate.depth);
}

static const char* s_type_strings[] = {
	"numeric",
	"special",
	"array",
	"map",
	"UTF-8",
	"UTF-16",
	"UTF-32"
};

static void s_throw_key_error(const BNJ::PullParser& p, unsigned key_enum){
	const bnj_val& val = p.GetValue();
	char buffer[1024];
	char* x;
	x = stpcpy(buffer, "Key mismatch: expected ");
	x = stpcpy(x, p.c_state().user_ctx->key_set[key_enum]);
	x = stpcpy(x, ", found ");
	x = bnj_stpncpy(x, &val, 1024 - (x - buffer), p.Buff(), 'K');
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

unsigned BNJ::GetKey(char* dest, unsigned destlen, const PullParser& p){
	const bnj_val& val = p.GetValue();
	if(dest){
		if(val.key_length > destlen)
			throw std::runtime_error("First key value overlong!");
		return bnj_stpcpy(dest, &val, p.Buff(), 'K') - dest;
	}
	return 0;
}

unsigned BNJ::Consume(char* dest, unsigned destlen, const PullParser& p,
	unsigned key_enum)
{
	const bnj_val& val = p.GetValue();
	if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
		s_throw_key_error(p, key_enum);
	if((val.type & BNJ_TYPE_MASK) != BNJ_UTF_8)
		s_throw_type_error(p, BNJ_UTF_8);
	if(dest){
		if(val.strval_length > destlen)
			throw std::runtime_error("First key value overlong!");
		return bnj_stpcpy(dest, &val, p.Buff(), 0) - dest;
	}
	return 0;
}

void BNJ::Get(unsigned& u, const PullParser& p, unsigned key_enum){
	const bnj_val& val = p.GetValue();
	if(key_enum != 0xFFFFFFFF && val.key_enum != key_enum)
		s_throw_key_error(p, key_enum);
	if((val.type & BNJ_TYPE_MASK) != BNJ_NUMERIC)
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
	if((val.type & BNJ_TYPE_MASK) != BNJ_NUMERIC)
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

	unsigned t = val.type & BNJ_TYPE_MASK;
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

	unsigned t = val.type & BNJ_TYPE_MASK;
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
	if((val.type & BNJ_TYPE_MASK) != BNJ_SPC_NULL
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
