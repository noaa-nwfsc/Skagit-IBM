#CCEXE = /usr/local/opt/llvm/clang++
CCEXE = g++
CFLAGS = -Wall -g -std=c++11

CC = $(CCEXE) $(CFLAGS)

WXFLAGS = $(shell wx-config --cxxflags)

WXLIBS = $(shell wx-config --libs)

RAPIDJSON = -Irapidjson/include

NETCDF = -Ilocal/netcdf-cxx4/include -Ilocal/netcdf-c/include -Ilocal/hdf5/include

ABSLIB_NCCPP = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))local/netcdf-cxx4/lib

ABSLIB_NCC = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))local/netcdf-c/lib

ifeq ($(shell uname -s), Darwin)
	DL_EXT = dylib
else
	DL_EXT = so
endif

LINKFLAGS = -v -Llocal/netcdf-cxx4/lib -Llocal/netcdf-c/lib -lnetcdf_c++4 -lnetcdf -lpthread -pthread -Wl,-rpath,$(ABSLIB_NCCPP) -Wl,-rpath,$(ABSLIB_NCC) local/netcdf-cxx4/lib/libnetcdf_c++4.$(DL_EXT) local/netcdf-c/lib/libnetcdf.$(DL_EXT)

OBJ = $(addprefix build/, model.o hydro.o load.o fish.o map.o util.o env_sim.o map_gen.o)

headless: $(OBJ) build/headless.o bin
	$(CC) $(OBJ) build/headless.o -o bin/headless $(LINKFLAGS)

gui: $(OBJ) build/gui.o bin
	$(CC) $(WXLIBS) $(OBJ) build/gui.o -o bin/gui $(LINKFLAGS)

build/gui.o: src/gui.cpp src/model.h src/fish.h build
	$(CC) $(WXFLAGS) -c src/gui.cpp -o build/gui.o

bin:
	mkdir bin

build:
	mkdir build

build/headless.o: src/headless.cpp src/model.h build
	$(CC) -c src/headless.cpp -o build/headless.o

build/model.o: src/model.cpp src/model.h src/load.h src/fish.h src/map.h build
	$(CC) $(RAPIDJSON) $(NETCDF) -c src/model.cpp -o build/model.o

build/hydro.o: src/hydro.cpp src/hydro.h src/map.h build
	$(CC) -c src/hydro.cpp -o build/hydro.o

build/load.o: src/load.cpp src/load.h src/map.h build
	$(CC) $(NETCDF) -c src/load.cpp -o build/load.o

build/fish.o: src/fish.cpp src/fish.h src/model.h src/map.h src/util.h build
	$(CC) -c src/fish.cpp -o build/fish.o

build/map.o: src/map.cpp src/map.h build
	$(CC) -c src/map.cpp -o build/map.o

build/util.o: src/util.cpp src/util.h build
	$(CC) -c src/util.cpp -o build/util.o

build/env_sim.o: src/env_sim.cpp src/env_sim.h src/map.h build
	$(CC) -c src/env_sim.cpp -o build/env_sim.o

build/map_gen.o: src/map_gen.cpp src/map_gen.h src/map.h build
	$(CC) -c src/map_gen.cpp -o build/map_gen.o

clean:
	rm -rdf bin/* build/*
