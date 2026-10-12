#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

int main()
{
    int pipe1[2];
    int pipe2[2];

    pid_t filter_pid;
    pid_t sink_pid;

    char buffer[1024];
    char line[1024];

    ssize_t bytes_read;
    int line_pos = 0;

    /* PIPE 1: Source -> Filter */
    if (pipe(pipe1) == -1)
    {
        perror("pipe1");
        return 1;
    }

    /* PIPE 2: Filter -> Sink */
    if (pipe(pipe2) == -1)
    {
        perror("pipe2");
        return 1;
    }

    /* =========================
       CREATE FILTER PROCESS
       ========================= */

    filter_pid = fork();

    if (filter_pid == -1)
    {
        perror("fork");
        return 1;
    }

    if (filter_pid == 0)
    {
        /* FILTER */

        close(pipe1[1]);
        close(pipe2[0]);

        while ((bytes_read = read(pipe1[0],
                                  buffer,
                                  sizeof(buffer))) > 0)
        {
            for (ssize_t i = 0; i < bytes_read; i++)
            {
                line[line_pos++] = buffer[i];

                if (buffer[i] == '\n')
                {
                    line[line_pos] = '\0';

                    /* Keep lines containing "ap" */
                    if (strstr(line, "ap") != NULL)
                    {
                        write(pipe2[1],
                              line,
                              strlen(line));
                    }

                    line_pos = 0;
                }
            }
        }

        close(pipe1[0]);
        close(pipe2[1]);

        printf("Filter: Processing completed\n");

        exit(0);
    }

    /* =========================
       CREATE SINK PROCESS
       ========================= */

    sink_pid = fork();

    if (sink_pid == -1)
    {
        perror("fork");
        return 1;
    }

    if (sink_pid == 0)
    {
        int output_fd;

        /* SINK */

        close(pipe1[0]);
        close(pipe1[1]);

        close(pipe2[1]);

        output_fd = open("../data/output.txt",
                         O_WRONLY | O_CREAT | O_TRUNC,
                         0644);

        if (output_fd == -1)
        {
            perror("output.txt");
            close(pipe2[0]);
            exit(1);
        }

        while ((bytes_read = read(pipe2[0],
                                  buffer,
                                  sizeof(buffer))) > 0)
        {
            write(output_fd,
                  buffer,
                  bytes_read);
        }

        close(pipe2[0]);
        close(output_fd);

        printf("Sink: Output written successfully\n");

        exit(0);
    }

    /* =========================
       SOURCE PROCESS
       ========================= */

    int input_fd;

    /* Source writes only to pipe1 */
    close(pipe1[0]);

    /* IMPORTANT:
       Parent/Source does not use pipe2 at all */
    close(pipe2[0]);
    close(pipe2[1]);

    input_fd = open("../data/input.txt", O_RDONLY);

    if (input_fd == -1)
    {
        perror("input.txt");

        close(pipe1[1]);

        waitpid(filter_pid, NULL, 0);
        waitpid(sink_pid, NULL, 0);

        return 1;
    }

    while ((bytes_read = read(input_fd,
                              buffer,
                              sizeof(buffer))) > 0)
    {
        write(pipe1[1],
              buffer,
              bytes_read);
    }

    close(input_fd);

    /*
     * Closing pipe1 write end sends EOF
     * to the Filter process.
     */
    close(pipe1[1]);

    printf("Source: Input sent to Filter\n");

    /* Wait for Filter */
    waitpid(filter_pid, NULL, 0);

    /* Wait for Sink */
    waitpid(sink_pid, NULL, 0);

    printf("PipeFlow: Source -> Filter -> Sink completed\n");

    return 0;
}
