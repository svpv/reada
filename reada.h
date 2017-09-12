#ifndef READA_H
#define READA_H

#include <assert.h>
#include <sys/types.h> // ssize_t
#include <string.h>

// Raw buffer to be declared separately, no need to initialize.
#define NREADA 8192

// File descriptor with readahead, initialize with { fd, buf }.
struct fda {
    int fd;
    char *buf; // NREADA
    size_t fill; // how many bytes were read into the buffer
    size_t off; // current offset into the buffer (some bytes consumed)
    size_t fpos; // file offset as seen by the OS (for page boundary)
};

#ifdef __cplusplus
extern "C" {
#endif

ssize_t reada_(struct fda *fda, void *buf, size_t size, size_t left);
ssize_t peeka_(struct fda *fda, void *buf, size_t size, size_t left);

static inline
ssize_t reada(struct fda *fda, void *buf, size_t size)
{
    assert(size > 0);

    size_t left = fda->fill - fda->off;
    if (left >= size) {
	memcpy(buf, fda->buf + fda->off, size);
	if (left == size)
	    fda->off = fda->fill = 0;
	else
	    fda->off += size;
	return size;
    }

    return reada_(fda, buf, size, left);
}

static inline
ssize_t peeka(struct fda *fda, void *buf, size_t size)
{
    assert(size > 0);

    size_t left = fda->fill - fda->off;
    if (left >= size) {
	memcpy(buf, fda->buf + fda->off, size);
	return size;
    }

    return peeka_(fda, buf, size, left);
}

#ifdef __cplusplus
}
#endif

#endif
