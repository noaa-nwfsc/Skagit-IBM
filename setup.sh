#!/bin/bash
unset CDPATH
cd "$( dirname "${BASH_SOURCE[0]}" )" || exit
BASEDIR="$( pwd )"
git submodule update --init --recursive
mkdir local

export CC=gcc
export CXX=g++
#export CC=/usr/local/opt/llvm/bin/clang
#export CXX=/usr/local/opt/llvm/bin/clang++

export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"
export LIBRARY_PATH="/usr/local/lib:$LIBRARY_PATH"
HDFDIR="$BASEDIR/local/hdf5"

# INSTALL hdf5
mkdir local/hdf5
mkdir hdf5
cd hdf5 || exit
curl -L https://www.hdfgroup.org/package/hdf5-1-12-0-tar-gz/?wpdmdl=14582 -o hdf5.tgz
tar -xzf hdf5.tgz
rm hdf5.tgz
cd "$( ls | head -n 1 )" || exit
CPPFLAGS="-I/usr/local/include" CXXFLAGS="-I/usr/local/include" ./configure --prefix="$HDFDIR" --enable-hl
make install

# INSTALL netcdf-c
NC_DL_DIR="$BASEDIR/netcdf-c"
mkdir "$NC_DL_DIR"
cd "$NC_DL_DIR" || exit
NCC_VERSION=4.9.2
curl -L "https://github.com/Unidata/netcdf-c/archive/refs/tags/v$NCC_VERSION.tar.gz" -o netcdf.tgz
tar -xzf netcdf.tgz
NCC_SRC_DIR="$BASEDIR/netcdf-c/netcdf-c-$NCC_VERSION"
cd "$NCC_SRC_DIR" || exit
echo $PWD
autoreconf -ivf
NCCDIR="$BASEDIR/local/netcdf-c"
mkdir "$NCCDIR"
CPPFLAGS="-I$HDFDIR/include -I/usr/local/include" CXXFLAGS="-I$HDFDIR/include -I/usr/local/include" LDFLAGS="-L$HDFDIR/lib" ./configure --prefix="$NCCDIR"
make install

export LD_LIBRARY_PATH="$NCCDIR/lib:$LD_LIBRARY_PATH"
export LIBRARY_PATH="$NCCDIR/lib:$LIBRARY_PATH"

# INSTALL netcdf-cpp
# https://github.com/Unidata/netcdf-cxx4
NCCXX_DL_DIR="$BASEDIR/netcdf-cxx4"
mkdir "$NCCXX_DL_DIR"
cd "$NCCXX_DL_DIR" || exit
NCCXX_VERSION=4.3.1
NCCXX_DL_URL="https://downloads.unidata.ucar.edu/netcdf-cxx/$NCCXX_VERSION/netcdf-cxx4-$NCCXX_VERSION.tar.gz"
curl -L "$NCCXX_DL_URL" -o netcdf-cxx4.tgz
tar -xzf netcdf-cxx4.tgz
NCCXX_SRC_DIR="$NCCXX_DL_DIR/netcdf-cxx4-$NCCXX_VERSION"
cd "$NCCXX_SRC_DIR" || exit
NCCXXDIR="$BASEDIR/local/netcdf-cxx4"
mkdir "$NCCXXDIR"
autoreconf -ivf
mkdir build
cd build || exit
CPPFLAGS="-I$NCCDIR/include -I$HDFDIR/include -I/usr/local/include" CXXFLAGS="-std=c++11 -I$NCCDIR/include -I$HDFDIR/include -I/usr/local/include" LDFLAGS="-L$NCCDIR/lib -L$HDFDIR/lib" LIBS="-lnetcdf" NC_LIBS="-lnetcdf -lnetcdf_c++4" ../configure --prefix="$NCCXXDIR"
mv ./VERSION ./VERSION.txt  # this fixes a Mac bug in the netcdf-cxx4 4.3.1 distribution
make
make check
make install
