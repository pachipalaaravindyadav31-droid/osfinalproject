#include <stdio.h>
#include <stdlib.h>

#include "../include/config.h"
#include "../include/pipeline.h"

int main(int argc, char *argv[])
{
    const char *config_file = "config/pipeline.conf";

    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [config-file]\n", argv[0]);
        return 2;
    }

    if (argc == 2)
        config_file = argv[1];

    PipelineGraph graph = {0};

    printf("========================================\n");
    printf("       PipeFlow Dataflow Engine\n");
    printf("========================================\n");
    printf("Configuration: %s\n", config_file);

    if (load_graph_config(config_file, &graph) == -1)
    {
        fprintf(stderr, "PipeFlow: Configuration loading failed\n");
        return 1;
    }

    printf("Stages configured: %d\n", graph.stage_count);

    for (int i = 0; i < graph.stage_count; i++)
        printf("  %d. %s\n", i + 1, graph.stages[i].name);

    printf("Connections configured: %d\n", graph.edge_count);
    printf("========================================\n");
    fflush(stdout);

    int result = run_graph_pipeline(&graph);

    for (int i = 0; i < graph.stage_count; i++)
    {
        for (int j = 1; j < graph.stages[i].argc; j++)
            free(graph.stages[i].argv[j]);
    }

    if (result == 0)
        printf("\nPipeFlow pipeline completed successfully.\n");
    else
        fprintf(stderr, "\nPipeFlow pipeline failed (status %d).\n", result);

    return result;
}
