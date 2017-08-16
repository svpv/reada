#ifndef READA_H
#define READA_H

#include <sys/types.h> // ssize_t

#ifndef NREADA
#define NREADA 16
#endif

struct fda {
    int fd;
    size_t fill;
    size_t off;
    char buf[NREADA];
};

#ifdef __cplusplus
extern "C" {
#endif

ssize_t reada(struct fda *fda, void *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif
