INCLUDE_DIRS="-I subprojects/json/include -I subprojects/valijson/include -I subprojects/wswrap/include -I subprojects/apclientpp -Isubprojects/asio/include -Isubprojects/websocketpp -Isubprojects"
DEFINES="-DASIO_STANDALONE -DWSWRAP_SEND_EXCEPTIONS"
NAME="lua-apclientpp"


case $(uname | tr '[:upper:]' '[:lower:]') in
    linux*)
        OS_NAME=linux
        ;;
    darwin*)
        OS_NAME=macos
        ;;
    msys*)
        OS_NAME=windows
        ;;
    cygwin*)
        OS_NAME=windows
        ;;
    mingw*)
        OS_NAME=windows
        ;;
    *)
        echo "Unknown OS: $(uname | tr '[:upper:]' '[:lower:]')"
esac

echo "Detected OS: $OS_NAME"


if [[ "$OS_NAME" == "windows" ]]; then
    FILENAME="$NAME.dll"
elif [[ "$OS_NAME" == "darwin" ]]; then
    FILENAME="$NAME.dylib"
else
    FILENAME="$NAME.so"
fi

# guess the c compiler
if [ -z "$CC" ]; then
    if [[ ! -z "$CXX" ]]; then
        CC="$CXX"
    elif [ -x "$(which gcc)" ]; then
        CC="gcc"
        CXX="g++"
    elif [ -x "$(which clang)" ]; then
        CC="clang"
        CXX="clang++"
    else
        CC="cc"
        CXX="cpp"
    fi
elif [[ -z "$CXX" ]]; then
    echo "CC is set. Please also set CXX!"
    exit 1
fi

"$CC" --version | grep -q "clang" && CC_IS_CLANG=1
