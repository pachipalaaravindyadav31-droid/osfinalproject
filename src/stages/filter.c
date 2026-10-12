#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "../../include/stats.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: filter PATTERN\n");
        return 2;
    }

    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;

    long long records = 0;
    long long write_time = 0;
    long long start = monotonic_ns();
    int status = 0;

    while ((length = getline(&line, &capacity, stdin)) != -1)
    {
        if (strstr(line, argv[1]) == NULL)
            continue;

        if (write_all_counted(STDOUT_FILENO,
                              line,
                              (size_t)length,
                              &write_time) == -1)
        {
            if (errno == EPIPE)
            {
                fprintf(stderr,
                        "Filter: Consumer closed pipe; stopping gracefully\n");
                break;
            }

            perror("Filter: write");
            status = 1;
            break;
        }

        records++;
    }

    if (ferror(stdin))
    {
        perror("Filter: read");
        status = 1;
    }

    free(line);

    report_stage_metrics("Filter",
                         records,
                         monotonic_ns() - start,
                         write_time);

    return status;
}
