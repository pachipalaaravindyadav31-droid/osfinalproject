#include <stdio.h>
#include <unistd.h>

#include "../include/transform.h"
#include "../include/stats.h"

void run_transform(int read_fd, int write_fd, int stats_fd)
{
    char buffer[1024];

    int transformed_lines = 0;

    ssize_t bytes_read;
    ssize_t bytes_written;

    while ((bytes_read = read(read_fd, buffer, sizeof(buffer))) > 0)
    {
        /*
         * Convert lowercase letters to uppercase
         */
        for (int i = 0; i < bytes_read; i++)
        {
            if (buffer[i] >= 'a' && buffer[i] <= 'z')
            {
                buffer[i] = buffer[i] - 'a' + 'A';
            }

            /*
             * Count each complete line
             */
            if (buffer[i] == '\n')
            {
                transformed_lines++;
            }
        }

        bytes_written = write(write_fd, buffer, bytes_read);

        if (bytes_written == -1)
        {
            perror("Transform: Failed to write to pipe");
            close(read_fd);
            close(write_fd);
            close(stats_fd);
            return;
        }

        if (bytes_written != bytes_read)
        {
            fprintf(stderr,
                    "Transform: Incomplete data written to pipe\n");

            close(read_fd);
            close(write_fd);
            close(stats_fd);
            return;
        }
    }

    if (bytes_read == -1)
    {
        perror("Transform: Failed to read from pipe");
    }

    /*
     * Send Transform statistics to main process
     */
    StageReport report;

    report.stage = STAGE_TRANSFORM;
    report.lines = transformed_lines;

    if (write(stats_fd, &report, sizeof(report)) == -1)
    {
        perror("Transform: Failed to send statistics");
    }

    close(read_fd);
    close(write_fd);
    close(stats_fd);

    printf("Transform: Transformation completed\n");
}
