#ifndef __BENEGON_JSON_MEM_READER_HH__
#define __BENEGON_JSON_MEM_READER_HH__

#include "pull.hh"

namespace BNJ {

	/** @brief Adapts inmemory JSON data to Pull Parser interface.
	 * -Note that all json characters must be in contiguous memory!
	 * -Nicely pairs with mmap() files.
	 *
	 * -One penalty to using this adapter is that the JSON data is repeatedly
	 *  copied into the PullParser's buffer. To really solve this problem,
	 *  PullParser should be extended to handle in memory buffers.
	 *  But for now this class will do.
	 * */
	class MemReader : public BNJ::PullParser::Reader {
		public:

			/** @brief ctor
			 *  @param buff ALL JSON data to be parsed!
			 *  @param len Length of the buffer. */
			MemReader(const uint8_t* buff, unsigned len) throw();

			/** @brief dtor */
			~MemReader() throw();

			/** @brief Override. */
			int Read(uint8_t* buff, unsigned len) throw();

		private:
			/** @brief Pointer to all data. */
			const uint8_t* _buff;

			/** @brief Length of all data. */
			const unsigned _length;

			/** @brief Current pointer in buffer. */
			unsigned _pos;
	};
}

#endif
