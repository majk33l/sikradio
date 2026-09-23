CXX := g++
TARGET := sikradio
BDIR := build

ifneq ($(wildcard src/*.cpp),)
SDIR := src
else
SDIR := .
endif

SRCS := $(wildcard $(SDIR)/*.cpp)
OBJS := $(patsubst $(SDIR)/%.cpp,$(BDIR)/%.o,$(SRCS))

CXXFLAGS := \
	-std=c++23 \
	-Wall -Wextra -Wpedantic \
	-Wshadow -Wconversion -Wsign-conversion \
	-Wnull-dereference -Wdouble-promotion \
	-Wformat=2 -Wundef \
	-fstack-protector-strong \
	-O2 -DNDEBUG

LDFLAGS :=

# --- macOS / Homebrew OpenSSL detection -------------------------------
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
	OPENSSL_PREFIX := $(shell if [ -d /opt/homebrew/opt/openssl@3 ]; then echo /opt/homebrew/opt/openssl@3; elif [ -d /usr/local/opt/openssl@3 ]; then echo /usr/local/opt/openssl@3; elif [ -d /opt/homebrew/opt/openssl ]; then echo /opt/homebrew/opt/openssl; elif [ -d /usr/local/opt/openssl ]; then echo /usr/local/opt/openssl; else brew --prefix openssl@3 2>/dev/null || brew --prefix openssl 2>/dev/null; fi)
	ifeq ($(OPENSSL_PREFIX),)
$(warning OpenSSL not found via Homebrew. Run: brew install openssl@3)
	else
		CXXFLAGS += -I$(OPENSSL_PREFIX)/include
		LDFLAGS  += -L$(OPENSSL_PREFIX)/lib
	endif
endif
# ------------------------------------------------------------------------

LDFLAGS += -lssl -lcrypto

# --- static analysis ---------------------------------------------------
CLANG_TIDY := clang-tidy
TIDY_CHECKS := -checks=bugprone-*,clang-analyzer-*,cert-*,performance-*,portability-*,readability-*,-readability-magic-numbers,-readability-braces-around-statements,-portability-avoid-pragma-once

.PHONY: all clean tidy

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BDIR):
	mkdir -p $@

$(BDIR)/%.o: $(SDIR)/%.cpp | $(BDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

tidy:
	$(CLANG_TIDY) $(TIDY_CHECKS) $(SRCS) -- $(CXXFLAGS)

clean:
	rm -rf $(BDIR) $(TARGET)