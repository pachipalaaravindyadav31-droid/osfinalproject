#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "../../include/stats.h"

int main(int argc, char *argv[])
{
    if (argc != 2 || strcmp(argv[1], "uppercase") != 0)
    {
        fprintf(stderr, "Usage: transform uppercase\n");
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
        for (ssize_t i = 0; i < length; i++)
        {
            unsigned char ch = (unsigned char)line[i];
            line[i] = (char)toupper(ch);
        }

        if (write_all_counted(STDOUT_FILENO,
                              line,
                              (size_t)length,
                              &write_time) == -1)
        {
            if (errno == EPIPE)
            {
                fprintf(stderr,
                        "Transform: Consumer closed pipe; stopping gracefully\n");
                break;
            }

            perror("Transform: write");
            status = 1;
            break;
        }

        records++;
    }

    if (ferror(stdin))
    {
        perror("Transform: read");
        status = 1;
    }

    free(line);

    report_stage_metrics("Transform",
                         records,
                         monotonic_ns() - start,
                         write_time);

    return status;
}
