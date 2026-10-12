#ifndef STAGE_H
#define STAGE_H

#define PF_MAX_STAGES 32
#define PF_MAX_ARGS 16
#define PF_NAME_SIZE 32
#define PF_PATH_SIZE 256
#define PF_MAX_EDGES (PF_MAX_STAGES * (PF_MAX_STAGES - 1))

typedef struct
{
    char name[PF_NAME_SIZE];
    char path[PF_PATH_SIZE];
    char *argv[PF_MAX_ARGS];
    int argc;
} Stage;

typedef struct
{
    int from;
    int to;
} Edge;

typedef struct
{
    Stage stages[PF_MAX_STAGES];
    int stage_count;

    Edge edges[PF_MAX_EDGES];
    int edge_count;
} PipelineGraph;

#endif
