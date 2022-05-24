#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <pulse/error.h>
#include <pulse/simple.h>

char *read_file(const char *file_path, size_t *size)
{
    const int fd = open(file_path, O_RDONLY);
    if (fd < 0) goto error;

    struct stat statbuf;
    if (fstat(fd, &statbuf) < 0) goto error;

    char *data = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) goto error;

    *size = statbuf.st_size;
    return data;

error:
    fprintf(stderr, "Error: could not read file '%s': %s\n", file_path, strerror(errno));
    exit(1);
}

static const pa_sample_spec spec = {
    .format = PA_SAMPLE_S16LE,
    .rate = 44100,
    .channels = 2
};

static int pa_errno;

void pcc(int code)
{
    if (code < 0) {
        fprintf(stderr, "[Pulse Error] %s\n", pa_strerror(pa_errno));
        exit(1);
    }
}

void *pcp(void *ptr)
{
    if (!ptr) {
        fprintf(stderr, "[Pulse Error] %s\n", pa_strerror(pa_errno));
        exit(1);
    }
    return ptr;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: smm FILE\n");
        fprintf(stderr, "Error: insufficient arguments\n");
        exit(1);
    }

    pa_simple *sink = pcp(pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &spec, NULL, NULL, &pa_errno));

    size_t size;
    char *data = read_file(argv[1], &size);
    pcc(pa_simple_write(sink, data, size, &pa_errno));
    munmap(data, size);

    pcc(pa_simple_drain(sink, &pa_errno));
    pa_simple_free(sink);
    return 0;
}
