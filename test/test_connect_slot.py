"""Test all variations of ConnectSlot command"""
from .bases import E2ETestCase, ClientTestCase
from .util import LuaError


class Bases:
    class FailedConnect(E2ETestCase):
        def setUp(self) -> None:
            with self.assertRaises(LuaError):
                super().setUp()

        def tearDown(self) -> None:
            try:
                super().tearDown()
            except AssertionError:
                pass

        def test_connect(self) -> None:
            pass  # testing happens in setUp


class TestConnectNoTags(E2ETestCase):
    def _connect_slot(self) -> None:
        res = self.call(
            "ConnectSlot",
            self.slot,
            "",
            self.items_handling,
        )
        self.assertTrue(res)

    def test_connect(self) -> None:
        v = self.server._connections[0].version
        self.assertTrue(v["major"] > 0 or v["minor"] > 0)


class TestConnectNoVersion(E2ETestCase):
    def _connect_slot(self) -> None:
        res = self.call(
            "ConnectSlot",
            self.slot,
            "",
            self.items_handling,
            self.lua.table("Test"),
        )
        self.assertTrue(res)

    def test_connect(self) -> None:
        v = self.server._connections[0].version
        self.assertTrue(v["major"] > 0 or v["minor"] > 0)


class TestConnectVersionArray(E2ETestCase):
    def _connect_slot(self) -> None:
        res = self.call(
            "ConnectSlot",
            self.slot,
            "",
            self.items_handling,
            self.lua.table("Test"),
            self.lua.table(0, 6, 3),
        )
        self.assertTrue(res)

    def test_connect(self) -> None:
        v = self.server._connections[0].version
        self.assertEqual(v["major"], 0)
        self.assertEqual(v["minor"], 6)
        self.assertEqual(v["build"], 3)


class TestConnectVersionObject(E2ETestCase):
    def _connect_slot(self) -> None:
        res = self.call(
            "ConnectSlot",
            self.slot,
            "",
            self.items_handling,
            self.lua.table("Test"),
            self.lua.table(major=0, minor=6, build=3),
        )
        self.assertTrue(res)

    def test_connect(self) -> None:
        v = self.server._connections[0].version
        self.assertEqual(v["major"], 0)
        self.assertEqual(v["minor"], 6)
        self.assertEqual(v["build"], 3)


class TestConnectInvalidVersion(Bases.FailedConnect):
    def _connect_slot(self) -> None:
        self.call(
            "ConnectSlot",
            self.slot,
            "",
            self.items_handling,
            self.lua.table("Test"),
            self.lua.table(major="string"),
        )
        self.fail("call did not error")


class TestConnectInvalidTags(Bases.FailedConnect):
    def _connect_slot(self) -> None:
        self.call(
            "ConnectSlot",
            self.slot,
            "",
            self.items_handling,
            self.lua.table(key="value"),
        )
        self.fail("call did not error")


class TestConnectInvalidSlot(Bases.FailedConnect):
    def _connect_slot(self) -> None:
        self.call(
            "ConnectSlot",
            self.lua.table(bad="slot"),
            "",
            self.items_handling,
        )
        self.fail("call did not error")


class TestConnectInvalidPassword(Bases.FailedConnect):
    def _connect_slot(self) -> None:
        self.call(
            "ConnectSlot",
            self.slot,
            self.lua.table(bad="password"),
            self.items_handling,
        )
        self.fail("call did not error")


class TestConnectInvalidItemsHandling(Bases.FailedConnect):
    def _connect_slot(self) -> None:
        self.call(
            "ConnectSlot",
            self.slot,
            "",
            "bad items handling",
        )
        self.fail("call did not error")


class TestConnectInvalidSelf(Bases.FailedConnect):
    def _connect_slot(self) -> None:
        self.client["ConnectSlot"](
            self.lua.table(),
            self.slot,
            "",
            self.items_handling,
        )
        self.fail("call did not error")


class TestConnectUnconnected(ClientTestCase):
    def setUp(self) -> None:
        super().setUp()
        self.connect()

    def test_with_version(self) -> None:
        res = self.call(
            "ConnectSlot",
            "Player1",
            "",
            0,
            self.lua.table(),
            self.lua.table(0, 6, 3),
        )
        self.assertFalse(res)

    def test_without_version(self) -> None:
        res = self.call(
            "ConnectSlot",
            "Player1",
            "",
            0,
        )
        self.assertFalse(res)
