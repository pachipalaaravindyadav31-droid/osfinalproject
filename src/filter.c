#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../include/filter.h"
#include "../include/stats.h"

void run_filter(int read_fd, int write_fd, int stats_fd)
{
    char buffer[1024];
    char line[1024];

    int line_pos = 0;
    int filtered_lines = 0;

    ssize_t bytes_read;
    ssize_t bytes_written;

    while ((bytes_read = read(read_fd, buffer, sizeof(buffer))) > 0)
    {
        for (int i = 0; i < bytes_read; i++)
        {
            if (buffer[i] == '\n')
            {
                line[line_pos] = '\0';

                /*
                 * Filter condition:
                 * Keep only lines containing "ap"
                 */
                if (strstr(line, "ERROR") != NULL)
                {
                    bytes_written = write(write_fd, line, strlen(line));

                    if (bytes_written == -1)
                    {
                        perror("Filter: Failed to write to pipe");
                        close(read_fd);
                        close(write_fd);
                        return;
                    }

                    if (write(write_fd, "\n", 1) == -1)
                    {
                        perror("Filter: Failed to write newline");
                        close(read_fd);
                        close(write_fd);
                        return;
                    }

                    filtered_lines++;
                }

                line_pos = 0;
            }
            else
            {
                if (line_pos < 1023)
                {
                    line[line_pos++] = buffer[i];
                }
                else
                {
                    fprintf(stderr,
                            "Filter: Line too long, skipping extra characters\n");
                }
            }
        }
    }

    if (bytes_read == -1)
    {
        perror("Filter: Failed to read from pipe");
    }

    /*
     * Send Filter statistics to main process
     */
    StageReport report;

    report.stage = STAGE_FILTER;
    report.lines = filtered_lines;

    if (write(stats_fd, &report, sizeof(report)) == -1)
    {
        perror("Filter: Failed to send statistics");
    }

    close(read_fd);
    close(write_fd);
    close(stats_fd);

    printf("Filter: Processing completed\n");
}
