"""Test all variations of ConnectUpdate command"""
from typing import List

from .bases import E2ETestCase, NotConnectedTestCase
from .util import LuaError, TimeoutLoop


class TestConnectUpdate(E2ETestCase):
    def get_items_handling(self) -> int:
        return self.server._connections[0].items_handling

    def get_tags(self) -> List[str]:
        return self.server._connections[0].tags

    def test_update_nothing(self) -> None:
        with self.assertRaises(LuaError):
            self.call("ConnectUpdate", None, None)

    def test_missing_arg1(self) -> None:
        with self.assertRaises(LuaError):
            self.call("ConnectUpdate")

    def test_update_items_handling(self) -> None:
        old_tags = self.get_tags()
        # FIXME: missing arg for tags will default to empty tags. Is this expected?
        self.call("ConnectUpdate", 1, None)
        self.assertNotEqual(self.get_items_handling(), 1)
        for _ in TimeoutLoop(lambda: self.get_items_handling() != 1):
            self.poll()
        self.assertEqual(self.get_items_handling(), 1)
        self.assertEqual(self.get_tags(), old_tags)

    def test_update_tags(self) -> None:
        new_tags = ["Updated"]
        old_items_handling = self.get_items_handling()
        self.call("ConnectUpdate", None, self.lua.table(*new_tags))
        self.assertNotEqual(self.get_tags(), new_tags)
        for _ in TimeoutLoop(lambda: self.get_tags() != new_tags):
            self.poll()
        self.assertEqual(self.get_tags(), new_tags)
        self.assertEqual(self.get_items_handling(), old_items_handling)

    def test_update_both(self) -> None:
        new_tags = ["Updated"]
        self.call("ConnectUpdate", 1, self.lua.table(*new_tags))
        self.assertNotEqual(self.get_items_handling(), 1)
        self.assertNotEqual(self.get_tags(), new_tags)
        for _ in TimeoutLoop(lambda: self.get_items_handling() != 1):
            self.poll()
        self.assertEqual(self.get_items_handling(), 1)
        self.assertEqual(self.get_tags(), new_tags)

    def test_bad_self(self) -> None:
        with self.assertRaises(LuaError):
            self.client["ConnectUpdate"]()

    def test_bad_items_handling(self) -> None:
        with self.assertRaises(LuaError):
            self.call("ConnectUpdate", "a")

    def test_bad_tags(self) -> None:
        with self.assertRaises(LuaError):
            self.call("ConnectUpdate", None, 1)


class TestConnectUpdateNotConnected(NotConnectedTestCase):
    def test_update(self) -> None:
        res = self.call("ConnectUpdate", 1, None)
        self.assertFalse(res)
