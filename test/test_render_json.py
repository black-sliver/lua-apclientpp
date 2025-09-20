from .bases import ClientTestCase
from .util import LuaError


class RenderJsonTest(ClientTestCase):
    def setUp(self) -> None:
        super().setUp()
        self.connect()

    def test_bad_self(self) -> None:
        with self.assertRaises(LuaError):
            self.client["render_json"]()

    def test_bad_data(self) -> None:
        with self.assertRaises(LuaError):
            self.call("render_json", 1)

    def test_bad_format(self) -> None:
        nodes = self.lua.table(self.lua.table())
        with self.assertRaises(LuaError):
            self.call("render_json", nodes, "bad")

    def test_fallback_format(self) -> None:
        nodes = self.lua.table(self.lua.table(text = "Test"))
        fmt1 = self.client["RenderFormat"]["TEXT"]
        text1 = self.call("render_json", nodes, fmt1)
        self.assertTrue(text1)
        fmt2 = 999
        text2 = self.call("render_json", nodes, fmt2)
        self.assertEqual(text1, text2)

    def test_unsupported_format(self) -> None:
        nodes = self.lua.table(self.lua.table())
        fmt = self.client["RenderFormat"]["HTML"]
        with self.assertRaises(LuaError):
            res = self.call("render_json", nodes, fmt)
            self.fail(f"render_json returned '{res}' instead of error")

    def test_empty_data(self) -> None:
        empty = self.client["EMPTY_ARRAY"]
        res = self.call("render_json", empty)
        self.assertEqual(res, "")

    # TODO: test plain, ansi, invalid
