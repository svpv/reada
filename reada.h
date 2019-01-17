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

#pragma once

#include <string.h>

#ifdef READA_DEBUG
#include <assert.h>
#define RA_ASSERT(cond) assert(cond)
#else
#define RA_ASSERT(cond) ((void)0)
#endif

#ifdef __GNUC__
#define RA_INLINE static inline __attribute__((always_inline))
#else
#define RA_INLINE static inline
#endif

// Raw buffer to be declared separately, no need to initialize.
#define BUFSIZA 8192

// File descriptor with readahead, initialize with { fd, buf }.
struct fda {
    // The underlying file descriptor.
    int fd;
    // The underlying buffer, BUFSIZA bytes.
    char *buf;
    // Current position / offset into the buffer.
    char *cur;
    // How many bytes are left in the buffer, starting with cur.
    size_t fill;
    // File offset as seen by the OS.
    off_t fpos;
};

#ifdef __cplusplus
extern "C" {
#endif

// Not to be re-exported on behalf of a shared library.
#pragma GCC visibility push(hidden)

size_t reada_(struct fda *fda, void *buf, size_t size);
size_t filla_(struct fda *fda, size_t size);
size_t skipa_(struct fda *fda, size_t size);

RA_INLINE size_t reada(struct fda *fda, void *buf, size_t size)
{
    RA_ASSERT(size > 0);

    if (fda->fill >= size) {
	memcpy(buf, fda->cur, size);
	fda->cur += size, fda->fill -= size;
	return size;
    }

    return reada_(fda, buf, size);
}

// How many bytes can the buffer currently harbor?  This may be
// less than BUFSIZA, because we only read to the page boundary,
// so one must not assume that the whole buffer can be filled.
size_t maxfilla(struct fda *fda);

// Try to fill the buffer with (at least) size bytes, returns <= size.
RA_INLINE size_t filla(struct fda *fda, size_t size)
{
    RA_ASSERT(size > 0);

    if (fda->fill >= size)
	return size;

    return filla_(fda, size);
}

RA_INLINE size_t peeka(struct fda *fda, void *buf, size_t size)
{
    RA_ASSERT(size > 0);

    if (fda->fill >= size) {
	memcpy(buf, fda->cur, size);
	return size;
    }

    size_t fill = filla_(fda, size);
    if (fill > 0 && fill != (size_t) -1)
	memcpy(buf, fda->cur, fill);
    return fill;
}

RA_INLINE size_t skipa(struct fda *fda, size_t size)
{
    RA_ASSERT(size > 0);

    if (fda->fill >= size) {
	fda->cur += size, fda->fill -= size;
	return size;
    }

    return skipa_(fda, size);
}

RA_INLINE off_t tella(struct fda *fda)
{
    return fda->fpos - fda->fill;
}

off_t setposa(struct fda *fda, off_t off);

#pragma GCC visibility pop

#ifdef __cplusplus
}
#endif
