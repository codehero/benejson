/* Copyright (c) 2010 David Bender assigned to Benegon Enterprises LLC
 * See the file LICENSE for full license information. */

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

				/** @brief No more data to parse. End state. */
				ST_NO_DATA,

				/** @brief After Pull(), Parser descended into a map. */
				ST_MAP,

				/** @brief After Pull(), Parser descended into a LIST. */
				ST_LIST,

				/** @brief After Pull(), Parser read new datum. Depth remains the same. */
				ST_DATUM,

				/** @brief After Pull(), Parser read string whose size exceeded buffer
				 * size. The . */
				ST_LONG,

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

			/** @brief Exception type thrown by PullParser and company.
			 * Not used for errors thrown for failures from Reader::Read(). */
			class input_error : public virtual std::runtime_error {
				public:
					input_error(const char* msg, unsigned offset);

					const unsigned file_offset;
			};

			/** @brief Initialize with scratch space and input retrieval
			 *  @param maxdepth Maximum json depth to parse.  */
			PullParser(unsigned maxdepth, uint32_t* stack);

			~PullParser();


			/* Accessors. */

			/** @brief Prepare parser for iteration operations. Allows instance reuse.
			 *  @param buffer Where to buffer character data.
			 *  @param len Size of buffer
			 *  @param reader From where to pull data. */
			void Begin(uint8_t* buffer, unsigned len, Reader* reader) throw();

			/** @brief Access to character buffer (same as buffer in Begin()). */
			const uint8_t* Buff(void) const;

			/** @brief Size of whole buffer. */
			unsigned BuffLen(void) const;

			/** @brief Get currently parsed value.
			 *  @throw If parser does not currently hold a valid value. */
			const bnj_val& GetValue(void) const;

			/** @brief Retrieve current parser state. */
			State GetState(void) const;

			/** @brief Get Reference to underlying C spj state. */
			const bnj_state& c_state(void) const;

			/** @brief Indicate at what file offset value begins. */
			unsigned FileOffset(const bnj_val& v) const throw();

			/** @brief Get pull parser depth. */
			unsigned Depth(void) const;

			/* Iteration operations. */

			/** @brief Read next value
			 *  Calling Pull() invalidates values from a previous Pull() call.
			 *  @param key_set Lexigraphically sorted array of keys to match
			 *  @param key_set_length length of key_set
			 *  @return New parser state
			 *  @throw on parsing errors */
			State Pull(char const * const * key_set = NULL, unsigned key_set_length = 0);

			/** @brief Skip over all remaining data in a context until depth rises by 1.
			 *  @return 1 if ascended; 0 if at end of parsing. */
			void Up(void);


		private:
			enum {
				/** @brief Filling the buffer. */
				READ_ST,

				/** @brief Parsing character data in buffer. */
				PARSE_ST
			};

			/** @brief Private default ctor to prevent inheritance. */
			PullParser(void);

			/** @brief Copy ctor. */
			PullParser(const PullParser& p);

			/** @brief Internal value buffer. */
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

			/** @brief How many bytes have been pull parsed. */
			unsigned _pull_parsed;

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

	/** @brief Consume string value into user buffer.
	 *  Note this can change internal parsing state.
	 *  @param dest Where to store key string. If NULL, then just verifies.
	 *  @param destlen Maximum size of destination.
	 *  @param p Parser instance.
	 *  @param key_enum Verify key idx matches enum value. Default no matching.
	 *  If omitted no key verification performed.
	 *  @return Number of bytes copied, excluding null terminator.
	 *  @throw destlen <= string length or type mismatch or key_enum mismatch. */
	unsigned Consume(char* dest, unsigned destlen, const PullParser& p,
		unsigned key_enum = 0xFFFFFFFF);

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

inline const uint8_t* BNJ::PullParser::Buff(void) const{
	return _buffer + _offset;
}

inline unsigned BNJ::PullParser::BuffLen(void) const{
	return _len;
}

inline const bnj_val& BNJ::PullParser::GetValue(void) const{
	if(_val_idx >= _val_len)
		throw std::runtime_error("No valid parser value!");
	return _valbuff[_val_idx];
}

inline BNJ::PullParser::State BNJ::PullParser::GetState(void) const{
	return _parser_state;
}

inline const bnj_state& BNJ::PullParser::c_state(void) const{
	return _pstate;
}

inline unsigned BNJ::PullParser::Depth(void) const{
	return _depth;
}

#endif
