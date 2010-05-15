/* Copyright (c) 2010 David Bender assigned to Benegon Enterprises LLC
 * See the file LICENSE for full license information.
 * 
 * Version 0.9.0, May 12 2010
 * */

#ifndef __BENEGON_JSON_PULL_PARSER_HH__
#define __BENEGON_JSON_PULL_PARSER_HH__

#include <stdexcept>

extern "C" {
	#include "benejson.h"
}

namespace BNJ {
	/** @brief 
	 * -Handles input details
	 * -User driven parsing
	 * -Handles fragmentation
	 * -Minimizes data copying
	 * Traits:
	 * -There is no reason to really inherit from this class.
	 *
	 * Error Handling:
	 * -Exception based. The motivation is that if one error of any kind is detected
	 *  in a json input, the entire input is invalid. Therefore, only one try...catch
	 *  block is sufficient to wrap around the entire pull parsing process.
	 * -It is unnecessary to wrap individual Pull(), Get(), etc calls with try{}.
	 *  There are methods to detect types, keys, etc without throwing exceptions.
	 *  */
	class PullParser {
		public:
			typedef enum {
				/** @brief Begin state. */
				ST_BEGIN,

				/** @brief No more data to parse. Final state. */
				ST_NO_DATA,

				/** @brief After Pull(), Parser descended into a map. */
				ST_MAP,

				/** @brief After Pull(), Parser descended into a LIST. */
				ST_LIST,

				/** @brief After Pull(), Parser read new datum. Depth remains the same. */
				ST_DATUM,

				/** @brief Ascend out of map. */
				ST_ASCEND_MAP,

				/** @brief Ascend out of list. */
				ST_ASCEND_LIST
			} State;

			/** @brief Adapter class for pulling more input data. */
			class Reader {
				public:
					virtual ~Reader() throw();

					/** @brief Read data into the specified buffer
					 *  @param dest Where to store data.
					 *  @param len maximum amount of data to read.
					 *  @return number of bytes read; 0 if end, < 0 if error.
					 *  @throw none, errors should be recorded locally in this class
					 *  and examined later in the catch() block if necessary.
					 *  */
					virtual int Read(uint8_t* buff, unsigned len) throw() = 0;
			};

			/** @brief Exception type for syntax errors.
			 * Not used for errors thrown for failures from Reader::Read(). */
			class input_error : public virtual std::exception{
				public:
					input_error(const char* msg, unsigned file_offset);
					const char* what(void) const throw();

				private:
					char _msg[128];

					friend class PullParser;
			};

			/** @brief Exception type thrown by user; Provided for convenience.
			 * Use when value is correct JSON, but not expected during parsing. */
			class invalid_value : public virtual std::exception{
				public:
					/** @brief  */
					invalid_value(const char* msg, const PullParser& parser);
					const char* what(void) const throw();

				private:
					/** @brief Internal message buffer. */
					char _msg[256];
			};


			/* Ctor/dtor. */

			/** @brief Initialize with scratch space and input retrieval
			 *  @param maxdepth Maximum json depth to parse.  */
			PullParser(unsigned maxdepth, uint32_t* stack);

			~PullParser();


			/* Iteration operations. */

			/** @brief Prepare parser for iteration operations.
			 *  Does NOT Pull any values.
			 *  Allows instance reuse.
			 *  @param buffer Where to buffer character data during parsing session.
			 *  Do NOT mess with the contents of the buffer while parsing!
			 *  @param len Size of buffer
			 *  @param reader From where to pull data. */
			void Begin(uint8_t* buffer, unsigned len, Reader* reader) throw();

			/** @brief Pull next value
			 *  Calling Pull() invalidates values from a previous Pull() call.
			 *  @param key_set Lexigraphically sorted array of keys to match
			 *  @param key_set_length length of key_set
			 *  @return New parser state
			 *  @throw on parsing errors */
			State Pull(char const * const * key_set = NULL, unsigned key_set_length = 0);

			/** @brief Jump out of deepest depth map/list.
			 *  @return Context out of which left
			 *  (ST_ASCEND_MAP or ST_ASCEND_LIST) */
			State Up(void);


			/* Accessors. */

			/** @brief Read a chunk of UTF-8 multibyte characters into user buffer.
			 *  -Note this can advance internal parsing c_state.
			 *  -CONSEQUENTLY, THE KEY MAY DISAPPEAR AFTER FIRST CHUNKREAD CALL.
			 *   IF YOU NEED TO READ THE KEY, READ IT BEFORE CALLING THIS!
			 *   (you should identify the string before reading it anyway!)
			 *  -Return value is 0 when no more chars to read
			 *  -Only "whole" UTF-8 characters read.
			 *  @param dest Where to store key string. If NULL, then just verifies.
			 *  @param destlen Maximum size of destination. Must be >= 4.
			 *  @param key_enum Verify key idx matches enum val. Default no matching.
			 *  If omitted no key verification performed.
			 *  @return Number of bytes copied, excluding null terminator.
			 *  If 0, then no more chars in the value. Next call should be Pull()
			 *  @throw  type mismatch, key_enum mismatch
			 *  or destlen < 4 bytes in size. */
			unsigned ChunkRead8(char* dest, unsigned destlen,
				unsigned key_enum = 0xFFFFFFFF);

			/* TODO UTF-16, UTF-32 versions of ChunkRead8(). */

			/** @brief Get currently parsed value.
			 *  @throw When parser state is not ST_DATUM */
			const bnj_val& GetValue(void) const;

			/** @brief Retrieve current parser state.
			 * @return After Begin() and before first Pull() call, returns ST_BEGIN.
			 * Otherwise, identical to return value from Pull() */
			State GetState(void) const;

			/** @brief Convenience function to interpret state if parser descended.
			 *  @return true if parser descended after Pull(); false otherwise. */
			bool Descended(void) const;


			/* Utility functions. */

			/** @brief Get Reference to underlying C spj state. */
			const bnj_state& c_state(void) const;

			/** @brief Get Reference to underlying C user state. */
			const bnj_ctx& c_ctx(void) const;

			/** @brief Get pull parser depth.
			 *  @return Current depth in JSON data. */
			unsigned Depth(void) const;

			/** @brief Access to character buffer (same as buffer in Begin()).
			 *  AS OF THIS TIME, NOT FOR GENERIC USE! */
			const uint8_t* Buff(void) const;

			/** @brief Size of whole buffer.
			 *  AS OF THIS TIME, NOT FOR GENERIC USE! */
			unsigned BuffLen(void) const;

			/** @brief Indicate at what file offset value begins. */
			unsigned FileOffset(const bnj_val& v) const throw();

		private:

			/** @brief Private default ctor to prevent inheritance. */
			PullParser(void);

			/** @brief Copy ctor. */
			PullParser(const PullParser& p);

			/** @brief Fill up _buffer from beginning.
			 *  @param end_bound Offset to end of fill region.
			 *  @throw On input error. */
			void FillBuffer(unsigned end_bound);

			/** @brief Internal value buffer.
			 * Presently, 4 is arbitrarily chosen, may change in the future... */
			bnj_val _valbuff[4];

			/** @brief Current value index. */
			unsigned _val_idx;

			/** @brief Number of values in _valbuff. */
			unsigned _val_len;

			/** @brief Pull parser state. */
			State _parser_state;

			/** @brief Buffer scratch space. */
			uint8_t* _buffer;

			/** @brief Size of _buffer */
			unsigned _len;

			/** @brief Input reader. */
			Reader* _reader;

			/** @brief Pulling state. */
			unsigned _state;

			/** @brief Where current fragment ends. */
			unsigned _offset;

			/** @brief offset of first unparsed character. */
			unsigned _first_unparsed;

			/** @brief offset of first empty character. */
			unsigned _first_empty;

			/** @brief Current depth. */
			unsigned _depth;

			/** @brief Count of bytes parsed by bnj_parse(). */
			unsigned _total_parsed;

			/** @brief Count of bytes successfully Pulled(). */
			unsigned _total_pulled;

			/** @brief How many UTF-8 bytes remaining. */
			unsigned _utf8_remaining;

			/** @brief Holds current user keyset. */
			bnj_ctx _ctx;

			/** @brief Parsing state. */
			bnj_state _pstate;
	};


	/* Value accessors. These functions are intended to be one liner calls for:
	 * -Type verification
	 * -Value reading
	 * -Key verification (via enum values)
	 * -Throwing detailed error messages
	 *  */

	/** @brief Copy key from a (key:value) pair to destination buffer.
	 *  @param dest Where to store key string.
	 *  @param destlen Maximum size of destination.
	 *  @param p Parser instance.
	 *  @return Number of bytes copied, excluding null terminator.
	 *  @throw destlen < key length or not in a MAP context. */
	unsigned GetKey(char* dest, unsigned destlen, const PullParser& p);

	void Get(unsigned& dest, const PullParser& p, unsigned key_enum = 0xFFFFFFFF);

	void Get(int& dest, const PullParser& p, unsigned key_enum = 0xFFFFFFFF);

	void Get(float& dest, const PullParser& p, unsigned key_enum = 0xFFFFFFFF);

	void Get(double& dest, const PullParser& p, unsigned key_enum = 0xFFFFFFFF);

	void Get(bool& dest, const PullParser& p, unsigned key_enum = 0xFFFFFFFF);


	/* Context/Value verification. */

	void VerifyNull(const PullParser& p, unsigned key_enum = 0xFFFFFFFF);

	void VerifyList(const PullParser& p, unsigned key_enum = 0xFFFFFFFF);

	void VerifyMap(const PullParser& p, unsigned key_enum = 0xFFFFFFFF);
}

/* Inlines */

inline const char* BNJ::PullParser::input_error::what(void) const throw(){
	return _msg;
}

inline const char* BNJ::PullParser::invalid_value::what(void) const throw(){
	return _msg;
}

inline const bnj_val& BNJ::PullParser::GetValue(void) const{
	if(_val_idx >= _val_len)
		throw input_error("No valid parser value!", _total_pulled);
	return _valbuff[_val_idx];
}

inline BNJ::PullParser::State BNJ::PullParser::GetState(void) const{
	return _parser_state;
}

inline bool BNJ::PullParser::Descended(void) const{
	return (_parser_state == ST_MAP || _parser_state == ST_LIST);
}

inline const bnj_state& BNJ::PullParser::c_state(void) const{
	return _pstate;
}

inline const bnj_ctx& BNJ::PullParser::c_ctx(void) const{
	return _ctx;
}

inline unsigned BNJ::PullParser::Depth(void) const{
	return _depth;
}

inline const uint8_t* BNJ::PullParser::Buff(void) const{
	return _buffer + _offset;
}

inline unsigned BNJ::PullParser::BuffLen(void) const{
	return _len;
}

#endif
