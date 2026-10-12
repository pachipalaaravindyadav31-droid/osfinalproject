#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "../include/sink.h"
#include "../include/stats.h"

void run_sink(int read_fd, int stats_fd)
{
    int output_fd;

    char buffer[1024];

    int output_lines = 0;

    ssize_t bytes_read;
    ssize_t bytes_written;

    output_fd = open(
        "data/output.txt",
        O_WRONLY | O_CREAT | O_TRUNC,
        0644
    );

    if (output_fd == -1)
    {
        perror("Sink: Unable to open output.txt");
        close(read_fd);
        close(stats_fd);
        return;
    }

    while ((bytes_read = read(read_fd, buffer, sizeof(buffer))) > 0)
    {
        for (int i = 0; i < bytes_read; i++)
        {
            if (buffer[i] == '\n')
            {
                output_lines++;
            }
        }

        bytes_written = write(output_fd, buffer, bytes_read);

        if (bytes_written == -1)
        {
            perror("Sink: Failed to write to output.txt");

            close(output_fd);
            close(read_fd);
            close(stats_fd);

            return;
        }

        if (bytes_written != bytes_read)
        {
            fprintf(stderr,
                    "Sink: Incomplete data written to output.txt\n");

            close(output_fd);
            close(read_fd);
            close(stats_fd);

            return;
        }
    }

    if (bytes_read == -1)
    {
        perror("Sink: Failed to read from pipe");
    }

    /*
     * Send Sink statistics to main process
     */
    StageReport report;

    report.stage = STAGE_SINK;
    report.lines = output_lines;

    if (write(stats_fd, &report, sizeof(report)) == -1)
    {
        perror("Sink: Failed to send statistics");
    }

    close(output_fd);
    close(read_fd);
    close(stats_fd);

    printf("Sink: Output written successfully\n");
}
