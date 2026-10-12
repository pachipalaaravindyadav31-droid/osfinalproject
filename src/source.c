#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "../include/source.h"

void run_source(int write_fd)
{
    int input_fd;
    char buffer[1024];
    ssize_t bytes_read;
    ssize_t bytes_written;

    input_fd = open("data/input.txt", O_RDONLY);

    if (input_fd == -1)
    {
        perror("Source: Unable to open input.txt");
        close(write_fd);
        return;
    }

    while ((bytes_read = read(input_fd, buffer, sizeof(buffer))) > 0)
    {
        bytes_written = write(write_fd, buffer, bytes_read);

        if (bytes_written == -1)
        {
            perror("Source: Failed to write to pipe");
            break;
        }

        if (bytes_written != bytes_read)
        {
            fprintf(stderr, "Source: Incomplete data written to pipe\n");
            break;
        }
    }

    if (bytes_read == -1)
    {
        perror("Source: Failed to read input.txt");
    }

    close(input_fd);
    close(write_fd);

    printf("Source: Input sent to Filter\n");
}
