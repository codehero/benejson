/* POSIX compatible classes.
 * These are strictly REFERENCE implementations and not meant for production use.
 * The user should write adapters for their favorite portable I/O library.*/
#ifndef __BENEGON_BENEJSON_PULL_POSIX_HH__
#define __BENEGON_BENEJSON_PULL_POSIX_HH__

#include <benejson/pull.hh>

/** @brief Read character data directly from a fd. */
class FD_Reader: public BNJ::PullParser::Reader {
	public:
		/** @brief ctor, initialize with file descriptor.
		 *  @param fd Open file descriptor. See dtor for more details. */
		FD_Reader(int fd) throw();

		/** @brief dtor. Note that this class closes the fd passed to it. */
		~FD_Reader() throw();

		/** @brief If error occurs, retrieve what happend. */
		int Errno(void) const throw();

		/** @brief Override. Note reading from closed fd returns 0! */
		int Read(uint8_t* buff, unsigned len) throw();
	
	private:
		/** @brief Fd from which we read. */
		int _fd;

		/** @brief Record error if it occurred. */
		int _errno;
};

/* Inlines. */

inline int FD_Reader::Errno(void) const throw() {
	return _errno;
}

#endif
