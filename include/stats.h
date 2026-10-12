#ifndef STATS_H
#define STATS_H

#include <stddef.h>

long long monotonic_ns(void);

int write_all_counted(int fd,
                      const void *buffer,
                      size_t size,
                      long long *write_time_ns);

long long count_newlines(const char *buffer, size_t size);

void report_stage_metrics(const char *name,
                          long long records,
                          long long elapsed_ns,
                          long long write_time_ns);

#endif
