#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

int main()
{
    int pipe1[2];   // Source -> Filter
    int pipe2[2];   // Filter -> Transform
    int pipe3[2];   // Transform -> Sink

    if (pipe(pipe1) == -1 ||
        pipe(pipe2) == -1 ||
        pipe(pipe3) == -1)
    {
        perror("pipe");
        return 1;
    }

    pid_t filter_pid, transform_pid, sink_pid;

    // =========================
    // FILTER PROCESS
    // =========================

    filter_pid = fork();

    if (filter_pid == -1)
    {
        perror("fork");
        return 1;
    }

    if (filter_pid == 0)
    {
        char buffer[1024];
        char line[1024];
        int line_pos = 0;
        ssize_t bytes_read;

        // Close unused pipes
        close(pipe1[1]);

        close(pipe2[0]);

        close(pipe3[0]);
        close(pipe3[1]);

        // Read data from Source
        while ((bytes_read = read(pipe1[0], buffer, sizeof(buffer))) > 0)
        {
            for (int i = 0; i < bytes_read; i++)
            {
                if (buffer[i] == '\n')
                {
                    line[line_pos] = '\0';

                    // Filter: keep lines containing "ap"
                    if (strstr(line, "ap") != NULL)
                    {
                        write(pipe2[1], line, strlen(line));
                        write(pipe2[1], "\n", 1);
                    }

                    line_pos = 0;
                }
                else
                {
                    if (line_pos < 1023)
                    {
                        line[line_pos++] = buffer[i];
                    }
                }
            }
        }

        close(pipe1[0]);
        close(pipe2[1]);

        printf("Filter: Processing completed\n");

        exit(0);
    }

    // =========================
    // TRANSFORM PROCESS
    // =========================

    transform_pid = fork();

    if (transform_pid == -1)
    {
        perror("fork");
        return 1;
    }

    if (transform_pid == 0)
    {
        char buffer[1024];
        ssize_t bytes_read;

        // Close unused pipes
        close(pipe1[0]);
        close(pipe1[1]);

        close(pipe2[1]);

        close(pipe3[0]);

        // Read filtered data
        while ((bytes_read = read(pipe2[0], buffer, sizeof(buffer))) > 0)
        {
            // Convert lowercase letters to uppercase
            for (int i = 0; i < bytes_read; i++)
            {
                if (buffer[i] >= 'a' && buffer[i] <= 'z')
                {
                    buffer[i] = buffer[i] - 'a' + 'A';
                }
            }

            // Send transformed data to Sink
            write(pipe3[1], buffer, bytes_read);
        }

        close(pipe2[0]);
        close(pipe3[1]);

        printf("Transform: Transformation completed\n");

        exit(0);
    }

    // =========================
    // SINK PROCESS
    // =========================

    sink_pid = fork();

    if (sink_pid == -1)
    {
        perror("fork");
        return 1;
    }

    if (sink_pid == 0)
    {
        char buffer[1024];
        ssize_t bytes_read;

        // Close unused pipes
        close(pipe1[0]);
        close(pipe1[1]);

        close(pipe2[0]);
        close(pipe2[1]);

        close(pipe3[1]);

        // Open output file
        int output_fd = open(
            "../data/output.txt",
            O_WRONLY | O_CREAT | O_TRUNC,
            0644
        );

        if (output_fd == -1)
        {
            perror("output.txt");
            exit(1);
        }

        // Read transformed data
        while ((bytes_read = read(pipe3[0], buffer, sizeof(buffer))) > 0)
        {
            write(output_fd, buffer, bytes_read);
        }

        close(output_fd);
        close(pipe3[0]);

        printf("Sink: Output written successfully\n");

        exit(0);
    }

    // =========================
    // SOURCE PROCESS
    // =========================

    // Parent acts as Source

    close(pipe1[0]);

    close(pipe2[0]);
    close(pipe2[1]);

    close(pipe3[0]);
    close(pipe3[1]);

    int input_fd = open("../data/input.txt", O_RDONLY);

    if (input_fd == -1)
    {
        perror("input.txt");
        return 1;
    }

    char buffer[1024];
    ssize_t bytes_read;

    // Read input file and send to Filter
    while ((bytes_read = read(input_fd, buffer, sizeof(buffer))) > 0)
    {
        write(pipe1[1], buffer, bytes_read);
    }

    close(input_fd);
    close(pipe1[1]);

    printf("Source: Input sent to Filter\n");

    // Wait for all child processes
    waitpid(filter_pid, NULL, 0);
    waitpid(transform_pid, NULL, 0);
    waitpid(sink_pid, NULL, 0);

    printf("PipeFlow: Source -> Filter -> Transform -> Sink completed\n");

    return 0;
}
