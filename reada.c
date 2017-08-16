#include <string.h>
#include <assert.h>
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
	struct iovec iov[2] = {
	    { .iov_base = buf, .iov_len = size },
	    { .iov_base = fda->buf, .iov_len = NREADA },
	};
	ssize_t n;
	do
	    n = readv(fda->fd, iov, 2);
	while (n < 0 && errno == EINTR);
	if (n < 0)
	    return n;
	if (n == 0)
	    break;
	if ((size_t) n > size) {
	    fda->fill = n - size;
	    n = size;
	}
	size -= n, buf = (char *) buf + n;
	total += n;
    }

    return total;
}
