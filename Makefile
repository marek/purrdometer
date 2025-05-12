CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
LDFLAGS = $(shell pkg-config --cflags --libs sdbus-c++) -pthread -lstdc++fs

PAIR_TARGET = bin/pair
PAIR_SRCS = pair.cpp
PAIR_OBJS = $(PAIR_SRCS:.cpp=.o)

MONITOR_TARGET = bin/monitor
MONITOR_SRCS = monitor.cpp
MONITOR_OBJS = $(MONITOR_SRCS:.cpp=.o)

TARGETS = $(PAIR_TARGET) $(MONITOR_TARGET)
OBJS = $(PAIR_OBJS) $(MONITOR_OBJS)


define build_target
$(1): $(2)
	mkdir -p bin
	$(CXX) -o $$@ $$^ $(LDFLAGS)
endef


all: $(TARGETS)

$(eval $(call build_target,$(PAIR_TARGET),$(PAIR_OBJS)))
$(eval $(call build_target,$(MONITOR_TARGET),$(MONITOR_OBJS)))


.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
