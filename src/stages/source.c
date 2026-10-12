#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "../../include/stats.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: source INPUT_FILE\n");
        return 2;
    }

    FILE *file = fopen(argv[1], "r");

    if (file == NULL)
    {
        perror("Source: fopen");
        return 1;
    }

    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;

    long long records = 0;
    long long write_time = 0;
    long long start = monotonic_ns();
    int status = 0;

    while ((length = getline(&line, &capacity, file)) != -1)
    {
        if (write_all_counted(STDOUT_FILENO,
                              line,
                              (size_t)length,
                              &write_time) == -1)
        {
            if (errno == EPIPE)
            {
                fprintf(stderr,
                        "Source: Consumer closed pipe; stopping gracefully\n");
                break;
            }

            perror("Source: write");
            status = 1;
            break;
        }

        records++;
    }

    if (ferror(file))
    {
        perror("Source: read");
        status = 1;
    }

    free(line);
    fclose(file);

    report_stage_metrics("Source",
                         records,
                         monotonic_ns() - start,
                         write_time);

    return status;
}
