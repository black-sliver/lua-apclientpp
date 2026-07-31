# This file sets EXTRA_* variables using pkg-config.
# Use source or . to include it.
#
# shellcheck shell=ksh
# shellcheck disable=SC2034,SC2181,SC2046,SC2086
# SC2034: Variables are used by the file sourcing this.
# SC2181: Using $? for readability.
# SC2046,SC2086: $PKGCONFIG_CONF, when used, needs to be expanded. Too many instances to suppress per occurrence.

# get from first arg, fall back to default
[ -z "$LUA" ] && LUA="$1"
[ -z "$LUA" ] && LUA="lua"

if [ "$LUA" = "lua" ]; then
    # try to get the version from the standard include
    LUA="$(printf '#include <lua.h>\nLUA_VERSION\n' | $CC $(pkg-config --cflags lua) -P -E - | tail -n1 | sed 's/[\t "]//g' | tr '[:upper:]' '[:lower:]')"
fi

# NOTE: we keep the '.' because MSYS requires it

EXTRA_CFLAGS="$(pkg-config $PKGCONFIG_CONF --cflags "$LUA")"
EXTRA_LIBS_STATIC="$(pkg-config $PKGCONFIG_CONF --libs "$LUA")"
if [ $? -ne 0 ]; then
    if [ "$LUA" = "lua5.4" ]; then # try without version
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

if [ "$OS_NAME" = "windows" ]; then
    # win32 has to link to the dll
    EXTRA_LIBS_DYNAMIC="$EXTRA_LIBS_STATIC"
    DYNAMIC_LIBS="-lcrypt32 -lws2_32"
elif [ "$OS_NAME" = "macos" ]; then
    # macos has to be forced to ignore undefined symbols
    EXTRA_LIBS_DYNAMIC="-Wl,-undefined,dynamic_lookup"
    # FIXME: linking to brew's libssl requires a recent macos to actually load the dylib
fi

echo "Libs for static build: $EXTRA_LIBS_STATIC"
echo "Libs for dynamic build: $EXTRA_LIBS_DYNAMIC"
