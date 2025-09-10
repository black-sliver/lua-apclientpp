from .bases import APITestCase
from .util import LuaTable


class TestEnums(APITestCase):
    def assert_is_uint_enum(self, o: LuaTable) -> None:
        self.assertGreater(sum(1 for _ in o.keys()), 0, "Table is empty")
        for k, v in o.items():
            self.assertIsInstance(k, str)
            self.assertIsInstance(v, int)
            self.assertGreaterEqual(v, 0)

    def assert_is_flags(self, o: LuaTable) -> None:
        from math import log2

        self.assertGreater(sum(1 for _ in o.keys()), 0, "Table is empty")
        for k, v in o.items():
            self.assertIsInstance(k, str)
            assert isinstance(k, str)  # make type checker happy
            self.assertIsInstance(v, int)
            if "none" in k.lower() or "all" in k.lower():
                continue
            self.assertAlmostEqual(log2(v) % 1, 0)

    def assert_equal_table(self, one: LuaTable, two: LuaTable) -> None:
        keys1 = sorted(one.keys())
        keys2 = sorted(two.keys())
        self.assertEqual(keys1, keys2)
        for k in keys1:
            self.assertEqual(one[k], two[k])

    def test_client_status(self) -> None:
        self.assert_is_uint_enum(self.apclient["ClientStatus"])
        self.assert_equal_table(self.apclient["ClientStatus"], self.api["ClientStatus"])

    def test_render_format(self) -> None:
        self.assert_is_uint_enum(self.apclient["RenderFormat"])
        self.assert_equal_table(self.apclient["RenderFormat"], self.api["RenderFormat"])

    def test_item_flags(self) -> None:
        self.assert_is_flags(self.apclient["ItemFlags"])
        self.assert_equal_table(self.apclient["ItemFlags"], self.api["ItemFlags"])

    def test_state(self) -> None:
        self.assert_is_uint_enum(self.apclient["State"])
        self.assert_equal_table(self.apclient["State"], self.api["State"])
