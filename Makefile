CXX ?= g++
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Isrc

BUILD = build
PURR_TMP ?= $(CURDIR)/tmp
export PURR_TMP

LOGGER = bin/purrdometer-logger

LOGGER_PKGS = sdbus-c++ sqlite3

all: $(LOGGER)

$(LOGGER): src/purrdometer-logger.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(shell pkg-config --cflags $(LOGGER_PKGS)) -o $@ $< $(shell pkg-config --libs $(LOGGER_PKGS)) -pthread

test: all
	@mkdir -p $(PURR_TMP)
	$(LOGGER) --selftest
	cd python && PURR_WHEELS=../data/wheels.ini python3 -m unittest discover -s tests -t . -v

# Render every e-paper screen from a fake-history database into preview/ (no hardware needed).
preview:
	@mkdir -p $(PURR_TMP)
	cd python && PURR_WHEELS=../data/wheels.ini python3 -m purrdometer.display --demo-db $(PURR_TMP)/demo.db --preview ../preview

# The same renders, for the README.
media:
	@mkdir -p $(PURR_TMP)
	cd python && PURR_WHEELS=../data/wheels.ini python3 -m purrdometer.display --demo-db $(PURR_TMP)/demo.db --preview ../docs/media

# Original prototype pairing tool.
legacy: pair.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $(shell pkg-config --cflags $(LOGGER_PKGS)) -o bin/pair pair.cpp $(shell pkg-config --libs $(LOGGER_PKGS)) -pthread

clean:
	rm -rf $(BUILD) $(LOGGER) bin/pair

.PHONY: all test preview media legacy clean

