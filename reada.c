// Copyright (c) 2017, 2018 Alexey Tourbin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>
#include "reada.h"

size_t reada_(struct fda *fda, char *buf, size_t size)
{
    // We are only called if the buffer is not full enough to satisfy
    // the request, so taking it all.
    size_t total = fda->fill;
    if (fda->fill) {
	memcpy(buf, fda->cur, fda->fill);
	buf += fda->fill, size -= fda->fill,
	fda->cur += fda->fill, fda->fill = 0;
    }

    if (fda->eof || fda->err)
	return total;

    while (1) {
	size_t asize;
	if (fda->ispipe)
	    asize = BUFSIZA;
	else {
	    // File position after reading size bytes into the caller's buffer.
	    size_t endpos1 = (size_t) fda->fpos + size;
	    // And then up to BUFSIZA bytes into fda->buf, to a page boundary.
	    size_t endpos2 = (endpos1 + BUFSIZA) & ~(size_t) 0xfff;
	    asize = endpos2 - endpos1;
	    RA_ASSERT(asize > BUFSIZA - 4096);
	    RA_ASSERT(asize <= BUFSIZA);
	}
	struct iovec iov[2] = {
	    { buf, size },
	    { fda->buf, asize },
	};
	ssize_t n;
	do
	    n = readv(fda->fd, iov, 2);
	while (n < 0 && errno == EINTR);
	if (n <= 0) {
	    if (n == 0)
		fda->eof = true;
	    else
		fda->err = errno;
	    return total;
	}
	fda->fpos += n;
	fda->cur = fda->buf; // no going back in setposa
	if ((size_t) n >= size) {
	    fda->fill = n - size;
	    return total + size;
	}
	buf += n, size -= n,
	total += n;
    }
}

// How many bytes can we read ahead?  (Unsigned mod size_t
// arithmetic should work just fine with large offsets.)
static inline size_t rasize(size_t fill, size_t fpos)
{
    // Can advance file position up to BUFSIZA bytes,
    // but also need to keep the bytes that are left.
    size_t endpos = fpos + BUFSIZA - fill;
    // Will read to a page boundary.
    endpos &= ~(size_t) 0xfff;

    size_t asize = endpos - fpos;
    RA_ASSERT(fill + asize <= BUFSIZA);
    return asize;
}

size_t maxfilla(struct fda *fda)
{
    return fda->fill + rasize(fda->fill, fda->fpos);
}

size_t filla_(struct fda *fda, size_t size)
{
    RA_ASSERT(size <= BUFSIZA);

    if (fda->fill && fda->cur > fda->buf)
	memmove(fda->buf, fda->cur, fda->fill);
    fda->cur = fda->buf;

    do {
	size_t asize = rasize(fda->fill, fda->fpos);
	RA_ASSERT(asize > 0);
	ssize_t n;
	do
	    n = read(fda->fd, fda->buf + fda->fill, asize);
	while (n < 0 && errno == EINTR);
	if (n < 0)
	    return (size_t) -1;
	if (n == 0)
	    break;
	fda->fill += n;
	fda->fpos += n;
    } while (fda->fill < size);

    if (size > fda->fill)
	size = fda->fill;
    return size;
}

size_t skipa_(struct fda *fda, size_t size)
{
    RA_ASSERT(size != (size_t) -1);

    size_t total = fda->fill;
    if (fda->fill) {
	size -= fda->fill;
	fda->fill = 0;
    }

    while (1) {
	// Can advance file position up to BUFSIZA bytes.
	size_t endpos = (size_t) fda->fpos + BUFSIZA;
	// Will read to a page boundary.
	endpos &= ~(size_t) 0xfff;
	size_t asize = endpos - (size_t) fda->fpos;
	RA_ASSERT(asize > 0);
	RA_ASSERT(asize <= BUFSIZA);
	ssize_t n;
	do
	    n = read(fda->fd, fda->buf, asize);
	while (n < 0 && errno == EINTR);
	if (n < 0)
	    return (size_t) -1;
	if (n == 0)
	    return total;
	fda->fpos += n;
	if ((size_t) n >= size) {
	    fda->cur = fda->buf + size;
	    fda->fill = n - size;
	    return total + size;
	}
	size -= n;
	total += n;
    }
}

bool setposa(struct fda *fda, uint64_t off)
{
    // The real offset, as seen by the OS, is fda->fpos.
    // But we also have a buffer which may take us a few bytes back.
    // So within this range, the position can be changed by simply
    // adjusting fda->cur, without actually calling lseek(2).
    uint64_t hi = fda->fpos;
    if (off <= hi) {
	// How many bytes can we go back?  This assumes that the data
	// in the buf..cur area is still valid.
	uint64_t back = fda->cur ? fda->cur - fda->buf + fda->fill : 0;
	uint64_t lo = hi - back;
	if (off >= lo) {
	    off_t curoff = hi - fda->fill;
	    ssize_t delta = off - curoff;
	    fda->cur += delta, fda->fill -= delta;
	    return true;
	}
    }
    int64_t ret = lseek(fda->fd, off, SEEK_SET);
    // No sticky fda->err, because fda->state is still valid, and the internal
    // state is consistent.  This is unlike read(2) which leaves unspecified
    // whether the file position changes.
    if (ret < 0)
	return false;
    fda->fpos = ret;
    fda->cur = NULL, fda->fill = 0;
    return fda->fpos == off;
}
