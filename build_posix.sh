# build helper for Linux
# to specify a lua version, pass "luaXX" as first argument

source ./build_common.sh
source ./extra_pkgconf.sh
shopt -s nocasematch

LIBS="$LIBS -pthread -lssl -lcrypto -lz"

if [ -z "$CC_IS_CLANG" ]; then
    if [[ "$OS_NAME" == "windows" ]]; then
        STATIC_LIBS="-Wl,-Bstatic $STATIC_LIBS -static-libstdc++" # always link libstdc++ static on windows
    else
        STATIC_LIBS="-Wl,-Bstatic $STATIC_LIBS"
    fi
else
    if [[ "$OS_NAME" == "linux" ]]; then
        STATIC_LIBS="-Wl,-Bstatic $STATIC_LIBS" # Linux clang likely uses system libstdc++
    else
        STATIC_LIBS="-Wl,-Bstatic $STATIC_LIBS -lc++" # always link libc++ static on non-linux
    fi
fi

if [[ "$2" == "static" ]] || [[ "$3" == "static" ]]; then
    # static build
    EXTRA_LIBS="-Wl,-Bstatic $EXTRA_LIBS_STATIC"
else
    # dynamic build (default)
    EXTRA_LIBS="-Wl,-Bdynamic $EXTRA_LIBS_DYNAMIC"
fi

if [[ "$2" == "debug" ]] || [[ "$3" == "debug" ]]; then
    CFLAGS="-Og -g -DAPCLIENT_DEBUG $CFLAGS"
else
    CFLAGS="-Os -DNDEBUG -DAP_NO_SCHEMA $CFLAGS"
fi

CFLAGS="$CFLAGS -fvisibility=hidden -std=$STD -Wall -Wextra -Werror -Wno-deprecated-declarations $EXTRA_CFLAGS"

OUT="$FILENAME"

# prefer static openssl
"$CXX" $CFLAGS $DEFINES $INCLUDE_DIRS -shared -o "$OUT" -fPIC src/lua-apclientpp.cpp $DYNAMIC_LIBS $EXTRA_LIBS $STATIC_LIBS -Wl,-Bstatic $LIBS > /dev/null 2>&1
if [ $? -ne 0 ]; then
    # try again with dynamic libssl
    "$CXX" $CFLAGS $DEFINES $INCLUDE_DIRS -shared -o "$OUT" -fPIC src/lua-apclientpp.cpp $DYNAMIC_LIBS $EXTRA_LIBS $STATIC_LIBS -Wl,-Bdynamic $LIBS
    exit $?
fi
strip "$OUT"
