#!/usr/bin/env bash

function printUsage(){
    cat <<EOF
    Usage: $0 [options]

    -h| --help                  Show this help and exit.

    -c| --ccompiler     CC      Use CC compiler
    --cppcompiler       CXX     Use CXX compiler
    -j| --jobs          J       Compile with J jobs (default=4)

    --purify                    Build OpenSSL with the --purify option, to suppress valgrind pain

    --clean             DEP     Clean dependency DEP
    -w| --with          DEP     Build dependency DEP if not found
    -r| --rebuild       DEP     Clean and Build dependency DEP

                                Options (case insensitive) for --with --rebuild --clean:
                                    Cynnypp
                                    GoogleTest
                                    NgHttp2
                                    OpenSSL
                                    SpdLog
                                    All
    Note: if called with no parameters all dependencies will be built
EOF
}

function toLower(){
    local str="$@"
    local output=$(tr '[A-Z]' '[a-z]'<<<"${str}")
    echo ${output}
}

function sharedLibraryExtension(){
    local Kernel=`uname -s`
    if [ ${Kernel}=="Linux" ]; then
        echo "so"
    elif [ ${Kernel}=="Darwin" ]; then
        echo "dylib"
    fi
}

function staticLibraryExtension(){
    local Kernel=`uname -s`
    if [ ${Kernel}=="Linux" ]; then
        echo "a"
    elif [ ${Kernel}=="Darwin" ]; then
        echo "a"
    fi
}

function getDir(){
    # google docet - jump within the directory which contains this script
    local SOURCE="${BASH_SOURCE[0]}"
    local DIR=""
    while [ -h "${SOURCE}" ]; do
      DIR="$(cd -P "$(dirname "${SOURCE}")" && pwd)"
      SOURCE="$(readlink "${SOURCE}")"
      [[ ${SOURCE} != /* ]] && SOURCE="${DIR}/${SOURCE}"
    done
    # set aside the base dir for future references
    DIR="$(cd -P "$(dirname "${SOURCE}")" && pwd)"
    echo ${DIR}
}

function setWith(){
    local arg="$1"
    arg=$(toLower ${arg})
    case ${arg} in
        cynnypp)
            buildCynnyppFlag=true
            ;;
        googletest)
            buildGoogleTestFlag=true
            ;;
        nghttp2)
            buildNgHttp2Flag=true
            ;;
        openssl)
            buildOpenSSLFlag=true
            ;;
        spdlog)
            buildSpdLogFlag=true
            ;;
        all)
            buildCynnyppFlag=true
            buildGoogleTestFlag=true
            buildNgHttp2Flag=true;
            buildOpenSSLFlag=true
            buildSpdLogFlag=true
            ;;
        *)
            echo "Unrecognized parameter for \"--with\""
            printUsage
            exit 1
            ;;
    esac
}

function setClean(){
    local arg="$1"
    arg=$(toLower ${arg})
    case ${arg} in
        cynnypp)
            cleanCynnyppFlag=true
            ;;
        googletest)
            cleanGoogleTestFlag=true
            ;;
        nghttp2)
            cleanNghttp2Flag=true
            ;;
        openssl)
            cleanOpensslFlag=true
            ;;
        spdlog)
            cleanSpdlogFlag=true
            ;;
        all)
            cleanCynnyppFlag=true
            cleanGoogleTestFlag=true
            cleanNghttp2Flag=true;
            cleanOpensslFlag=true
            cleanSpdlogFlag=true
            ;;
        *)
            echo "Unrecognized parameter for \"--clean\""
            printUsage
            exit 1
            ;;
    esac
}


function cleanRepo(){
    local repo_dir="$1"
    local flag=$2
    if ${flag}; then
        if [ -d ${repo_dir}/build ]; then
            rm -rf ${repo_dir}/build/*
        fi
        if [ -f ${repo_dir}/deps.sh ]; then
            cd ${repo_dir} && ./deps.sh --clean all
        fi
    fi
    cd ${DIR}
}

function cleanOpenSSL(){
    local repo_dir="$1"
    local flag=$2
    if ${flag}; then
        cd ${repo_dir} && make clean
    fi
    cd ${DIR}
}

function buildCynnypp(){
    local repo_dir="$1"
    local flag=$2
    local cmake_fwd_args="$3"
    local JOBS=$4
    if ${flag}; then
        cd ${repo_dir} && git fetch -p &&
        if [ -f "build/lib/libcynpp_async_fs.$(sharedLibraryExtension)" ]; then
            echo "Cynnypp already built, skip rebuilding..."
        else
            echo "Cynnypp not found, building..."
            mkdir -p ${repo_dir}/build && cd ${repo_dir}/build && rm -rf * &&
            LIBRARY_TYPE=shared cmake ..
            cmake ${cmake_fwd_args} .. && make -j${JOBS} async_fs_shared
        fi
    fi
    cd ${DIR}
}

function buildNgHttp2(){
    local repo_dir="$1"
    local flag=$2
    local cmake_fwd_args="$3"
    local JOBS=$4
    if ${flag}; then
        cd ${repo_dir} && git fetch -p &&
        if [ -L "build/lib/libnghttp.$(sharedLibraryExtension)" ] &&
           [ -L "build/lib/libnghttp2_asio.$(sharedLibraryExtension)" ]; then
            echo "NgHttp2 already built, skip rebuilding..."
        else
            buildOpenSSL ${DIR}/deps/openssl true "${cmake_fwd_args}" ${JOBS}
            echo "NgHttp2 not found, building..."
            cd ${repo_dir}
            autoreconf -i
            automake
            autoconf
            OPENSSL_CFLAGS="-I${OPENSSL_ROOT_DIR}/include/" OPENSSL_LIBS="-L${OPENSSL_ROOT_DIR} -lssl -lcrypto" \
                ./configure --enable-asio-lib=yes --enable-lib-only --prefix="${repo_dir}/build"
            make -j${JOBS} || ( echo "fatal: nghttp2 build failed"; exit )
            make install
        fi
    fi
    cd ${DIR}
}

function buildGoogleTest(){
    local repo_dir="$1"
    local flag=$2
    local cmake_fwd_args="$3"
    local JOBS=$4
    if ${flag}; then
        cd ${repo_dir} && mkdir -p build && cd build
        local STATIC_LIB_EXT=$(staticLibraryExtension)
        if [ -f "googlemock/libgmock.${STATIC_LIB_EXT}" ] &&
           [ -f "googlemock/libgmock_main.${STATIC_LIB_EXT}" ] &&
           [ -f "googlemock/gtest/libgtest.${STATIC_LIB_EXT}" ] &&
           [ -f "googlemock/gtest/libgtest_main.${STATIC_LIB_EXT}" ]; then
            echo "GoogleTest already built, skip rebuilding..."
        else
            echo "GoogleTest not found, building..."
            cmake ${cmake_fwd_args} .. && make -j${JOBS}
        fi
    fi
    cd ${DIR}
}

function buildOpenSSL(){
    local repo_dir="$1"
    local flag=$2
    if ${flag}; then
        cd ${repo_dir}
        if [ -L "libssl.$(sharedLibraryExtension)" ] &&
           [ -L "libcrypto.$(sharedLibraryExtension)" ]; then
            echo "OpenSSL already built, skip rebuilding..."
        else
            echo "OpenSSL not found, building..."
            ./config "shared ${OPENSSL_PURIFY}"
            make -j${JOBS} || ( echo "fatal: openssl build failed"; exit )
        fi
    fi
    cd ${DIR}
}

function buildSpdLog(){
    local repo_dir="$1"
    local flag=$2
    local deps_fwd_args="$3"
    local JOBS=$4
    if ${flag}; then
        cd ${repo_dir} && git fetch -p
        # Header only
    fi
    cd ${DIR}
}

function showStatus(){
    echo "=================================="
    echo "${name} Dependency Status: "
    echo "=================================="
    echo "   Clean Cynnypp:                         $cleanCynnyppFlag"
    echo "   Clean GoogleTest:                      $cleanGoogleTestFlag"
    echo "   Clean NgHttp2:                         $cleanNghttp2Flag"
    echo "   Clean OpenSSL:                         $cleanOpensslFlag"
    echo "   Clean SpdLog:                          $cleanSpdlogFlag"

    echo ""
    echo "   Build (if necessary) Cynnypp:          $buildCynnyppFlag"
    echo "   Build (if necessary) GoogleTest:       $buildGoogleTestFlag"
    echo "   Build (if necessary) NgHttp2:          $buildNgHttp2Flag"
    echo "   Build (if necessary) OpenSSL:          $buildOpenSSLFlag"
    echo "   Build (if necessary) SpdLog:           $buildSpdLogFlag"
    echo ""
}

###  Start
JOBS=4
buildCynnyppFlag=false
buildGoogleTestFlag=false
buildNgHttp2Flag=false
buildOpenSSLFlag=false
buildSpdLogFlag=false

cleanCynnyppFlag=false
cleanGoogleTestFlag=false
cleanNghttp2Flag=false
cleanOpensslFlag=false
cleanSpdlogFlag=false

FORWARD_JOBS="-j ${JOBS}"
FORWARD_CC=""
FORWARD_CXX=""

if [ $# -le 0 ]; then
    echo "No parameters -> Everything necessary will be built"
    echo ""
    setWith "all"
fi

while [ "$1" != "" ]; do
    case $1 in
        -j | --jobs ) shift
            JOBS=$1
		    FORWARD_JOBS="-j $1"
            ;;
        -c | --ccompiler ) shift
	    	CC="-DCMAKE_C_COMPILER=$1"
 		    FORWARD_CC="-c $1"
		    ;;
        --cppcompiler ) shift
            CXX="-DCMAKE_CXX_COMPILER=$1"
            FORWARD_CXX="-p $1"
            ;;
        --purify ) shift
            OPENSSL_PURIFY="-DPURIFY"
            ;;
        -r | --rebuild) shift
            setClean $1
            setWith $1
            ;;
        -w | --with) shift
            setWith $1
            ;;
        --clean) shift
            setClean $1
            ;;
        -h | --help)
            printUsage
            exit 1
            ;;
        * )
            printUsage
            exit 1
    esac
    shift
done

DIR=$(getDir)
OPENSSL_ROOT_DIR=${DIR}/deps/openssl

cd ${DIR}

showStatus

FWD_DEPS_ARGUMENTS="$FORWARD_CXX $FORWARD_CC $FORWARD_JOBS"
FWD_CMAKE_ARGUMENTS="$CC $CXX -DCMAKE_BUILD_TYPE=Release"

### Apply submodules
git submodule update --init && git submodule sync

cleanRepo           ${DIR}/deps/cynnypp          ${cleanCynnyppFlag}
cleanRepo           ${DIR}/deps/gtest            ${cleanGoogleTestFlag}
cleanRepo           ${DIR}/deps/nghttp2          ${cleanNghttp2Flag}
cleanOpenSSL        ${OPENSSL_ROOT_DIR}          ${cleanOpensslFlag}


buildCynnypp        ${DIR}/deps/cynnypp          ${buildCynnyppFlag}         "${FWD_CMAKE_ARGUMENTS}"    ${JOBS}
buildGoogleTest     ${DIR}/deps/gtest            ${buildGoogleTestFlag}      "${FWD_CMAKE_ARGUMENTS}"    ${JOBS}
buildNgHttp2        ${DIR}/deps/nghttp2          ${buildNgHttp2Flag}         "${FWD_CMAKE_ARGUMENTS}"    ${JOBS}
buildOpenSSL        ${DIR}/deps/openssl          ${buildOpenSSLFlag}         "${FWD_CMAKE_ARGUMENTS}"    ${JOBS}
buildSpdLog         ${DIR}/deps/spdlog           ${buildSpdLogFlag}          "${FWD_CMAKE_ARGUMENTS}"    ${JOBS}

cd ${DIR}