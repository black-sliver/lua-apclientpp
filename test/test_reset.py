from test.bases import E2ETestCase
from test.util import LuaError, TimeoutLoop


class ResetTest(E2ETestCase):
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
