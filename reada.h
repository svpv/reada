#pragma once

#include <assert.h>
#include <sys/types.h> // ssize_t
#include <string.h>

// Raw buffer to be declared separately, no need to initialize.
#ifndef NREADA
#define NREADA 8192
#elif NREADA < 4096 || NREADA % 4096
#error "bad NREADA value"
#endif

// File descriptor with readahead, initialize with { fd, buf }.
struct fda {
    int fd;
    char *buf; // NREADA
    char *cur; // current offset into the buffer
    char *end; // how many bytes were read into the buffer
    off_t fpos; // file offset as seen by the OS
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

    // Hands up all those who believe that subtracting
    // two null pointers results in undefined behaviour.
    size_t left = fda->end - fda->cur;
    if (left >= size) {
	memcpy(buf, fda->cur, size);
	fda->cur += size;
	return size;
    }

    return reada_(fda, buf, size, left);
}

static inline
ssize_t peeka(struct fda *fda, void *buf, size_t size)
{
    assert(size > 0);

    size_t left = fda->end - fda->cur;
    if (left >= size) {
	memcpy(buf, fda->cur, size);
	return size;
    }

    return peeka_(fda, buf, size, left);
}

static inline
off_t tella(struct fda *fda)
{
    return fda->fpos - (fda->end - fda->cur);
}

#ifdef __cplusplus
}
#endif
