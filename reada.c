#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>
#include "reada.h"

ssize_t reada_(struct fda *fda, void *buf, size_t size, size_t left)
{
    size_t total = 0;

    if (left) {
	memcpy(buf, fda->cur, left);
	size -= left, buf = (char *) buf + left;
	fda->cur = fda->end = NULL;
	total += left;
    }

    do {
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
	    fda->cur = fda->buf;
	    fda->end = fda->buf + (n - size);
	    n = size;
	}
	size -= n, buf = (char *) buf + n;
	total += n;
    } while (size);

    return total;
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
	size_t endpos = (fda->fpos + NREADA - left) / 4096 * 4096;
	size_t asize = endpos - fda->fpos;
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
