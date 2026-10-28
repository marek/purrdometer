CXX ?= g++
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++26 -Wall -Wextra -Werror -Isrc -I$(BUILD)/gen

# Install directories, as autotools names them; the defaults are the Debian layout the package uses.
PREFIX ?= /usr
SYSCONFDIR ?= /etc
LOCALSTATEDIR ?= /var
RUNSTATEDIR ?= /run
DATADIR ?= $(PREFIX)/share
# The system's fonts-dejavu-core, not ours: not under PREFIX.
FONTDIR ?= /usr/share/fonts/truetype/dejavu
CXXFLAGS += -DPURR_SYSCONFDIR='"$(SYSCONFDIR)"' -DPURR_LOCALSTATEDIR='"$(LOCALSTATEDIR)"' \
            -DPURR_RUNSTATEDIR='"$(RUNSTATEDIR)"' -DPURR_DATADIR='"$(DATADIR)"' -DPURR_FONTDIR='"$(FONTDIR)"'

BUILD = build
PURR_TMP ?= $(CURDIR)/tmp
export PURR_TMP

LOGGER = bin/purrdometer-logger
WEB = bin/purrdometer-web
DISPLAY = bin/purrdometer-display
SETUP = bin/purrdometer-setup
PAGEC = bin/pagec

LOGGER_PKGS = sdbus-c++ sqlite3
# cpp-httplib is built with TLS, zlib and brotli support; its .pc file leaves their libraries out.
WEB_PKGS = cpp-httplib sqlite3 nlohmann_json openssl zlib libbrotlienc libbrotlidec
DISPLAY_PKGS = freetype2 harfbuzz libgpiod libpng libqrencode sqlite3

# Pages: web/pages/<name>.page -> purr::pages::<name>() in build/gen/.
PAGES = $(wildcard web/pages/*.page)
GEN_SRCS = $(patsubst web/pages/%.page,$(BUILD)/gen/%.page.cpp,$(PAGES))
GEN_HDRS = $(GEN_SRCS:.cpp=.h)
COMMON_SRCS = $(wildcard src/common/*.cpp)
WEB_SRCS = $(wildcard src/web/*.cpp) $(COMMON_SRCS)
WEB_OBJS = $(patsubst %.cpp,$(BUILD)/%.o,$(WEB_SRCS)) $(GEN_SRCS:.cpp=.o)
DISPLAY_SRCS = $(wildcard src/display/*.cpp src/display/screens/*.cpp) $(COMMON_SRCS)
DISPLAY_OBJS = $(patsubst %.cpp,$(BUILD)/%.o,$(DISPLAY_SRCS))
LOGGER_SRCS = $(wildcard src/logger/*.cpp) $(COMMON_SRCS)
LOGGER_OBJS = $(patsubst %.cpp,$(BUILD)/%.o,$(LOGGER_SRCS))
SETUP_SRCS = $(wildcard src/setup/*.cpp) src/common/kv_file.cpp src/common/setup_state.cpp src/common/setup_form.cpp
SETUP_OBJS = $(patsubst %.cpp,$(BUILD)/%.o,$(SETUP_SRCS))

all: $(LOGGER) $(WEB) $(DISPLAY) $(SETUP)

$(LOGGER): $(LOGGER_OBJS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $^ $(shell pkg-config --libs $(LOGGER_PKGS)) -pthread

$(PAGEC): src/tools/pagec.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD)/gen/%.page.cpp $(BUILD)/gen/%.page.h: web/pages/%.page $(PAGEC)
	@mkdir -p $(@D)
	$(PAGEC) $< $(BUILD)/gen/$*.page.cpp $(BUILD)/gen/$*.page.h

# Generated pages.
$(BUILD)/gen/%.o: $(BUILD)/gen/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

# Every web object may include a generated header, so they wait for all of them.
$(BUILD)/%.o: %.cpp | $(GEN_HDRS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP $(subst -I,-isystem ,$(shell pkg-config --cflags $(LOGGER_PKGS) $(WEB_PKGS) $(DISPLAY_PKGS))) -c -o $@ $<

$(WEB): $(WEB_OBJS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $^ $(shell pkg-config --libs $(WEB_PKGS)) -pthread

$(SETUP): $(SETUP_OBJS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $^

$(DISPLAY): $(DISPLAY_OBJS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $^ $(shell pkg-config --libs $(DISPLAY_PKGS))

test: all
	@mkdir -p $(PURR_TMP)
	tools/check-includes.sh
	$(LOGGER) --selftest
	$(WEB) --selftest --wheels data/wheels.conf
	$(DISPLAY) --selftest --wheels data/wheels.conf
	$(SETUP) --selftest

# Render every e-paper screen from a fake-history database into preview/ (no hardware needed).
preview: $(DISPLAY)
	@mkdir -p $(PURR_TMP)
	$(DISPLAY) --wheels data/wheels.conf --config /dev/null --demo-db $(PURR_TMP)/demo.db --preview preview

# The same renders, for the README.
media: $(DISPLAY)
	@mkdir -p $(PURR_TMP)
	$(DISPLAY) --wheels data/wheels.conf --config /dev/null --demo-db $(PURR_TMP)/demo.db --preview docs/media

# The web dashboard on :8081 over the same fake history, from the checkout.
serve: all
	@mkdir -p $(PURR_TMP)
	$(DISPLAY) --wheels data/wheels.conf --demo-db $(PURR_TMP)/demo.db
	$(WEB) --db $(PURR_TMP)/demo.db --wheels data/wheels.conf --config /dev/null --port 8081

deb:
	packaging/build-deb.sh

clean:
	rm -rf $(BUILD) $(LOGGER) $(WEB) $(DISPLAY) $(SETUP) $(PAGEC)

.PHONY: all test preview media serve deb clean

-include $(WEB_OBJS:.o=.d) $(DISPLAY_OBJS:.o=.d) $(LOGGER_OBJS:.o=.d) $(SETUP_OBJS:.o=.d)
