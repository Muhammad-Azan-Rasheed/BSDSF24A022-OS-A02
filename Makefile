CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
VERSION ?= 3
CPPFLAGS += -DLS_VERSION=$(VERSION)
TARGET = bin/ls
SOURCE = src/ls-v1.0.0.c
OBJECT = obj/ls-v1.0.0.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECT) | bin
	$(CC) $(CFLAGS) -o $@ $^

$(OBJECT): $(SOURCE) | obj
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

bin obj:
	mkdir -p $@

clean:
	rm -f $(TARGET) $(OBJECT)
