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

ssize_t reada_(struct fda *fda, void *buf, size_t size, size_t left)
{
    // Inline functions check that the size is greater than 0.
    // Each "tail fuction" checks that the size is not too big.
    RA_ASSERT(size <= SSIZE_MAX);

    size_t total = 0;

    if (left) {
	memcpy(buf, fda->cur, left);
	size -= left, buf = (char *) buf + left;
	fda->cur = fda->end = NULL; // in case read fails
	total += left;
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
	    return n;
	if (n == 0)
	    return total;
	fda->fpos += n;
	if ((size_t) n >= size) {
	    fda->cur = fda->buf;
	    fda->end = fda->buf + (n - size);
	    return total + size;
	}
	size -= n, buf = (char *) buf + n;
	total += n;
    }
}

// How many bytes can we read ahead?  (Unsigned mod size_t
// arithmetic should work just fine with large offsets.)
static inline size_t rasize(size_t left, size_t fpos)
{
    // Can advance file position up to BUFSIZA bytes,
    // but also need to keep the bytes that are left.
    size_t endpos = fpos + BUFSIZA - left;
    // Will read to a page boundary.
    endpos &= ~(size_t) 0xfff;

    size_t asize = endpos - fpos;
    RA_ASSERT(left + asize <= BUFSIZA);
    return asize;
}

size_t maxfilla(struct fda *fda)
{
    size_t left = fda->end - fda->cur;
    return left + rasize(left, fda->fpos);
}

ssize_t filla_(struct fda *fda, size_t size, size_t left)
{
    RA_ASSERT(size <= BUFSIZA);

    if (left && fda->cur > fda->buf) {
	memmove(fda->buf, fda->cur, left);
	fda->cur = fda->buf;
	fda->end = fda->buf + left;
    }

    do {
	size_t asize = rasize(left, fda->fpos);
	RA_ASSERT(asize > 0);
	ssize_t n;
	do
	    n = read(fda->fd, fda->buf + left, asize);
	while (n < 0 && errno == EINTR);
	if (n < 0)
	    return n;
	if (n == 0)
	    break;
	left += n;
	fda->fpos += n;
	fda->cur = fda->buf;
	fda->end = fda->buf + left;
    } while (left < size);

    if (left < size)
	size = left;
    return size;
}

ssize_t skipa_(struct fda *fda, size_t size, size_t left)
{
    RA_ASSERT(size <= SSIZE_MAX);

    size_t total = 0;

    if (left) {
	size -= left;
	fda->cur = fda->end = NULL;
	total += left;
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
	    return n;
	if (n == 0)
	    return total;
	fda->fpos += n;
	if ((size_t) n >= size) {
	    fda->cur = fda->buf + size;
	    fda->end = fda->buf + n;
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
	// How many bytes can we go back?
	off_t back = fda->end ? fda->end - fda->buf : 0;
	off_t lo = hi - back;
	if (off >= lo) {
	    // How many bytes do we need to go back?
	    back = hi - off;
	    fda->cur = fda->end - back;
	    return fda->fpos - back;
	}
    }

    off_t ret = lseek(fda->fd, off, SEEK_SET);
    if (ret >= 0) {
	fda->fpos = ret;
	fda->cur = fda->end = NULL;
    }
    return ret;
}
