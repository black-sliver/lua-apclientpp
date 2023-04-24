# this file sets EXTRA_* variables using pkg-config

# get from first arg, fall back to default
[ -z "$LUA" ] && LUA="$1"
[ -z "$LUA" ] && LUA="lua"

if [[ "$LUA" == "lua" ]]; then
    # try to get the version from the standard include
    LUA="$(echo -e '#include <lua.h>\nLUA_VERSION' | $CC $(pkg-config --cflags lua) -E - | tail -n1 | sed 's/[\t "]//g' | tr '[:upper:]' '[:lower:]')"
fi

# NOTE: we keep the '.' because MSYS requires it

EXTRA_CFLAGS="$(pkg-config $PKGCONFIG_CONF --cflags $LUA)"
EXTRA_LIBS_STATIC="$(pkg-config $PKGCONFIG_CONF --libs $LUA)"
if [ $? -ne 0 ]; then
    if [[ "$LUA" == "lua5.4" ]]; then # try without version
        EXTRA_CFLAGS="$(pkg-config $PKGCONFIG_CONF --cflags lua)"
        EXTRA_LIBS_STATIC="$(pkg-config $PKGCONFIG_CONF --libs lua)"
        if [ $? -ne 0 ]; then
            echo "no pkg-config --cflags lua"
            exit 1
        fi
    fi
    if [ $? -ne 0 ]; then
        echo "no pkg-config --cflags $LUA"
        exit 1
    fi
fi

if [[ "$OS_NAME" == "windows" ]]; then
    # win32 has to link to the dll
    EXTRA_LIBS_DYNAMIC="$EXTRA_LIBS_STATIC"
    DYNAMIC_LIBS="-lcrypt32 -lws2_32"
elif [[ "$OS_NAME" == "macos" ]]; then
    # macos has to be forced to ignore undefined symbols
    EXTRA_LIBS_DYNAMIC="-Wl,-undefined,dynamic_lookup"
    # FIXME: linking to brew's libssl requires a recent macos to actually load the dylib
fi

echo "Libs for static build: $EXTRA_LIBS_STATIC"
echo "Libs for dynamic build: $EXTRA_LIBS_DYNAMIC"
