#!/bin/bash

## Run this script to build proxygen and run the tests. If you want to
## install proxygen to use in another C++ project on this machine, run
## the sibling file `reinstall.sh`.

#Backup system state before to modify it
set -e
START_DIR=$(pwd)
CPPFLAGS_BAK=${CPPFLAGS}
LDFLAGS_BAK=${LDFLAGS}
trap 'export CPPFLAGS=${CPPFLAGS_BAK};export LDFLAGS=${LDFLAGS_BAK}; export VERBOSE=${VERBOSE_BAK};cd $START_DIR' EXIT

# Parse args
JOBS=8
USAGE="$0"
USAGE="\n$USAGE [-j/--jobs num_jobs]"
USAGE="\n$USAGE [-f/--force deps re-fetch/re-build]"
USAGE="\n$USAGE [-r/--release compile in release mode]"
USAGE="\n$USAGE [-a/--asan enable address sanitizer (forces debug mode)]"
USAGE="\n$USAGE [-v/--verbose enable VERBOSE]"
USAGE="\n$USAGE [-p/--purify compile openssl in a way to suppress valgrind pain]"

#Default values
BUILD_TYPE=Debug
PURIFY=""

while [ "$1" != "" ]; do
  case $1 in
	-j | --jobs ) shift
				JOBS=$1
				;;
	-f | --force )
				FORCE=yes
				;;
	-v | --verbose )
				VERBOSE_BAK=${VERBOSE}
				export VERBOSE=1
				;;
	-r | --release )
				BUILD_TYPE=Release
				;;
	-a | --asan )
				BUILD_TYPE=Asan
				;;					  
	-p | --purify )
				PURIFY="-DPURIFY"
				;;					  
	* )			echo "${USAGE}"
				exit 1
esac
shift
done



WORKING_DIR="$(pwd)/$(dirname "$0")"
BUILD_DIR="${WORKING_DIR}/../build"

# Must execute from the build folder
cd "${BUILD_DIR}" || (echo "fatal: Can't access build folder"; exit -1)
echo "Cleaning build folder";
rm -rf ./*

if [ "x${FORCE}" == "xyes" ]; then
	rm -rf "${BUILD_DIR}/../deps/*"
fi

#Get Boost
#BOOST_ROOT_DIR="${BUILD_DIR}/../deps/boost"
#if [ ! -e ${BOOST_ROOT_DIR} ]; then
#    cd "${BUILD_DIR}/../deps"||exit
#    wget http://sourceforge.net/projects/boost/files/boost/1.61.0/boost_1_61_0.tar.bz2
#    tar xvf boost_1_61_0.tar.bz2
#    mkdir boost || exit
#    cd boost_1_61_0 || exit
#    ./bootstrap.sh --prefix=../boost/ || ( echo "fatal: boost boostrap failed"; exit )
#    ./b2 -j$JOBS || ( echo "fatal: boost build failed"; exit )
#    ./b2 install || ( echo "fatal: boost install failed"; exit )
#fi
#export BOOST_ROOT=$BOOST_ROOT_DIR
#echo "Boost is installed at ${BOOST_ROOT_DIR}"

#Get UVW
UVW_ROOT_DIR="${BUILD_DIR}/../deps/uvw"
if [ ! -e "${UVW_ROOT_DIR}/src/uvw.hpp" ] || [ "x${FORCE}" == "xyes" ]; then
	cd "${BUILD_DIR}/../deps"||exit
	git submodule init uvw
	git submodule update uvw
	cd ${UVW_ROOT_DIR} || exit
	./deps.sh
fi

# Get GTest
GTEST_ROOT_DIR="${BUILD_DIR}/../deps/gtest/googletest"
if [ ! -e "${GTEST_ROOT_DIR}/libgtest.a" ] || [ "x${FORCE}" == "xyes" ]; then
	cd "${BUILD_DIR}/../deps"||exit
	git submodule init gtest 
	git submodule update gtest
	cd "${GTEST_ROOT_DIR}"||exit
	cmake .
	make || ( echo "fatal: gtest build failed"; exit )
fi
echo "GTest Is installed at ${GTEST_ROOT_DIR}"

# Get GMock
GMOCK_ROOT_DIR="${BUILD_DIR}/../deps/gtest/googlemock"
if [ ! -e "${GMOCK_ROOT_DIR}/libgmock.a" ] || [ "x${FORCE}" == "xyes" ]; then
	cd "${BUILD_DIR}/../deps"||exit
	git submodule init gtest 
	git submodule update gtest
	cd "${GMOCK_ROOT_DIR}"||exit
	cmake .
	make || ( echo "fatal: gmock build failed"; exit )
fi
echo "GMock Is installed at ${GMOCK_ROOT_DIR}"

# Get Cynnypp
CYNPP_ROOT_DIR="${BUILD_DIR}/../deps/cynnypp"
if [ ! -e "${CYNPP_ROOT_DIR}/build/lib*" ] || [ "x${FORCE}" == "xyes" ]; then
	cd "${BUILD_DIR}/../deps"||exit
	git submodule init cynnypp
	git submodule update cynnypp
	mkdir -p "${CYNPP_ROOT_DIR}/build"||exit
	cd "${CYNPP_ROOT_DIR}/build"||exit
	LIBRARY_TYPE=shared cmake ..
	make async_fs_shared || ( echo "fatal: Cynnypp build failed"; exit )
fi
echo "Cynnypp Is installed at ${CYNPP_ROOT_DIR}"

# Get OpenSSL 1.0.2
OPENSSL_ROOT_DIR="${BUILD_DIR}/../deps/openssl"
if [ ! -e "${OPENSSL_ROOT_DIR}/libssl.so" ] || [ "x${FORCE}" == "xyes" ]; then
	cd "${BUILD_DIR}/../deps"||exit
	git submodule init openssl
	git submodule update openssl
	cd "${OPENSSL_ROOT_DIR}"||exit
	./config shared $PURIFY
	make || ( echo "fatal: openssl build failed"; exit )
fi
echo "OpenSSL 1.0.2 Is installed at ${OPENSSL_ROOT_DIR}"

# Get NGHTTP2
NGHTTP2_ROOT_DIR="${BUILD_DIR}/../deps/nghttp2"
if [ ! -e "${NGHTTP2_ROOT_DIR}/build/lib/libnghttp2.a" ] || [ "x${FORCE}" == "xyes" ]; then
	cd "${BUILD_DIR}/../deps"||exit
	git submodule init nghttp2 
	git submodule update nghttp2 
	cd "${NGHTTP2_ROOT_DIR}"||exit
	autoreconf -i
	automake
	autoconf
	OPENSSL_CFLAGS="-I${OPENSSL_ROOT_DIR}/include/" OPENSSL_LIBS="-L${OPENSSL_ROOT_DIR} -lssl -lcrypto" \
		./configure --enable-asio-lib=yes --enable-lib-only --prefix="$(pwd)/build"
	make || ( echo "fatal: nghttp2 build failed"; exit )
	make install
fi
echo "nghttp2 Is installed at ${NGHTTP2_ROOT_DIR}"

# Get CITYHASH
CITYHASH_ROOT_DIR="${BUILD_DIR}/../deps/cityhash"
if [ ! -e "${NGHTTP2_ROOT_DIR}/build/lib/libcityhash.a" ] || [ "x${FORCE}" == "xyes" ]; then
	cd "${BUILD_DIR}/../deps"||exit
	git submodule init cityhash 
	git submodule update cityhash 
	cd "${CITYHASH_ROOT_DIR}"||exit
	./configure --enable-sse4.2 --prefix="$(pwd)/build"
	make CXXFLAGS="-g -O3 -msse4.2" || ( echo "fatal: cityhash build failed"; exit )
	make install
fi
echo "cityhash Is installed at ${CITYHASH_ROOT_DIR}"

# Get SPDLOG
SPDLOG_ROOT_DIR="${BUILD_DIR}/../deps/spdlog"
if [ ! -e "${SPDLOG_ROOT_DIR}/include/spdlog" ] || [ "x${FORCE}" == "xyes" ]; then
	cd "${BUILD_DIR}/../deps"||exit
	git submodule init spdlog || exit 
	git submodule update spdlog || exit
	cd "${SPDLOG_ROOT_DIR}"||exit
fi
echo "spdlog has been fetched at ${SPDLOG_ROOT_DIR}"

#Build doormat
cd "${BUILD_DIR}"
echo "Configuring doormat";
CONFIG_COMMAND="cmake"
CONFIG_COMMAND="${CONFIG_COMMAND}  -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}"
CONFIG_COMMAND="${CONFIG_COMMAND}  -DGMOCK_ROOT=${GMOCK_ROOT_DIR}"
if [ x${BUILD_TYPE} == "xAsan" ]; then
	CONFIG_COMMAND="${CONFIG_COMMAND}  -DENABLE_SAN=1"
elif [ x${BUILD_TYPE} == "xRelease" ]; then
	CONFIG_COMMAND="${CONFIG_COMMAND}  -DCMAKE_BUILD_TYPE=Release"
fi


CONFIG_COMMAND="${CONFIG_COMMAND}  .."
[[ x${VERBOSE} == "x1" ]] && echo "${CONFIG_COMMAND}"
$CONFIG_COMMAND

echo "Building doormat";
make "-j${JOBS}" || ( echo "fatal: doormat build failed"; exit -1 )
