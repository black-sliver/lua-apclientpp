"""Run some simple tests with Lua EXE, if available."""

from glob import glob
from subprocess import run
from typing import Callable
from unittest import TestCase, skipUnless

from .server import APServer
from .util import lua_exe


@skipUnless(lua_exe, "Lua is not installed")
class TestExe(TestCase):
    server: APServer

    @classmethod
    def setUpClass(cls) -> None:
        cls.server = APServer()
        cls.server.port = 38281
        try:
            cls.server.start()
        except OSError:
            pass  # any server on 38281 will suffice

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
            res = run([lua_exe, test_fn])
            self.assertEqual(res.returncode, 0, f"{test_fn} failed")
        return f

    test_name = filename.rsplit("/", 1)[-1].split(".")[0]
    setattr(TestExe, test_name, make(filename))
