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

// Not to be re-exported on behalf of a shared library.
#pragma GCC visibility push(hidden)

ssize_t reada_(struct fda *fda, void *buf, size_t size, size_t left);
ssize_t filla_(struct fda *fda, size_t size, size_t left);
ssize_t skipa_(struct fda *fda, size_t size, size_t left);

static inline __attribute__((always_inline))
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

// How many bytes can the buffer currently harbor?  This may be
// less than NREADA, because we only read to the page boundary,
// so one must not assume that the whole buffer can be filled.
size_t maxfilla(struct fda *fda);

// Try to fill the buffer with (at least) size bytes, returns <= size.
static inline __attribute__((always_inline))
ssize_t filla(struct fda *fda, size_t size)
{
    assert(size > 0);

    size_t left = fda->end - fda->cur;
    if (left >= size)
	return size;

    return filla_(fda, size, left);
}

static inline __attribute__((always_inline))
ssize_t peeka(struct fda *fda, void *buf, size_t size)
{
    assert(size > 0);

    size_t left = fda->end - fda->cur;
    if (left >= size) {
	memcpy(buf, fda->cur, size);
	return size;
    }

    ssize_t fill = filla_(fda, size, left);
    if (fill > 0)
	memcpy(buf, fda->cur, fill);
    return fill;
}

static inline __attribute__((always_inline))
ssize_t skipa(struct fda *fda, size_t size)
{
    assert(size > 0);

    size_t left = fda->end - fda->cur;
    if (left >= size) {
	fda->cur += size;
	return size;
    }

    return skipa_(fda, size, left);
}

static inline __attribute__((always_inline))
off_t tella(struct fda *fda)
{
    return fda->fpos - (fda->end - fda->cur);
}

off_t setposa(struct fda *fda, off_t off);

#pragma GCC visibility pop

#ifdef __cplusplus
}
#endif
