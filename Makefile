CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -Wno-misleading-indentation
INC      = -Ifirmware/include
SRC      = firmware/src/meter_dsp.c
BUILD    = build

.PHONY: all test sim clean

all: test sim

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_dsp: tests/test_dsp.c $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ $^ -lm

$(BUILD)/sim_meter: sim/sim_meter.c $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ $^ -lm

test: $(BUILD)/test_dsp
	./$(BUILD)/test_dsp

sim: $(BUILD)/sim_meter
	./$(BUILD)/sim_meter

clean:
	rm -rf $(BUILD)
