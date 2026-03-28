#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

void die(const char* ctx) {
    int e = errno;
    fprintf(stderr, "arg3pipe: %s: %s\n", ctx, strerror(e));
    exit(1);
}

int write_all(int fd, const void* buf, size_t n) {
    const char* p = (const char*)buf;
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(fd, p + off, n - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (w == 0) {
            errno = EIO;
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

void flush_buf(int fd, char* buf, size_t* used) {
    if (*used == 0) return;
    if (write_all(fd, buf, *used) < 0)
        die("write");
    *used = 0;
}

int main(int argc, char** argv) {
    int p[2];
    if (pipe(p) < 0) die("pipe");

    pid_t pid = fork();
    if (pid < 0) die("fork");

    if (pid == 0) {
        if (close(p[1]) < 0) die("child close write-end");

        char buf[16 * 1024];
        for (;;) {
            ssize_t r = read(p[0], buf, sizeof(buf));
            if (r < 0) {
                if (errno == EINTR) continue;
                die("child read");
            }
            if (r == 0) {
                 break;
            } 
            if (write_all(STDOUT_FILENO, buf, (size_t)r) < 0)
                die("child write");
        }

        if (close(p[0]) < 0) die("child close read-end");
        return 0;
    }

    if (close(p[0]) < 0) die("parent close read-end");

    char out[16 * 1024];
    size_t used = 0;

    for (int i = 1; i < argc; i++) {
        const char* s = argv[i];
        size_t n = strlen(s);

        size_t pos = 0;
        while (pos < n) {
            size_t space = sizeof(out) - used;
            if (space == 0) {
                flush_buf(p[1], out, &used);
                space = sizeof(out);
            }
            size_t take = n - pos;
            if (take > space) take = space;

            memcpy(out + used, s + pos, take);
            used += take;
            pos += take;
        }

        if (used == sizeof(out))
            flush_buf(p[1], out, &used);
        out[used++] = '\n';
    }

    flush_buf(p[1], out, &used);

    if (close(p[1]) < 0) die("parent close write-end");

    int st = 0;
    if (waitpid(pid, &st, 0) < 0) die("waitpid");

    if (WIFEXITED(st)) return WEXITSTATUS(st);
    if (WIFSIGNALED(st)) return 128 + WTERMSIG(st);
    return 1;
}