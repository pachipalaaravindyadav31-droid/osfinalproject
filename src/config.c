#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../include/config.h"

static const char *stage_path(const char *name)
{
    if (strcmp(name, "source") == 0) return "./build/source";
    if (strcmp(name, "filter") == 0) return "./build/filter";
    if (strcmp(name, "transform") == 0) return "./build/transform";
    if (strcmp(name, "sink") == 0) return "./build/sink";

    /* Allow multiple uniquely named instances of built-in stages. */
    if (strncmp(name, "source_", 7) == 0) return "./build/source";
    if (strncmp(name, "filter_", 7) == 0) return "./build/filter";
    if (strncmp(name, "transform_", 10) == 0) return "./build/transform";
    if (strncmp(name, "sink_", 5) == 0) return "./build/sink";

    return NULL;
}

static void free_stage_args(Stage stages[], int count)
{
    for (int i = 0; i < count; i++)
    {
        for (int j = 1; j < stages[i].argc; j++)
            free(stages[i].argv[j]);

        stages[i].argc = 0;
    }
}

static int find_stage(const PipelineGraph *g, const char *name)
{
    for (int i = 0; i < g->stage_count; i++)
        if (strcmp(g->stages[i].name, name) == 0)
            return i;
    return -1;
}

static int add_stage(PipelineGraph *g, char **tokens, int n)
{
    if (n < 1 || n >= PF_MAX_ARGS || g->stage_count >= PF_MAX_STAGES)
        return -1;

    const char *path = stage_path(tokens[0]);

    /* Allow external commands such as head, cat, and wc. */
    if (!path)
        path = tokens[0];

    if (find_stage(g, tokens[0]) >= 0)
    {
        fprintf(stderr, "Config: Duplicate stage '%s'\n", tokens[0]);
        return -1;
    }

    Stage *s = &g->stages[g->stage_count];
    memset(s, 0, sizeof(*s));
    snprintf(s->name, sizeof(s->name), "%s", tokens[0]);
    snprintf(s->path, sizeof(s->path), "%s", path);
    s->argv[0] = s->path;
    s->argc = 1;

    for (int i = 1; i < n; i++)
    {
        s->argv[s->argc] = strdup(tokens[i]);
        if (!s->argv[s->argc])
        {
            perror("Config: strdup");
            for (int j = 1; j < s->argc; j++)
                free(s->argv[j]);
            memset(s, 0, sizeof(*s));
            return -1;
        }
        s->argc++;
    }

    s->argv[s->argc] = NULL;
    g->stage_count++;
    return 0;
}

static int add_edge(PipelineGraph *g, const char *from, const char *to)
{
    int u = find_stage(g, from);
    int v = find_stage(g, to);

    if (u < 0 || v < 0 || u == v ||
        g->edge_count >= PF_MAX_EDGES)
    {
        fprintf(stderr, "Config: Invalid edge %s -> %s\n", from, to);
        return -1;
    }

    for (int i = 0; i < g->edge_count; i++)
        if (g->edges[i].from == u && g->edges[i].to == v)
        {
            fprintf(stderr, "Config: Duplicate edge\n");
            return -1;
        }

    g->edges[g->edge_count++] = (Edge){u, v};
    return 0;
}

int load_graph_config(const char *filename, PipelineGraph *g)
{
    if (!filename || !g) return -1;
    memset(g, 0, sizeof(*g));

    FILE *f = fopen(filename, "r");
    if (!f)
    {
        perror("Config: fopen");
        return -1;
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    int connections_started = 0;
    int result = -1;

    while ((len = getline(&line, &cap, f)) != -1)
    {
        (void)len;
        char *tokens[PF_MAX_ARGS + 1];
        int n = 0;
        char *save = NULL;
        char *t = strtok_r(line, " \t\r\n", &save);

        if (!t || t[0] == '#') continue;

        while (t && n < PF_MAX_ARGS + 1)
        {
            tokens[n++] = t;
            t = strtok_r(NULL, " \t\r\n", &save);
        }

        if (t)
        {
            fprintf(stderr, "Config: Too many tokens\n");
            goto done;
        }

        if (strcmp(tokens[0], "connect") == 0)
        {
            connections_started = 1;
            if (n != 3 || add_edge(g, tokens[1], tokens[2]) != 0)
                goto done;
        }
        else
        {
            if (connections_started || add_stage(g, tokens, n) != 0)
            {
                fprintf(stderr, "Config: Invalid stage definition\n");
                goto done;
            }
        }
    }

    if (ferror(f))
    {
        perror("Config: getline");
        goto done;
    }

    if (g->stage_count < 2 || g->edge_count == 0)
    {
        fprintf(stderr, "Config: Need at least two stages and one edge\n");
        goto done;
    }

    /* Reject cycles using topological sorting. */
    {
        int indegree[PF_MAX_STAGES] = {0};
        int queue[PF_MAX_STAGES];
        int head = 0, tail = 0, visited = 0;

        for (int i = 0; i < g->edge_count; i++)
            indegree[g->edges[i].to]++;

        for (int i = 0; i < g->stage_count; i++)
            if (indegree[i] == 0) queue[tail++] = i;

        while (head < tail)
        {
            int u = queue[head++];
            visited++;
            for (int i = 0; i < g->edge_count; i++)
                if (g->edges[i].from == u &&
                    --indegree[g->edges[i].to] == 0)
                    queue[tail++] = g->edges[i].to;
        }

        if (visited != g->stage_count)
        {
            fprintf(stderr, "Config: Graph contains a cycle\n");
            goto done;
        }
    }

    result = 0;

done:
    free(line);
    fclose(f);

    if (result != 0)
    {
        free_stage_args(g->stages, g->stage_count);
        memset(g, 0, sizeof(*g));
    }
    return result;
}

/* Compatibility loader for the current linear runner. */
int load_config(const char *filename, Stage stages[], int *stage_count)
{
    PipelineGraph g;
    if (load_graph_config(filename, &g) != 0) return -1;

    if (g.edge_count != g.stage_count - 1)
    {
        fprintf(stderr, "Config: Current runner requires a linear chain\n");
        free_stage_args(g.stages, g.stage_count);
        return -1;
    }

    for (int i = 0; i < g.stage_count - 1; i++)
    {
        int found = 0;
        for (int j = 0; j < g.edge_count; j++)
            if (g.edges[j].from == i && g.edges[j].to == i + 1)
                found = 1;

        if (!found)
        {
            fprintf(stderr, "Config: Edges must follow stage order\n");
            free_stage_args(g.stages, g.stage_count);
            return -1;
        }
    }

    for (int i = 0; i < g.stage_count; i++)
    {
        stages[i] = g.stages[i];
        g.stages[i].argc = 0;
    }

    *stage_count = g.stage_count;
    return 0;
}
