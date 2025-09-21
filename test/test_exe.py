"""Run some simple tests with Lua EXE, if available."""

from glob import glob
from subprocess import run
from typing import Callable
from unittest import TestCase, skipUnless

from .server import APServer
from .util import lua_exe, TimeoutLoop


@skipUnless(lua_exe, "Lua is not installed")
class TestExe(TestCase):
    server: APServer

    @classmethod
    def setUpClass(cls) -> None:
        cls.server = APServer()
        cls.server.start()
        for _ in TimeoutLoop(lambda: cls.server.port == 0):
            pass

    @classmethod
    def tearDownClass(cls) -> None:
        try:
            cls.server.stop()
        except AssertionError:
            pass


for filename in glob("test/test_*.lua"):
    def make(test_fn: str) -> Callable[[TestExe], None]:
        def f(self: TestExe) -> None:
            assert lua_exe
            res = run([lua_exe, test_fn, f"ws://127.0.0.1:{self.server.port}"])
            self.assertEqual(res.returncode, 0, f"{test_fn} failed")
        return f

    test_name = filename.rsplit("/", 1)[-1].split(".")[0]
    setattr(TestExe, test_name, make(filename))
