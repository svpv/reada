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

size_t reada_(struct fda *fda, void *buf, size_t size)
{
    // Inline functions check that the size is greater than 0.
    // Each "tail fuction" checks that the size is not too big.
    RA_ASSERT(size != (size_t) -1);

    // We are only called if the buffer is not full enough to satisfy
    // the request, so taking it all.
    size_t total = fda->fill;
    if (fda->fill) {
	memcpy(buf, fda->cur, fda->fill);
	size -= fda->fill, buf = (char *) buf + fda->fill;
	fda->cur = NULL, fda->fill = 0; // in case read fails
    }

    while (1) {
	// File position after reading size bytes into the caller's buffer.
	size_t endpos1 = (size_t) fda->fpos + size;
	// And then up to BUFSIZA bytes into fda->buf, to a page boundary.
	size_t endpos2 = (endpos1 + BUFSIZA) & ~(size_t) 0xfff;
	size_t asize = endpos2 - endpos1;
	RA_ASSERT(asize > BUFSIZA - 4096);
	RA_ASSERT(asize <= BUFSIZA);
	struct iovec iov[2] = {
	    { .iov_base = buf, .iov_len = size },
	    { .iov_base = fda->buf, .iov_len = asize },
	};
	ssize_t n;
	do
	    n = readv(fda->fd, iov, 2);
	while (n < 0 && errno == EINTR);
	if (n < 0)
	    return (size_t) -1;
	if (n == 0)
	    return total;
	fda->fpos += n;
	if ((size_t) n >= size) {
	    fda->cur = fda->buf;
	    fda->fill = n - size;
	    return total + size;
	}
	size -= n, buf = (char *) buf + n;
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

off_t setposa(struct fda *fda, off_t off)
{
    // The real offset, as seen by the OS, is fda->fpos.
    // But we also have a buffer which may take us a few bytes back.
    // So within this range, the position can be changed by simply
    // adjusting fda->cur, without actually calling lseek(2).
    off_t hi = fda->fpos;
    if (off <= hi) {
	// How many bytes can we go back?  This assumes that the data
	// in the buf..cur area is still valid.
	off_t back = fda->cur ? fda->cur - fda->buf + fda->fill : 0;
	off_t lo = hi - back;
	if (off >= lo) {
	    off_t curoff = hi - fda->fill;
	    ssize_t delta = off - curoff;
	    fda->cur += delta, fda->fill -= delta;
	    return off;
	}
    }

    off_t ret = lseek(fda->fd, off, SEEK_SET);
    if (ret >= 0) {
	fda->fpos = ret;
	fda->cur = NULL, fda->fill = 0;
    }
    return ret;
}
