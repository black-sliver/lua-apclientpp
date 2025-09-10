from unittest import skipIf

from .bases import ClientTestCase, E2ETestCase
from .util import LuaError, LuaTable, TimeoutLoop


class Bases:
    class SlotRefusedTestCase(E2ETestCase):
        done: bool = False
        expected_reason: str

        def on_slot_refused(self, reasons: LuaTable) -> None:
            if not hasattr(self, "expected_reason"):
                return
            reasons_list = []
            for k, v in reasons.items():
                self.assertIsInstance(k, int)
                self.assertIsInstance(v, str)
                reasons_list.append(v)
            if self.expected_reason not in reasons_list:
                print(f"{self.expected_reason} not in {reasons_list}")
            self.assertIn(self.expected_reason, reasons_list)
            self.done = True
            self.slot_connected = True

        def setUp(self) -> None:
            if self.__class__ is Bases.SlotRefusedTestCase:
                self.skipTest("is a base")
            super().setUp()

        def test_connect(self) -> None:
            self.assertTrue(self.done)


    class BadSetUpTest(E2ETestCase):
        def setUp(self) -> None:
            with self.assertRaises(LuaError):
                super().setUp()

        def tearDown(self) -> None:
            try:
                super().tearDown()
            except AssertionError:
                pass

        def test_set_up(self) -> None:
            pass  # test runs in setUp


class TestInvalidSlot(Bases.SlotRefusedTestCase):
    slot = "Player999"
    expected_reason = "InvalidSlot"


class TestBadSlotRefused(E2ETestCase):
    slot = "Player999"

    def setUp(self) -> None:
        # the default on_slot_refused handler raises on refused
        with self.assertRaises(LuaError):
            super().setUp()

    def tearDown(self) -> None:
        # if we raise in setUp, then test case may already be torn down
        try:
            super().tearDown()
        except AssertionError:
            pass

    def test_error(self) -> None:
        pass  # assertion is in setUp


class TestBadSlotConnected(E2ETestCase):
    def on_slot_connected(self, slot_data: LuaTable) -> None:
        raise RuntimeError("OK")

    def on_slot_refused(self, reasons: LuaTable) -> None:
        pass

    def setUp(self) -> None:
        with self.assertRaises(LuaError):
            super().setUp()

    def tearDown(self) -> None:
        # if we raise in setUp, then test case may already be torn down
        try:
            super().tearDown()
        except AssertionError:
            pass

    def test_error(self) -> None:
        pass  # assertion is in setUp


class TestPrint(E2ETestCase):
    done = False

    def on_print(self, message: str) -> None:
        super().on_print(message)
        if "Hello, World!" in message:
            self.done = True

    def test_print(self) -> None:
        self.server.print_all("Hello, World!")
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()


class TestBadPrint(E2ETestCase):
    def on_print(self, message: str) -> None:
        raise RuntimeError("OK")

    def test_print(self) -> None:
        self.server.print_all("Hello, World!")
        with self.assertRaises(LuaError):
            for _ in TimeoutLoop(lambda: True):
                self.poll()


class TestBadPrintJSON(E2ETestCase):
    def on_print_json(self, data: LuaTable, cmd: LuaTable) -> None:
        raise RuntimeError("OK")

    def test_print_json(self) -> None:
        self.server.print_json(
            self.server._connections[0].connection,
            [],
            {},
        )
        with self.assertRaises(LuaError):
            for _ in TimeoutLoop(lambda: True):
                self.poll()


class TestBadBounced(E2ETestCase):
    # NOTE: good Bounced is tested in test_commands
    def on_bounced(self, command: LuaTable) -> None:
        raise RuntimeError("OK")

    def test_error(self) -> None:
        self.server.send_bounce([self.game], [], [], {})
        with self.assertRaises(LuaError):
            for _ in TimeoutLoop(lambda: True):
                self.poll()


class TestHandlerBadSelf(E2ETestCase):
    def connect(self) -> None:
        super().connect()
        with self.assertRaises(LuaError):
            self.client["set_socket_connected_handler"]()

    def test_bad_self(self) -> None:
        pass  # test runs in setUp


class TestHandlerNotCallable(Bases.BadSetUpTest):
    def connect(self) -> None:
        super().connect()
        self.call("set_socket_connected_handler", 1)


class TestHandlerMissing(Bases.BadSetUpTest):
    def connect(self) -> None:
        super().connect()
        self.call("set_socket_connected_handler")


class TestBadSocketConnected(Bases.BadSetUpTest):
    def on_socket_connected(self) -> None:
        raise RuntimeError("OK")


class TestBadRoomInfo(Bases.BadSetUpTest):
    def on_room_info(self) -> None:
        raise RuntimeError("OK")


class TestBadItemsReceived(E2ETestCase):
    # NOTE: good ItemsReceived is tested in test_commands
    def test_error(self) -> None:
        def on_items_received(items: LuaTable) -> None:
            raise RuntimeError("OK")

        self.call("set_items_received_handler", on_items_received)
        self.call("Sync")
        with self.assertRaises(LuaError):
            for _ in TimeoutLoop(lambda: True):
                self.poll()


class TestBadSocketError(ClientTestCase):
    def on_socket_error(self, reason: str) -> None:
        raise RuntimeError("OK")

    def test_error(self) -> None:
        unused_port = 38289
        self.uri = f"ws://localhost:{unused_port}"
        self.connect()
        with self.assertRaises(LuaError):
            for _ in TimeoutLoop(lambda: True):
                self.poll()


@skipIf(True, "Test doesn't work because of GIL")
class TestBadSocketDisconnected(E2ETestCase):
    def on_socket_disconnected(self) -> None:
        raise RuntimeError("OK")

    def test_error(self) -> None:
        self.server._connections[0].connection.close()
        with self.assertRaises(LuaError):
            for _ in TimeoutLoop(lambda: True):
                self.poll()


class TestLocationChecked(E2ETestCase):
    started = False
    done = False
    locations = [1, 2**32]

    def on_location_checked(self, locations: LuaTable) -> None:
        if self.started:
            self.done = True
        self.assertEqual(sum(1 for _ in locations.values()), len(self.locations))
        for k, v in locations.items():
            print(k, v)
            self.assertIsInstance(k, int)
            self.assertIsInstance(v, int)

    def test_location_checked(self) -> None:
        self.started = True
        conn = self.server._connections[0].connection
        self.server.send_room_update(conn, checked_locations=self.locations)
        for _ in TimeoutLoop(lambda: not self.done):
            self.poll()


class TestBadLocationChecked(E2ETestCase):
    def on_location_checked(self, locations: LuaTable) -> None:
        raise RuntimeError("OK")

    def test_error(self) -> None:
        conn = self.server._connections[0].connection
        self.server.send_room_update(conn, checked_locations=[1])
        with self.assertRaises(LuaError):
            for _ in TimeoutLoop(lambda: True):
                self.poll()


class TestBadLocationInfo(E2ETestCase):
    def on_location_info(self, items: LuaTable) -> None:
        raise RuntimeError("OK")

    def test_error(self) -> None:
        conn = self.server._connections[0].connection
        self.server.send_location_info(
            conn,
            [{
                "item": 1,
                "location": 1,
                "player": 1,
                "flags": 0,
            }]
        )
        with self.assertRaises(LuaError):
            for _ in TimeoutLoop(lambda: True):
                self.poll()


class TestBadOnRetrieved(E2ETestCase):
    def on_retrieved(self, data: LuaTable, keys: LuaTable, command: LuaTable) -> None:
        raise RuntimeError("OK")

    def test_error(self) -> None:
        conn = self.server._connections[0].connection
        self.server.send_retrieved(
            conn,
            {
                "keys": {},
            }
        )
        with self.assertRaises(LuaError):
            for _ in TimeoutLoop(lambda: True):
                self.poll()


class TestBadOnSetReply(E2ETestCase):
    def on_set_reply(self, command: LuaTable) -> None:
        raise RuntimeError("OK")

    def test_error(self) -> None:
        conn = self.server._connections[0].connection
        self.server.send_set_reply(
            conn,
            {
                "key": "a",
                "default": "b",
                "value": "c",
                "original_value": "d",
                "operations": [],
                "slot": 1,
            }
        )
        with self.assertRaises(LuaError):
            for _ in TimeoutLoop(lambda: True):
                self.poll()


class TestOnDataPackageChanged(E2ETestCase):
    def on_data_package_changed(self, data_package: LuaTable) -> None:
        for k in data_package.keys():
            self.assertIsInstance(k, str)

    def test_ok(self) -> None:
        # FIXME: should empty datapackage trigger on_data_package_changed?
        pass


class TestBadOnDataPackageChanged(Bases.BadSetUpTest):
    def on_data_package_changed(self, data_package: LuaTable) -> None:
        raise RuntimeError("OK")
