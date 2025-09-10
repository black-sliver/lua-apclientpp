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
        # bad data just returns nil
        self.assertEqual(None, self.call("render_json", 1))

    # TODO: test plain, ansi, invalid
