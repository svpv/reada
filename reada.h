#ifndef READA_H
#define READA_H

#include <sys/types.h> // ssize_t

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

ssize_t reada(struct fda *fda, void *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif
