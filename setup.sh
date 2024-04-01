#!/bin/bash
unset CDPATH
cd "$( dirname "${BASH_SOURCE[0]}" )"
BASEDIR="$( pwd )"
git submodule update --init --recursive
mkdir local

export CC=gcc
export CXX=g++
#export CC=/usr/local/opt/llvm/bin/clang
#export CXX=/usr/local/opt/llvm/bin/clang++

export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"
export LIBRARY_PATH="/usr/local/lib:$LIBRARY_PATH"

# INSTALL hdf5
mkdir local/hdf5
HDFDIR="$BASEDIR/local/hdf5"
mkdir hdf5
cd hdf5
curl -L https://www.hdfgroup.org/package/hdf5-1-12-0-tar-gz/?wpdmdl=14582 -o hdf5.tgz
tar -xzf hdf5.tgz
rm hdf5.tgz
cd "$( ls | head -n 1 )"
CPPFLAGS="-I/usr/local/include" CXXFLAGS="-I/usr/local/include" ./configure --prefix="$HDFDIR" --enable-hl
make install

# INSTALL netcdf-c
NCCDIR="$BASEDIR/local/netcdf-c"
mkdir "$NCCDIR"
cd "$BASEDIR/netcdf-c"
echo $PWD
autoreconf -ivf
CPPFLAGS="-I$HDFDIR/include -I/usr/local/include" CXXFLAGS="-I$HDFDIR/include -I/usr/local/include" LDFLAGS="-L$HDFDIR/lib" ./configure --prefix="$NCCDIR"
make install

export LD_LIBRARY_PATH="$NCCDIR/lib:$LD_LIBRARY_PATH"
export LIBRARY_PATH="$NCCDIR/lib:$LIBRARY_PATH"

# INSTALL netcdf-cpp
NCCXXDIR="$BASEDIR/local/netcdf-cxx4"
mkdir "$NCCXXDIR"
cd "$BASEDIR/netcdf-cxx4"
autoreconf -if
CPPFLAGS="-I$NCCDIR/include -I$HDFDIR/include -I/usr/local/include" CXXFLAGS="-std=c++11 -I$NCCDIR/include -I$HDFDIR/include -I/usr/local/include" LDFLAGS="-L$NCCDIR/lib -L$HDFDIR/lib" LIBS="-lnetcdf" NC_LIBS="-lnetcdf -lnetcdf_c++4" ./configure --prefix="$NCCXXDIR"
make install
