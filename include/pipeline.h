#ifndef PIPELINE_H
#define PIPELINE_H

#include "stage.h"

int run_pipeline(Stage stages[], int stage_count);
int run_graph_pipeline(PipelineGraph *graph);

#endif
