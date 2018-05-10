CC?=$(CROSS_COMPILE)gcc
CXX?=$(CROSS_COMPILE)g++
AR?= $(CROSS_COMPILE)ar
RANLIB?= $(CROSS_COMPILE)ranlib

CFLAGS+=-Wall -std=gnu11 -fPIC
CXXFLAGS+=-Wall -std=gnu++14 -fPIC

libname=benejson
ifeq ($(OS),Windows_NT)
	static_lib_name=$(libname).lib
	dynamic_lib_name=$(libname).dll
else
	static_lib_name=lib$(libname).a
	dynamic_lib_name=lib$(libname).so
endif

build_dir=build
bin_dir=bin
lib_dir=lib
src_dir=benejson

static_file=$(lib_dir)/$(static_lib_name)
dynamic_file=$(lib_dir)/$(dynamic_lib_name)

objects=$(build_dir)/benejson.o $(build_dir)/pull.o

all: $(static_file) $(dynamic_file)
	@echo Complete

header_install :
	mkdir -p $(INC_DEST)/benejson
	cp benejson/benejson.h benejson/pull.hh $(INC_DEST)/benejson

clean:
	rm -rf $(build_dir)
	rm -rf $(bin_dir)
	rm -rf $(lib_dir)

$(static_file) : $(objects)
	mkdir -p $(lib_dir)
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(dynamic_file) : $(objects)
	mkdir -p $(lib_dir)
	$(CC) $(LDFLAGS) -shared -o $@ $^

$(build_dir)/benejson.o : $(src_dir)/benejson.c $(src_dir)/benejson.h
	mkdir -p $(build_dir)
	$(CC) $(CFLAGS) -c -o $@ $(src_dir)/benejson.c

$(build_dir)/pull.o : $(src_dir)/pull.cpp $(src_dir)/pull.hh
	mkdir -p $(build_dir)
	$(CXX) $(CXXFLAGS) -c -o $@ $(src_dir)/pull.cpp
