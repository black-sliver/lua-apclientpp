from typing import Any, Dict, Optional, cast

from .bases import E2ETestCase, NotConnectedTestCase
from .util import LuaError, LuaTable, TimeoutLoop


class TestSay(E2ETestCase):
    done = False

    def on_print_json(self, data: LuaTable, cmd: LuaTable) -> None:
        plain = self.call("render_json", data, self.client["RenderFormat"]["TEXT"])
        dflt = self.call("render_json", data)
        self.assertEqual(plain, dflt)
        if "Hello, World!" in plain:
            self.done = True

    def test_say(self) -> None:
        self.call("Say", "Hello, World!")
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()

    def test_bad_self(self) -> None:
        with self.assertRaises(LuaError):
            self.client["Say"](self.lua.table())


class TestSayNotConnected(NotConnectedTestCase):
    def test_call(self) -> None:
        res = self.call("Say", "Hello, World!")
        self.assertFalse(res)


class TestBounce(E2ETestCase):
    done = False
    nonce = "ok"

    def on_bounced(self, command: LuaTable) -> None:
        if command["data"]["nonce"] != self.nonce:
            raise RuntimeError("Bad bounce")
        self.done = True

    def test_game(self) -> None:
        ok = self.call(
            "Bounce",
            self.lua.table(nonce = self.nonce),
            self.lua.table(self.game),
            None,
            None,
        )
        self.assertTrue(ok)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()

    def test_slot(self) -> None:
        ok = self.call(
            "Bounce",
            self.lua.table(nonce = self.nonce),
            None,
            self.lua.table(self.call("get_player_number")),
            None,
        )
        self.assertTrue(ok)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()

    def test_tag(self) -> None:
        ok = self.call(
            "Bounce",
            self.lua.table(nonce = self.nonce),
            None,
            None,
            self.lua.table("Test"),
        )
        self.assertTrue(ok)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()

    def test_bad_game(self) -> None:
        with self.assertRaises(LuaError):
            self.call(
                "Bounce",
                self.lua.table(nonce = self.nonce),
                1,
                None,
                None,
            )

    def test_bad_slot(self) -> None:
        with self.assertRaises(LuaError):
            self.call(
                "Bounce",
                self.lua.table(nonce = self.nonce),
                None,
                1,
                None,
            )

    def test_bad_tag(self) -> None:
        with self.assertRaises(LuaError):
            self.call(
                "Bounce",
                self.lua.table(nonce = self.nonce),
                None,
                None,
                1,
            )

    def test_bad_self(self) -> None:
        with self.assertRaises(LuaError):
            self.client["Bounce"](self.lua.table())


class TestBounceNotConnected(NotConnectedTestCase):
    def test_call(self) -> None:
        res = self.call("Bounce")
        self.assertFalse(res)


class TestStatusUpdate(E2ETestCase):
    def get_status(self) -> int:
        return self.server._connections[0].status

    def set_status(self, status: int) -> None:
        old_status = self.get_status()
        ok = self.call("StatusUpdate", status)
        self.assertTrue(ok)
        for _ in TimeoutLoop(lambda: self.get_status() == old_status):
            self.poll()
        self.assertEqual(self.get_status(), status)

    def test_ready(self) -> None:
        self.set_status(self.client["ClientStatus"]["READY"])

    def test_playing(self) -> None:
        self.set_status(self.client["ClientStatus"]["PLAYING"])

    def test_goal(self) -> None:
        self.set_status(self.client["ClientStatus"]["GOAL"])

    def test_unset(self) -> None:
        self.test_playing()
        self.test_ready()

    def test_bad_self(self) -> None:
        with self.assertRaises(LuaError):
            self.client["StatusUpdate"](self.lua.table())


class TestStatusUpdateNotConnected(NotConnectedTestCase):
    def test_call(self) -> None:
        res = self.call("StatusUpdate", self.client["ClientStatus"]["READY"])
        self.assertFalse(res)


class TestSync(E2ETestCase):
    done = False
    items: LuaTable

    def on_items_received(self, items: LuaTable) -> None:
        # NOTE: this expects a start inventory
        self.assertTrue(sum(1 for _ in items.keys()))
        for k, v in items.items():
            assert isinstance(k, int)
            self.assertTrue(sum(1 for _ in items[k].keys()))
        if not hasattr(self, "items"):
            self.items = items
        for i, old_item in self.items.items():
            for item_key, old_item_value in old_item.items():
                self.assertIsInstance(item_key, str)
                self.assertEqual(old_item_value, items[i][item_key])
        self.done = True

    def test_sync(self) -> None:
        self.done = False
        res = self.call("Sync")
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()

    def test_bad_self(self) -> None:
        with self.assertRaises(LuaError):
            self.client["Sync"](self.lua.table())


class TestSyncNotConnected(NotConnectedTestCase):
    def test_call(self) -> None:
        res = self.call("Sync")
        self.assertFalse(res)


class TestLocationChecks(E2ETestCase):
    started = False
    done = False
    location_id = 2**32

    def on_items_received(self, items: LuaTable) -> None:
        # ignore start inventory
        if self.started:
            self.assertEqual(items[1]["location"], self.location_id)
            self.done = True

    def on_location_checked(self, locations: LuaTable) -> None:
        # own location should be filtered out
        # TODO: test receiveOwnLocations once we update apclientpp
        # TODO: also test duplicates
        if self.started:
            self.fail("Unexpected location checked")

    def test_location_checks(self) -> None:
        self.started = True
        res = self.call("LocationChecks", self.lua.table(self.location_id))
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()

    def test_bad_self(self) -> None:
        with self.assertRaises(LuaError):
            self.client["LocationChecks"](self.lua.table())

    def test_bad_locations(self) -> None:
        res = self.call("LocationChecks", 1)
        self.assertFalse(res)


class TestLocationChecksNotConnected(NotConnectedTestCase):
    def test_call(self) -> None:
        # This will queue up the checks
        res = self.call("LocationChecks", self.lua.table(1))
        self.assertTrue(res)
        # TODO: test if polling afterwards actually does the checks


class TestLocationScout(E2ETestCase):
    got_hint = False
    done = False
    location_id = 2**32

    def on_location_info(self, items: LuaTable) -> None:
        # test server will send item id = location id
        slot = self.call("get_player_number")
        self.assertEqual(items[1]["location"], self.location_id)
        self.assertEqual(items[1]["item"], self.location_id)
        self.assertEqual(items[1]["player"], slot)
        self.done = True

    def on_print_json(self, data: LuaTable, cmd: LuaTable) -> None:
        self.got_hint = True

    def test_location_scouts(self) -> None:
        res = self.call("LocationScouts", self.lua.table(self.location_id))
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()

    def test_location_scouts_no_hint(self) -> None:
        res = self.call("LocationScouts", self.lua.table(self.location_id), False)
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()

    def test_location_scouts_as_hint(self) -> None:
        res = self.call("LocationScouts", self.lua.table(self.location_id), True)
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()
        self.assertTrue(self.got_hint)

    def test_location_scouts_new_hint(self) -> None:
        res = self.call("LocationScouts", self.lua.table(self.location_id), 2)
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()
        self.assertTrue(self.got_hint)

    def test_bad_self(self) -> None:
        with self.assertRaises(LuaError):
            self.client["LocationScouts"](self.lua.table())

    def test_bad_locations(self) -> None:
        res = self.call("LocationScouts", 1)
        self.assertFalse(res)


class TestLocationScoutNotConnected(NotConnectedTestCase):
    def test_call(self) -> None:
        # This will queue up the scout
        res = self.call("LocationScouts", self.lua.table(1))
        self.assertTrue(res)
        # TODO: test if polling afterwards actually does the scout


class TestGet(E2ETestCase):
    done = False
    data: Dict[str, Any] = {
        "a": None,
        "b": {"c": 1},
        "empty_array": [],
    }
    extra: Dict[str, Any] = {
        "x": "y",
    }
    received_extra: Optional[Dict[str, Any]] = None

    def setUp(self) -> None:
        super().setUp()
        self.server.data_storage = self.data

    def on_retrieved(self, data: LuaTable, keys: LuaTable, command: LuaTable) -> None:
        for key in keys.values():
            self.assertIn(key, self.data)
            if isinstance(self.data[key], dict):
                self.assertTableEqualDict(data[key], self.data[key])
            elif isinstance(self.data[key], list):
                self.assertTableEqualList(data[key], self.data[key])
            else:
                self.assertEqual(data[key], self.data[key])
        self.received_extra = cast(Dict[str, Any], {
            k: v
            for k, v in command.items()
            if k not in ("cmd", "keys")
        })
        self.done = True

    def test_get(self) -> None:
        res = self.call("Get", self.lua.table(*self.data.keys()))
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()
        self.assertFalse(self.received_extra)

    def test_get_extra(self) -> None:
        res = self.call("Get", self.lua.table(*self.data.keys()), self.lua.table(**self.extra))
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()
        self.assertEqual(self.received_extra, self.extra)

    def test_bad_self(self) -> None:
        with self.assertRaises(LuaError):
            self.client["Get"](self.lua.table())

    def test_bad_keys(self) -> None:
        with self.assertRaises(LuaError):
            self.call("Get", 1)

    def test_bad_extra(self) -> None:
        with self.assertRaises(LuaError):
            self.call("Get", self.lua.table(*self.data.keys()), 1)


class TestGetNotConnected(NotConnectedTestCase):
    def test_call(self) -> None:
        res = self.call("Get", self.lua.table("a"))
        self.assertFalse(res)


class TestSet(E2ETestCase):
    # also tests SetNotify and EmptyArray
    done = False
    extra: Dict[str, Any] = {
        "x": "y",
    }
    received_original: Optional[Any] = None
    received_value: Optional[Any] = None
    received_extra: Optional[Dict[str, Any]] = None

    def on_set_reply(self, command: LuaTable) -> None:
        self.received_value = command["value"]
        self.received_original = command["original_value"]
        self.received_extra = cast(Dict[str, Any], {
            k: v
            for k, v in command.items()
            if k not in {"cmd", "key", "default", "value", "want_reply", "operations", "original_value", "slot"}
        })
        self.done = True

    def data_as_table(self, data: Any) -> Any:
        if isinstance(data, dict):
            res = self.lua.table()
            for k, v in data.items():
                res[k] = self.data_as_table(v)
            return res
        elif isinstance(data, list):
            if len(data) == 0:
                return self.apclient["EMPTY_ARRAY"]
            res = self.lua.table()
            for i, v in enumerate(data, 1):
                res[i] = self.data_as_table(v)
            return res
        else:
            return data

    def test_no_notify(self) -> None:
        value = 1
        res = self.call(
            "Set",
            "a",
            value,
            False,
            self.apclient["EMPTY_ARRAY"],
        )
        self.assertTrue(res)
        with self.assertRaises(TimeoutError):
            for _ in TimeoutLoop(lambda: not self.done):
                self.poll()
        self.assertIsNone(self.received_extra)
        self.assertIsNone(self.received_original)
        self.assertIsNone(self.received_value)

    def test_with_notify(self) -> None:
        value = 1
        res = self.call("SetNotify", self.lua.table("a"))
        self.assertTrue(res)
        res = self.call(
            "Set",
            "a",
            value,
            False,
            self.apclient["EMPTY_ARRAY"],
        )
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()
        self.assertFalse(self.received_extra)
        self.assertIsNotNone(self.received_original)
        self.assertEqual(self.received_value, value)

    def test_with_want_reply(self) -> None:
        value = 1
        res = self.call(
            "Set",
            "a",
            value,
            True,
            self.apclient["EMPTY_ARRAY"],
        )
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()
        self.assertFalse(self.received_extra)
        self.assertIsNotNone(self.received_original)
        self.assertEqual(self.received_value, value)

    def test_with_operation_object(self) -> None:
        not_value = 1
        value = 2
        operations = self.lua.table(self.lua.table(operation="replace", value=value))
        res = self.call(
            "Set",
            "a",
            not_value,
            True,
            operations,
        )
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()
        self.assertFalse(self.received_extra)
        self.assertNotEqual(self.received_original, value)
        self.assertEqual(self.received_value, value)

    def test_with_operation_array(self) -> None:
        not_value = 1
        value = 2
        operations = self.lua.table(self.lua.table("replace", value))
        res = self.call(
            "Set",
            "b",
            not_value,
            True,
            operations,
        )
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()
        self.assertFalse(self.received_extra)
        self.assertNotEqual(self.received_original, value)
        self.assertEqual(self.received_value, value)

    def test_extra(self) -> None:
        value = 1
        res = self.call(
            "Set",
            "a",
            value,
            True,
            self.apclient["EMPTY_ARRAY"],
            self.data_as_table(self.extra),
        )
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()
        self.assertEqual(self.received_extra, self.extra)

    def test_empty_array(self) -> None:
        key = "a"
        res = self.call(
            "Set",
            key,
            self.apclient["EMPTY_ARRAY"],
            True,
            self.apclient["EMPTY_ARRAY"],
        )
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()
        self.assertTableEqualList(cast(LuaTable, self.received_value), [])
        self.assertEqual(self.server.data_storage[key], [])

    def test_empty_dict(self) -> None:
        key = "a"
        res = self.call(
            "Set",
            key,
            self.lua.table(),
            True,
            self.apclient["EMPTY_ARRAY"],
        )
        self.assertTrue(res)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()
        self.assertTableEqualDict(cast(LuaTable, self.received_value), {})
        self.assertEqual(self.server.data_storage[key], {})

    def test_bad_self(self) -> None:
        with self.assertRaises(LuaError):
            self.client["Set"](self.lua.table())

    def test_missing_value(self) -> None:
        with self.assertRaises(LuaError):
            self.call(
                "Set",
                "a",
            )

    def test_bad_key(self) -> None:
        with self.assertRaises(LuaError):
            self.call(
                "Set",
                self.lua.table(),
                1,
                False
            )

    def test_bad_operations(self) -> None:
        with self.assertRaises(LuaError):
            self.call(
                "Set",
                "a",
                1,
                False,
                1,
            )

    def test_bad_extra(self) -> None:
        with self.assertRaises(LuaError):
            self.call(
                "Set",
                "a",
                1,
                False,
                self.lua.table(),
                1,
            )


class TestSetNotConnected(NotConnectedTestCase):
    def test_call(self) -> None:
        res = self.call(
            "Set",
            "a",
            1,
            False,
            self.client["EMPTY_ARRAY"],
        )
        self.assertFalse(res)


class TestSetNotify(E2ETestCase):
    # NOTE: happy code path tested in TestSet
    def test_bad_self(self) -> None:
        with self.assertRaises(LuaError):
            self.client["SetNotify"](self.lua.table())

    def test_missing_keys(self) -> None:
        with self.assertRaises(LuaError):
            self.call(
                "SetNotify",
            )

    def test_bad_keys(self) -> None:
        with self.assertRaises(LuaError):
            self.call(
                "SetNotify",
                1,
            )


class TestSetNotifyNotConnected(NotConnectedTestCase):
    def test_call(self) -> None:
        res = self.call(
            "SetNotify",
            self.lua.table("a"),
        )
        self.assertFalse(res)


if __name__ == "__main__":
    # for single test, run this as `python -m test.test_commands`
    test = TestSay()
    test.setUp()
    test.test_say()
    test.tearDown()
