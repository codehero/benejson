#include "memreader.hh"

BNJ::MemReader::MemReader(const uint8_t* buff, unsigned len) throw()
	: _buff(buff), _length(len), _pos(0)
{
}

BNJ::MemReader::~MemReader() throw(){
}

int BNJ::MemReader::Read(uint8_t* buff, unsigned len) throw(){
	unsigned amount = (len < _length - _pos) ? len : _length - _pos;
	memcpy(buff, _buff + _pos, amount);
	_pos += amount;
	return amount;
}
