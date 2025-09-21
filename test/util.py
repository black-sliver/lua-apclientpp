import os
import sys
import time
from abc import ABC, abstractmethod
from collections.abc import MutableMapping
from shutil import which
from typing import Any, Callable, Iterator, Optional, Union


__all__ = [
    "is_jit",
    "lua_exe",
    "lua_version",
    "LuaError",
    "LuaRuntime",
    "LuaTable",
    "LuaAPClient",
    "TimeoutLoop",
]

lua_version = os.environ.get("LUA_VERSION", None)
try:
    _orig_dlflags = sys.getdlopenflags()
    sys.setdlopenflags(258)
except AttributeError:
    pass

is_jit = False
lua_exe: Optional[str] = None

if lua_version == "5.4":
    from lupa.lua54 import LuaError, LuaRuntime
    lua_exe = which("lua5.4")
elif lua_version == "5.3":
    from lupa.lua53 import LuaError, LuaRuntime  # type: ignore[assignment]
    lua_exe = which("lua5.3")
elif lua_version == "5.2":
    from lupa.lua52 import LuaError, LuaRuntime  # type: ignore[assignment]
    lua_exe = which("lua5.2")
elif lua_version == "5.1":
    from lupa.lua51 import LuaError, LuaRuntime  # type: ignore[assignment]
    lua_exe = which("lua5.1")
elif lua_version == "JIT2.1":
    from lupa.luajit21 import LuaError, LuaRuntime  # type: ignore[assignment]
    lua_exe = which("luajit2") or which("luajit")
    is_jit = True
else:  # default to 5.4
    from lupa.lua54 import LuaError, LuaRuntime
    lua_exe = which("lua5.4")

try:
    sys.setdlopenflags(_orig_dlflags)  # noqa
except (AttributeError, NameError):
    pass

try:
    from .server import APServer
except ImportError:
    from server import APServer  # type: ignore[import-not-found, no-redef]

LuaTable = MutableMapping[Union[int, str], Any]


class LuaAPClient(MutableMapping[str, Any], ABC):
    @abstractmethod
    def __call__(self, uuid: str, game: str, host: str) -> LuaTable:
        ...


class TimeoutLoop:
    f: Callable[[], bool]
    timeout: float
    start: float

    def __init__(self, f: Callable[[], bool], timeout: float = .5) -> None:
        # NOTE: on Windows server we seem to need >100ms everywhere
        self.f = f
        self.timeout = timeout

    def __iter__(self) -> Iterator[None]:
        self.start = time.time()
        return self

    def __next__(self) -> None:
        res = self.f()
        if not res:
            raise StopIteration
        now = time.time()
        elapsed = now - self.start
        if elapsed >= self.timeout:
            raise TimeoutError()
        time.sleep(0.001)  # release the GIL
