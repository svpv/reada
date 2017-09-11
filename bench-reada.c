/*
 * This program tests IO peformance of reading rpm headers from pkglist files,
 * using either of the three APIs: 1) read; 2) reada; 3) fread.

$ set /var/lib/apt/lists/*list.*
$ set $@ $@ $@
$ set $@ $@ $@
$ time ./a.out read $@
0.04s user 0.61s system 99% cpu 0.646 total
$ time ./a.out reada $@
0.03s user 0.53s system 99% cpu 0.561 total
$ time ./a.out fread $@
0.12s user 0.39s system 99% cpu 0.511 total

 * The result suggests that system calls might still be expensive, and that
 * stdio can still be faster - even despite double copying.  However, both
 * read and fread miss the feature which I need most: peeking a few bytes
 * ahead.
 *
 * Update: early implementations used a very small (16-byte) lookahead buffer.
 * The implementation since has been changed to use a big 8K buffer and to
 * extend reads to a 4K page boundary.  It now outperforms stdio.

$ time ./a.out reada $@
0.11s user 0.37s system 99% cpu 0.482 total

 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <endian.h>
#include "reada.h"

union fdu {
    int fd;
    struct fda *fda;
    FILE *fp;
};

static ssize_t xread_read(union fdu u, void *buf, size_t size)
{
    return read(u.fd, buf, size);
}

static ssize_t xread_reada(union fdu u, void *buf, size_t size)
{
    return reada(u.fda, buf, size);
}

static ssize_t xread_fread(union fdu u, void *buf, size_t size)
{
    return fread(buf, 1, size, u.fp);
}

int main(int argc, char **argv)
{
    assert(argc >= 3);
    ssize_t (*xread)(union fdu u, void *buf, size_t size);
    if (strcmp(argv[1], "read") == 0)
	xread = xread_read;
    else if (strcmp(argv[1], "reada") == 0)
	xread = xread_reada;
    else if (strcmp(argv[1], "fread") == 0)
	xread = xread_fread;
    else
	assert(!"mode");
    int nhdr = 0;
    for (int i = 2; i < argc; i++) {
	int fd = open(argv[i], O_RDONLY);
	assert(fd >= 0);
	char buf[NREADA];
	struct fda fda = { fd, buf };
	FILE *fp = fdopen(fd, "r");
	assert(fp);
	union fdu u;
	if (xread == xread_read)
	    u.fd = fd;
	else if (xread == xread_reada)
	    u.fda = &fda;
	else
	    u.fp = fp;
	while (1) {
	    unsigned w[4];
	    ssize_t n = xread(u, &w, sizeof w);
	    if (n == 0)
		break;
	    assert(n == 16);
	    assert(w[0] == be32toh(0x8eade801));
	    assert(w[1] == 0);
	    unsigned il = be32toh(w[2]);
	    unsigned dl = be32toh(w[3]);
	    size_t dataSize = 16 * il + dl;
	    assert(dataSize < (1<<20));
	    char a[dataSize];
	    n = xread(u, a, dataSize);
	    assert(n == dataSize);
#ifdef TEST_PEEKA
	    n = peeka(&fda, w + 1, 4);
	    assert((n == 0 && w[1] == 0) ||
		   (n == 4 && w[1] == be32toh(0x8eade801)));
#endif
	    nhdr++;
	}
	fclose(fp);
    }
    //fprintf(stderr, "nhdr=%d\n", nhdr);
    return 0;
}
