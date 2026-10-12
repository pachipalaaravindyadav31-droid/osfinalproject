#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "../include/stats.h"

long long monotonic_ns(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
        return 0;

    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int write_all_counted(int fd,
                      const void *buffer,
                      size_t size,
                      long long *write_time_ns)
{
    const char *data = buffer;
    size_t total = 0;

    struct sigaction sa = {0};
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGPIPE, &sa, NULL) == -1)
        return -1;

    while (total < size)
    {
        long long start = monotonic_ns();

        ssize_t written = write(fd, data + total, size - total);

        long long end = monotonic_ns();

        if (write_time_ns != NULL && start && end >= start)
            *write_time_ns += end - start;

        if (written == -1)
        {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (written == 0)
        {
            errno = EIO;
            return -1;
        }

        total += (size_t)written;
    }

    return 0;
}

long long count_newlines(const char *buffer, size_t size)
{
    long long count = 0;

    for (size_t i = 0; i < size; i++)
    {
        if (buffer[i] == '\n')
            count++;
    }

    return count;
}

void report_stage_metrics(const char *name,
                          long long records,
                          long long elapsed_ns,
                          long long write_time_ns)
{
    double seconds = elapsed_ns / 1000000000.0;
    double rate = seconds > 0 ? records / seconds : 0;

    fprintf(stderr,
            "[Metrics] %s: records=%lld, records/sec=%.2f, "
            "write_time_ms=%.3f\n",
            name,
            records,
            rate,
            write_time_ns / 1000000.0);
}
