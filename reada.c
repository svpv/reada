#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>
#include "reada.h"

ssize_t reada(struct fda *fda, void *buf, size_t size)
{
    assert(size > 0);

    size_t total = 0;

    size_t left = fda->fill - fda->off;
    if (left) {
	if (size < left) {
	    memcpy(buf, fda->buf + fda->off, size);
	    fda->off += size;
	    return size;
	}
	memcpy(buf, fda->buf + fda->off, left);
	size -= left, buf = (char *) buf + left;
	fda->off = fda->fill = 0;
	total += left;
    }

    while (size) {
	size_t endpos = (fda->fpos + size + NREADA) / 4096 * 4096;
	size_t asize = endpos - (fda->fpos + size);
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
	    break;
	fda->fpos += n;
	if ((size_t) n > size) {
	    fda->fill = n - size;
	    n = size;
	}
	size -= n, buf = (char *) buf + n;
	total += n;
    }

    return total;
}

ssize_t peeka(struct fda *fda, void *buf, size_t size)
{
    assert(size > 0);
    assert(size <= NREADA);

    size_t left = fda->fill - fda->off;
    if (left < size) {
	if (fda->off) {
	    memmove(fda->buf, fda->buf + fda->off, left);
	    fda->off = 0, fda->fill = left;
	}
	do {
	    size_t endpos = (fda->fpos + NREADA - fda->fill) / 4096 * 4096;
	    size_t asize = endpos - fda->fpos;
	    assert(asize > 0);
	    assert(fda->fill + asize <= NREADA);
	    ssize_t n;
	    do
		n = read(fda->fd, fda->buf + fda->fill, asize);
	    while (n < 0 && errno == EINTR);
	    if (n < 0)
		return n;
	    if (n == 0)
		break;
	    fda->fpos += n;
	    fda->fill += n, left += n;
	} while (left < size);
    }

    if (left < size)
	size = left;
    memcpy(buf, fda->buf + fda->off, size);
    return size;
}
