ERL_INCLUDE_DIR ?= $(shell erl -noshell -eval 'io:format("~s/erts-~s/include", [code:root_dir(), erlang:system_info(version)]), halt().')
CXX ?= c++
CXXFLAGS ?= -O3 -std=c++11 -Wall -Wextra -fPIC -DNDEBUG -DUSE_COEFFICIENTS
LDFLAGS ?= -shared

RIPSER_CPP ?= vendor/ripser.cpp
TARGET = build/ripser_nif.so
SOURCES = ripser_nif.cpp

ifeq ($(shell uname -s),Darwin)
	LDFLAGS = -dynamiclib -undefined dynamic_lookup
endif

all: $(TARGET)

$(TARGET): $(SOURCES) $(RIPSER_CPP)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -DRIPSER_CPP_INCLUDE=\"$(RIPSER_CPP)\" -I$(ERL_INCLUDE_DIR) $(SOURCES) $(LDFLAGS) -o $(TARGET)

clean:
	rm -rf build

.PHONY: all clean
