#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

int main()
{
    int fd[2];
    pid_t pid;

    char buffer[1024];
    ssize_t bytes_read;

    // Create pipe
    if (pipe(fd) == -1)
    {
        perror("pipe");
        return 1;
    }

    // Create child process
    pid = fork();

    if (pid == -1)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0)
    {
        // =========================
        // CHILD = SINK PROCESS
        // =========================

        int output_fd;

        // Child only reads from pipe
        close(fd[1]);

        // Open output file
        output_fd = open("../data/output.txt",
                         O_WRONLY | O_CREAT | O_TRUNC,
                         0644);

        if (output_fd == -1)
        {
            perror("output.txt");
            close(fd[0]);
            exit(1);
        }

        // Read data from pipe
        while ((bytes_read = read(fd[0],
                                  buffer,
                                  sizeof(buffer))) > 0)
        {
            // Write data to output file
            write(output_fd, buffer, bytes_read);
        }

        close(fd[0]);
        close(output_fd);

        printf("Sink: Data written to output.txt\n");
    }
    else
    {
        // =========================
        // PARENT = SOURCE PROCESS
        // =========================

        int input_fd;

        // Parent only writes to pipe
        close(fd[0]);

        // Open input file
        input_fd = open("../data/input.txt", O_RDONLY);

        if (input_fd == -1)
        {
            perror("input.txt");
            close(fd[1]);
            waitpid(pid, NULL, 0);
            return 1;
        }

        // Read input file and send data through pipe
        while ((bytes_read = read(input_fd,
                                  buffer,
                                  sizeof(buffer))) > 0)
        {
            write(fd[1], buffer, bytes_read);
        }

        close(input_fd);
        close(fd[1]);

        printf("Source: Data sent through pipe\n");

        // Wait for sink process
        waitpid(pid, NULL, 0);

        printf("PipeFlow: Source -> Pipe -> Sink completed\n");
    }

    return 0;
}
