#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <string.h>

#include "../include/pipeline.h"

#define MAX_CHILDREN (PF_MAX_STAGES * 3)
#define MAX_FDS (PF_MAX_EDGES * 2 + PF_MAX_STAGES * 2)

static volatile sig_atomic_t termination_signal = 0;

static void handle_termination(int sig)
{
    termination_signal = sig;
}

static int write_all(int fd, const char *buf, size_t len)
{
    size_t done = 0;

    while (done < len)
    {
        ssize_t n = write(fd, buf + done, len - done);

        if (n > 0)
            done += (size_t)n;
        else if (n < 0 && errno == EINTR)
            continue;
        else
            return -1;
    }

    return 0;
}

static int is_kept(int fd, const int keep[], int keep_count)
{
    for (int i = 0; i < keep_count; i++)
        if (keep[i] == fd)
            return 1;

    return 0;
}

static void close_except(const int all[], int all_count,
                         const int keep[], int keep_count)
{
    for (int i = 0; i < all_count; i++)
        if (all[i] >= 0 && !is_kept(all[i], keep, keep_count))
            close(all[i]);
}

static void fanout_worker(int input_fd, const int outputs[],
                          int output_count, const int all[],
                          int all_count)
{
    int keep[PF_MAX_STAGES + 1];
    int active[PF_MAX_STAGES];
    char buffer[4096];

    keep[0] = input_fd;
    for (int i = 0; i < output_count; i++)
    {
        keep[i + 1] = outputs[i];
        active[i] = 1;
    }

    close_except(all, all_count, keep, output_count + 1);
    signal(SIGPIPE, SIG_IGN);

    for (;;)
    {
        ssize_t n = read(input_fd, buffer, sizeof(buffer));

        if (n == 0)
            break;

        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        int active_count = 0;

        for (int i = 0; i < output_count; i++)
        {
            if (!active[i])
                continue;

            if (write_all(outputs[i], buffer, (size_t)n) == -1)
            {
                close(outputs[i]);
                active[i] = 0;
            }
            else
                active_count++;
        }

        if (active_count == 0)
            break;
    }

    close(input_fd);
    for (int i = 0; i < output_count; i++)
        if (active[i])
            close(outputs[i]);

    _exit(0);
}

static void fanin_worker(const int inputs[], int input_count,
                         int output_fd, const int all[],
                         int all_count)
{
    struct pollfd pfds[PF_MAX_STAGES];
    int keep[PF_MAX_STAGES + 1];
    int active[PF_MAX_STAGES];
    char buffer[4096];

    for (int i = 0; i < input_count; i++)
    {
        keep[i] = inputs[i];
        pfds[i].fd = inputs[i];
        pfds[i].events = POLLIN | POLLHUP;
        pfds[i].revents = 0;
        active[i] = 1;
    }

    keep[input_count] = output_fd;
    close_except(all, all_count, keep, input_count + 1);
    signal(SIGPIPE, SIG_IGN);

    int remaining = input_count;

    while (remaining > 0)
    {
        int ready = poll(pfds, (nfds_t)input_count, -1);

        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        for (int i = 0; i < input_count; i++)
        {
            if (!active[i] || pfds[i].revents == 0)
                continue;

            ssize_t n = read(inputs[i], buffer, sizeof(buffer));

            if (n > 0)
            {
                if (write_all(output_fd, buffer, (size_t)n) == -1)
                {
                    remaining = 0;
                    break;
                }
            }
            else if (n == 0)
            {
                close(inputs[i]);
                active[i] = 0;
                pfds[i].fd = -1;
                remaining--;
            }
            else if (errno != EINTR)
            {
                close(inputs[i]);
                active[i] = 0;
                pfds[i].fd = -1;
                remaining--;
            }
        }
    }

    for (int i = 0; i < input_count; i++)
        if (active[i])
            close(inputs[i]);

    close(output_fd);
    _exit(0);
}

int run_graph_pipeline(PipelineGraph *g)
{
    int edge_pipes[PF_MAX_EDGES][2];
    int aux_pipes[PF_MAX_STAGES][2];
    int indegree[PF_MAX_STAGES] = {0};
    int outdegree[PF_MAX_STAGES] = {0};
    int all_fds[MAX_FDS];
    int fd_count = 0;
    pid_t children[MAX_CHILDREN];
    int child_count = 0;
    int result_code = 0;
    int handlers_installed = 0;
    struct sigaction action, old_int, old_term;

    if (!g || g->stage_count < 2 ||
        g->stage_count > PF_MAX_STAGES ||
        g->edge_count < 1 || g->edge_count > PF_MAX_EDGES)
    {
        fprintf(stderr, "Pipeline: Invalid graph\n");
        return 1;
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_termination;
    sigemptyset(&action.sa_mask);

    termination_signal = 0;

    if (sigaction(SIGINT, &action, &old_int) == -1)
    {
        perror("Pipeline: sigaction SIGINT");
        return 1;
    }

    if (sigaction(SIGTERM, &action, &old_term) == -1)
    {
        perror("Pipeline: sigaction SIGTERM");
        sigaction(SIGINT, &old_int, NULL);
        return 1;
    }

    handlers_installed = 1;

    for (int i = 0; i < PF_MAX_STAGES; i++)
        aux_pipes[i][0] = aux_pipes[i][1] = -1;

    for (int i = 0; i < g->edge_count; i++)
    {
        if (pipe(edge_pipes[i]) == -1)
        {
            perror("Pipeline: edge pipe");
            result_code = 1;
            goto cleanup;
        }

        all_fds[fd_count++] = edge_pipes[i][0];
        all_fds[fd_count++] = edge_pipes[i][1];

        outdegree[g->edges[i].from]++;
        indegree[g->edges[i].to]++;
    }

    for (int i = 0; i < g->stage_count; i++)
    {
        if (indegree[i] > 1 || outdegree[i] > 1)
        {
            if (pipe(aux_pipes[i]) == -1)
            {
                perror("Pipeline: relay pipe");
                result_code = 1;
                goto cleanup;
            }

            all_fds[fd_count++] = aux_pipes[i][0];
            all_fds[fd_count++] = aux_pipes[i][1];
        }
    }

    for (int i = 0; i < g->stage_count; i++)
    {
        int input_fd = STDIN_FILENO;
        int output_fd = STDOUT_FILENO;

        if (indegree[i] == 1)
        {
            for (int e = 0; e < g->edge_count; e++)
                if (g->edges[e].to == i)
                    input_fd = edge_pipes[e][0];
        }
        else if (indegree[i] > 1)
            input_fd = aux_pipes[i][0];

        if (outdegree[i] == 1)
        {
            for (int e = 0; e < g->edge_count; e++)
                if (g->edges[e].from == i)
                    output_fd = edge_pipes[e][1];
        }
        else if (outdegree[i] > 1)
            output_fd = aux_pipes[i][1];

        pid_t pid = fork();

        if (pid == -1)
        {
            perror("Pipeline: fork stage");
            result_code = 1;
            goto stop_children;
        }

        if (pid == 0)
        {
            int keep[2];
            int keep_count = 0;

            if (indegree[i] > 0)
                keep[keep_count++] = input_fd;
            if (outdegree[i] > 0)
                keep[keep_count++] = output_fd;

            close_except(all_fds, fd_count, keep, keep_count);

            if (indegree[i] > 0 &&
                dup2(input_fd, STDIN_FILENO) == -1)
            {
                perror("Pipeline: dup2 stdin");
                _exit(126);
            }

            if (outdegree[i] > 0 &&
                dup2(output_fd, STDOUT_FILENO) == -1)
            {
                perror("Pipeline: dup2 stdout");
                _exit(126);
            }

            if (indegree[i] > 0 && input_fd != STDIN_FILENO)
                close(input_fd);
            if (outdegree[i] > 0 && output_fd != STDOUT_FILENO)
                close(output_fd);

            execvp(g->stages[i].path, g->stages[i].argv);
            perror(g->stages[i].path);
            _exit(127);
        }

        children[child_count++] = pid;
        fprintf(stderr, "[Manager] Started stage %-12s PID=%ld\n",
                g->stages[i].name, (long)pid);
    }

    /* Start fan-out relays. */
    for (int i = 0; i < g->stage_count; i++)
    {
        if (outdegree[i] <= 1)
            continue;

        int outputs[PF_MAX_STAGES];
        int count = 0;

        for (int e = 0; e < g->edge_count; e++)
            if (g->edges[e].from == i)
                outputs[count++] = edge_pipes[e][1];

        pid_t pid = fork();

        if (pid == -1)
        {
            perror("Pipeline: fork fan-out");
            result_code = 1;
            goto stop_children;
        }

        if (pid == 0)
            fanout_worker(aux_pipes[i][0], outputs, count,
                          all_fds, fd_count);

        children[child_count++] = pid;
    }

    /* Start fan-in relays. */
    for (int i = 0; i < g->stage_count; i++)
    {
        if (indegree[i] <= 1)
            continue;

        int inputs[PF_MAX_STAGES];
        int count = 0;

        for (int e = 0; e < g->edge_count; e++)
            if (g->edges[e].to == i)
                inputs[count++] = edge_pipes[e][0];

        pid_t pid = fork();

        if (pid == -1)
        {
            perror("Pipeline: fork fan-in");
            result_code = 1;
            goto stop_children;
        }

        if (pid == 0)
            fanin_worker(inputs, count, aux_pipes[i][1],
                         all_fds, fd_count);

        children[child_count++] = pid;
    }

    close_except(all_fds, fd_count, NULL, 0);

    for (int i = 0; i < child_count; i++)
    {
        int status;
        pid_t waited;

        do {
            waited = waitpid(children[i], &status, 0);
        } while (waited == -1 && errno == EINTR && !termination_signal);

        if (termination_signal)
        {
            result_code = 128 + termination_signal;
            goto stop_children;
        }

        if (waited == -1)
        {
            perror("Pipeline: waitpid");
            result_code = 1;
        }
        else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            fprintf(stderr, "[Manager] PID=%ld failed\n",
                    (long)children[i]);
            result_code = 1;
        }
    }

    if (termination_signal)
        result_code = 128 + termination_signal;

    if (handlers_installed)
    {
        sigaction(SIGINT, &old_int, NULL);
        sigaction(SIGTERM, &old_term, NULL);
    }

    return result_code;

stop_children:
    for (int i = 0; i < child_count; i++)
        kill(children[i], SIGTERM);

    for (int i = 0; i < child_count; i++)
    {
        while (waitpid(children[i], NULL, 0) == -1 && errno == EINTR)
            ;
    }

    if (termination_signal)
        result_code = 128 + termination_signal;

cleanup:
    close_except(all_fds, fd_count, NULL, 0);

    if (handlers_installed)
    {
        sigaction(SIGINT, &old_int, NULL);
        sigaction(SIGTERM, &old_term, NULL);
    }

    return result_code ? result_code : 1;
}

int run_pipeline(Stage stages[], int stage_count)
{
    PipelineGraph g = {0};

    if (stage_count < 2 || stage_count > PF_MAX_STAGES)
        return 1;

    g.stage_count = stage_count;

    for (int i = 0; i < stage_count; i++)
    {
        g.stages[i] = stages[i];

        if (i > 0)
            g.edges[g.edge_count++] = (Edge){i - 1, i};
    }

    return run_graph_pipeline(&g);
}
