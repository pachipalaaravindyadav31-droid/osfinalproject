CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
BUILD = build

.PHONY: all clean test

all: $(BUILD)/pipeflow $(BUILD)/source $(BUILD)/filter $(BUILD)/transform $(BUILD)/sink

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/pipeflow: src/main.c src/config.c src/pipeline.c include/config.h include/pipeline.h include/stage.h | $(BUILD)
	$(CC) $(CFLAGS) src/main.c src/config.c src/pipeline.c -o $@

$(BUILD)/source: src/stages/source.c src/stats.c include/stats.h | $(BUILD)
	$(CC) $(CFLAGS) src/stages/source.c src/stats.c -o $@

$(BUILD)/filter: src/stages/filter.c src/stats.c include/stats.h | $(BUILD)
	$(CC) $(CFLAGS) src/stages/filter.c src/stats.c -o $@

$(BUILD)/transform: src/stages/transform.c src/stats.c include/stats.h | $(BUILD)
	$(CC) $(CFLAGS) src/stages/transform.c src/stats.c -o $@

$(BUILD)/sink: src/stages/sink.c src/stats.c include/stats.h | $(BUILD)
	$(CC) $(CFLAGS) src/stages/sink.c src/stats.c -o $@

test: all
	bash tests/test_linear.sh
	bash tests/test_sigpipe.sh
	bash tests/test_backpressure.sh
	bash tests/test_cleanup.sh
	bash tests/test_fanout.sh
	bash tests/test_fanin.sh

clean:
	rm -rf $(BUILD)
