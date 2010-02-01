/* Copyright (c) 2010 David Bender assigned to Benegon Enterprises LLC
 * See the file LICENSE for full license information. */

#include <errno.h>
#include "posix.hh"

FD_Reader::FD_Reader(int fd) throw()
	: _fd(fd), _errno(0)
{
}

FD_Reader::~FD_Reader() throw(){
	/* Assume file is read only; don't really care about successful close. */
	if(-1 != _fd)
		close(_fd);
}

int FD_Reader::Read(uint8_t* buff, unsigned len) throw(){
	/* If file closed just return 0. */
	if(-1 == _fd)
		return 0;

	/* BLOCKING read until data comes or unrecoverable error. */
	while(true){
		int ret = read(_fd, buff, len);
		if(ret < 0){
			if(EINTR == errno)
				continue;
			_errno = errno;
			close(_fd);
			_fd = -1;
		}
		else if(0 == ret){
			close(_fd);
			_fd = -1;
		}
		return ret;
	}
}
