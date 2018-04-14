#include <string.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>
#include "reada.h"

ssize_t reada_(struct fda *fda, void *buf, size_t size, size_t left)
{
    // Inline functions check that the size is greater than 0.
    // Each "tail fuction" checks that the size is not too big.
    assert(size <= SSIZE_MAX);

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
	// And then up to NREADA bytes into fda->buf, to a page boundary.
	size_t endpos2 = (endpos1 + NREADA) & ~(size_t) 0xfff;
	size_t asize = endpos2 - endpos1;
	assert(asize > NREADA - 4096);
	assert(asize <= NREADA);
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

ssize_t peeka_(struct fda *fda, void *buf, size_t size, size_t left)
{
    assert(size <= NREADA);

    if (left && fda->cur > fda->buf) {
	memmove(fda->buf, fda->cur, left);
	fda->cur = fda->buf;
	fda->end = fda->buf + left;
    }

    do {
	// Can advance file position up to NREADA bytes.
	size_t endpos = (size_t) fda->fpos + NREADA - left;
	// Will read to a page boundary.
	endpos &= ~(size_t) 0xfff;
	size_t asize = endpos - (size_t) fda->fpos;
	assert(asize > 0);
	assert(left + asize <= NREADA);
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
    memcpy(buf, fda->buf, size);
    return size;
}

ssize_t skipa_(struct fda *fda, size_t size, size_t left)
{
    assert(size <= SSIZE_MAX);

    size_t total = 0;

    if (left) {
	size -= left;
	fda->cur = fda->end = NULL;
	total += left;
    }

    while (1) {
	// Can advance file position up to NREADA bytes.
	size_t endpos = (size_t) fda->fpos + NREADA;
	// Will read to a page boundary.
	endpos &= ~(size_t) 0xfff;
	size_t asize = endpos - (size_t) fda->fpos;
	assert(asize > 0);
	assert(asize <= NREADA);
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
