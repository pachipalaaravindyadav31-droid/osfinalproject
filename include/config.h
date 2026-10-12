#ifndef CONFIG_H
#define CONFIG_H

#include "stage.h"

int load_config(const char *filename,
                Stage stages[],
                int *stage_count);

int load_graph_config(const char *filename,
                      PipelineGraph *graph);

#endif
