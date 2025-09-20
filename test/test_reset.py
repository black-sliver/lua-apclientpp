from test.bases import E2ETestCase, ClientTestCase
from test.util import LuaError, TimeoutLoop


class TestReset(E2ETestCase):
    done = False

    def on_room_info(self) -> None:
        super().on_room_info()
        self.done = True

    def test_reset(self) -> None:
        self.done = False
        self.call("reset")
        for _ in TimeoutLoop(lambda: not self.done, timeout=3.5):
            self.poll()

    def test_bad_call(self) -> None:
        with self.assertRaises(LuaError):
            self.client["reset"](self.lua.table())


class TestResetNotConnected(ClientTestCase):
    def setUp(self) -> None:
        super().setUp()
        self.connect()
        # don't poll, so never actually connect

    def test_reset(self) -> None:
        res = self.call("reset")
        self.assertFalse(res)  # nil
