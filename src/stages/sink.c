#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "../../include/stats.h"

static void optional_delay(void)
{
    const char *value = getenv("PIPEFLOW_SINK_DELAY_MS");

    if (value == NULL)
        return;

    char *end = NULL;
    long milliseconds = strtol(value, &end, 10);

    if (*value == '\0' || *end != '\0' ||
        milliseconds < 0 || milliseconds > 60000)
        return;

    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L
    };

    while (nanosleep(&delay, &delay) == -1 && errno == EINTR)
    {
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: sink OUTPUT_FILE\n");
        return 2;
    }

    int output_fd = open(argv[1],
                         O_WRONLY | O_CREAT | O_TRUNC,
                         0644);

    if (output_fd == -1)
    {
        perror("Sink: open");
        return 1;
    }

    char buffer[4096];
    long long records = 0;
    long long write_time = 0;
    long long start = monotonic_ns();
    int status = 0;

    for (;;)
    {
        ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (n == 0)
            break;

        if (n == -1)
        {
            if (errno == EINTR)
                continue;

            perror("Sink: read");
            status = 1;
            break;
        }

        records += count_newlines(buffer, (size_t)n);

        if (write_all_counted(output_fd,
                              buffer,
                              (size_t)n,
                              &write_time) == -1)
        {
            perror("Sink: write");
            status = 1;
            break;
        }

        /*
         * Test-only delay. It makes the sink consume data more slowly.
         */
        optional_delay();
    }

    if (close(output_fd) == -1)
    {
        perror("Sink: close");
        status = 1;
    }

    report_stage_metrics("Sink",
                         records,
                         monotonic_ns() - start,
                         write_time);

    return status;
}
