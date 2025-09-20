from .bases import E2ETestCase, ClientTestCase
from .util import LuaError


class TestProperties(E2ETestCase):
    def test_player_alias(self) -> None:
        self.assertEqual("Server", self.call("get_player_alias", 0))
        self.assertEqual(self.slot, self.call("get_player_alias", 1))
        self.assertEqual("Unknown", self.call("get_player_alias", 999))

    def test_player_game(self) -> None:
        self.assertEqual("Archipelago", self.call("get_player_game", 0))
        self.assertEqual(self.game, self.call("get_player_game", 1))
        self.assertEqual("", self.call("get_player_game", 999), "Unknown game should be empty")

    def test_game(self) -> None:
        self.assertEqual(self.game, self.call("get_game"))

    def test_location_name_unknown(self) -> None:
        self.assertEqual("Unknown", self.call("get_location_name", 999999, self.game))
        self.assertEqual("Unknown", self.call("get_location_name", 999999, None))
        self.assertEqual("Unknown", self.call("get_location_name", 999999.0, None))
        # TODO: test non-unknown locations

    def test_location_name_bad_args(self) -> None:
        with self.assertRaises(LuaError):
            self.call("get_location_name")
        with self.assertRaises(LuaError):
            self.call("get_location_name", 1)
        with self.assertRaises(LuaError):
            self.call("get_location_name", "a", None)

    def test_location_id_unknown(self) -> None:
        invalid_id = -1 * 2**63
        self.assertEqual(invalid_id, self.call("get_location_id", "Blerf"))
        # TODO: test non-unknown locations

    def test_location_id_bad_arg(self) -> None:
        with self.assertRaises(LuaError):
            self.call("get_location_id")
        with self.assertRaises(LuaError):
            self.call("get_location_id", self.lua.table())

    def test_item_name_unknown(self) -> None:
        self.assertEqual("Unknown", self.call("get_item_name", 999999, self.game))
        self.assertEqual("Unknown", self.call("get_item_name", 999999, None))
        self.assertEqual("Unknown", self.call("get_item_name", 999999.0, None))
        # TODO: test non-unknown items

    def test_item_name_bad_args(self) -> None:
        with self.assertRaises(LuaError):
            self.call("get_item_name")
        with self.assertRaises(LuaError):
            self.call("get_item_name", 1)
        with self.assertRaises(LuaError):
            self.call("get_item_name", "a", None)

    def test_item_id_unknown(self) -> None:
        invalid_id = -1 * 2**63
        self.assertEqual(invalid_id, self.call("get_item_id", "Blerf"))
        # TODO: test non-unknown items

    def test_item_id_bad_arg(self) -> None:
        with self.assertRaises(LuaError):
            self.call("get_item_id")
        with self.assertRaises(LuaError):
            self.call("get_item_id", self.lua.table())

    def test_state(self) -> None:
        self.assertEqual(self.call("get_state"), self.client["State"]["SLOT_CONNECTED"])

    def test_seed(self) -> None:
        self.assertEqual("seed", self.call("get_seed"))

    def test_slot(self) -> None:
        self.assertEqual(self.slot, self.call("get_slot"))

    def test_player_number(self) -> None:
        self.assertEqual(1, self.call("get_player_number"))

    def test_team_number(self) -> None:
        self.assertEqual(0, self.call("get_team_number"))

    def test_hint_points(self) -> None:
        # TODO: also test with room update
        self.assertEqual(7, self.call("get_hint_points"))

    def test_hint_cost(self) -> None:
        # TODO: also test for different location count
        # TODO: also test with room update
        # TODO: also check free hints (hint_cost = 0)
        location_count = len(self.client["checked_locations"]) + len(self.client["missing_locations"])
        self.assertEqual(50, self.call("get_hint_cost_percent"))
        self.assertEqual(max(1, location_count//2), self.call("get_hint_cost_points"))

    def test_data_package_valid(self) -> None:
        self.assertIsInstance(self.call("is_data_package_valid"), bool)
        # self.assertTrue(self.call("is_data_package_valid"))

    def test_server_time(self) -> None:
        from time import sleep, time
        t1 = self.call("get_server_time")
        sleep(0.001)
        t2 = self.call("get_server_time")
        self.assertGreaterEqual(t2 - t1, 0.0009)
        self.assertGreater(t1, 1757455200)
        self.assertLess(t1, time() + 24 * 60 * 60)

    def test_players(self) -> None:
        for k, v in self.call("get_players").items():
            self.assertIsInstance(k, int)
            self.assertIsInstance(v["team"], int)
            self.assertIsInstance(v["slot"], int)
            self.assertIsInstance(v["alias"], str)
            self.assertIsInstance(v["name"], str)

    def test_checked_locations(self) -> None:
        # FIXME: this is currently empty
        for k, v in self.client["checked_locations"].items():
            self.assertIsInstance(k, int)
            self.assertIsInstance(v, int)

    def test_missing_locations(self) -> None:
        # FIXME: this is currently empty
        for k, v in self.client["missing_locations"].items():
            self.assertIsInstance(k, int)
            self.assertIsInstance(v, int)

    def test_get_unknown(self) -> None:
        self.assertEqual(None, self.client["does_not_exist"])

    def test_set_unknown(self) -> None:
        with self.assertRaises(Exception):
            self.client["does_not_exist"] = 1


class TestPropertiesNotConnected(ClientTestCase):
    def setUp(self) -> None:
        super().setUp()
        self.connect()

    def test_players(self) -> None:
        self.assertEqual(0, len(list(self.call("get_players").values())))
