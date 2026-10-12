# PipeFlow — Dataflow Stream Engine

## 1. Purpose

PipeFlow is a Linux-based dataflow stream engine that connects independent
processes to process records through configurable pipelines and graphs.

## 2. Architecture

The default pipeline is:

Source -> Filter -> Transform -> Sink

The graph configuration can also describe fan-out (one stage feeding
multiple destinations) and fan-in (multiple sources feeding a common sink).

## 3. Process Model

The manager loads the configuration, creates the required pipes, and
starts stage processes using fork() and execvp(). The dup2() system call
connects each stage's standard input and standard output to the appropriate
pipe descriptors.

## 4. Inter-Process Communication

PipeFlow uses anonymous POSIX pipes for communication between processes.
The operating system manages pipe buffers and blocks writers when a pipe
cannot accept more data, providing natural backpressure.

## 5. End-of-Stream and File Descriptors

When every write descriptor for a pipe is closed, its reader receives EOF.
The manager and stage processes close descriptors they no longer need.
The manager uses waitpid() to collect child process statuses.

## 6. Error Handling and Signals

Built-in stages handle write errors, including EPIPE when a downstream
consumer closes its pipe. This supports graceful handling of early-exiting
consumers.

The manager installs SIGINT and SIGTERM handlers and attempts to stop and
reap child processes when termination is requested. Signal termination may
produce a non-zero exit status.

## 7. Backpressure and Metrics

Built-in stages report record counts, approximate records per second, and
time spent in write() calls. A deliberately slow sink can be used to
observe backpressure through the pipeline.

## 8. Testing

The test suite covers:
- Linear pipeline output
- Early-exiting consumer / SIGPIPE behavior
- Slow-sink backpressure experiment
- Repeated start/stop cleanup
- Fan-out to multiple sinks
- Fan-in from multiple input streams

Valgrind is used to check memory errors and leaks.

## 9. Limitations and Future Work

- Automatic restart of failed stages is not implemented.
- A dedicated DAG scheduling and recovery system is future work.
- Fan-in merges byte streams; general record-boundary preservation and
  deterministic ordering across concurrent producers are not guaranteed.
- Throughput measurements are approximate and depend on the environment.
