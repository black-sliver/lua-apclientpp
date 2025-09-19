from typing import Any, Dict, List, cast
from unittest import TestCase

from .util import LuaAPClient, LuaRuntime, LuaTable, TimeoutLoop
from .server import APServer


class LuaTestCase(TestCase):
    lua: LuaRuntime
    apclient: LuaAPClient

    def assertTableEqualDict(self, first: LuaTable, second: Dict[Any, Any]) -> None:
        self.assertEqual(sorted(first.keys()), sorted(second.keys()))
        for k, v in first.items():
            self.assertEqual(v, second[k])

    def assertTableEqualList(self, first: LuaTable, second: List[Any]) -> None:
        self.assertEqual(len(list(first.keys())), len(second))
        for i, v in enumerate(second, 1):
            self.assertEqual(first[i], v)

    def setUp(self) -> None:
        super().setUp()
        self.lua = LuaRuntime()  # type: ignore[call-arg, unused-ignore]
        lib = cast(Any, self.lua.require("lua-apclientpp"))
        self.assertTrue(lib, "Could not load lua-apclientpp")
        if isinstance(lib, tuple):
            self.apclient = lib[0]  # lua5.4
        else:
            self.apclient = lib


class APITestCase(LuaTestCase):
    api: LuaTable

    def setUp(self) -> None:
        super().setUp()
        with open("api/library/lua-apclientpp.lua") as f:
            script = f.read().replace("APClient = APClient.__call", "")
            self.api = cast(LuaTable, self.lua.execute(script))


class ClientTestCase(LuaTestCase):
    uri: str = "ws://localhost"
    uuid: str = ""
    game: str = "Game"

    client: LuaTable

    def call(self, name: str, *args: Any) -> Any:
        return self.client[name](self.client, *args)

    def poll(self) -> None:
        self.call("poll")

    def on_socket_connected(self) -> None:
        pass

    def on_socket_error(self, reason: str) -> None:
        raise RuntimeError("on_socket_error")

    def on_socket_disconnected(self) -> None:
        pass

    def on_room_info(self) -> None:
        pass

    def on_slot_connected(self, slot_data: LuaTable) -> None:
        pass

    def on_slot_refused(self, reasons: LuaTable) -> None:
        raise RuntimeError("on_slot_refused")

    def on_items_received(self, items: LuaTable) -> None:
        pass

    def on_location_info(self, items: LuaTable) -> None:
        pass

    def on_location_checked(self, locations: LuaTable) -> None:
        pass

    def on_data_package_changed(self, data_package: LuaTable) -> None:
        pass

    def on_print(self, message: str) -> None:
        self.assertIsInstance(message, str)

    def on_print_json(self, data: LuaTable, cmd: LuaTable) -> None:
        pass

    def on_bounced(self, command: LuaTable) -> None:
        pass

    def on_retrieved(self, data: LuaTable, keys: LuaTable, command: LuaTable) -> None:
        pass

    def on_set_reply(self, command: LuaTable) -> None:
        pass

    def connect(self) -> None:
        self.client = self.apclient(self.uuid, self.game, self.uri)
        self.call("set_socket_connected_handler", self.on_socket_connected)
        self.call("set_socket_error_handler", self.on_socket_error)
        self.call("set_socket_disconnected_handler", self.on_socket_disconnected)
        self.call("set_room_info_handler", self.on_room_info)
        self.call("set_slot_connected_handler", self.on_slot_connected)
        self.call("set_slot_refused_handler", self.on_slot_refused)
        self.call("set_items_received_handler", self.on_items_received)
        self.call("set_location_info_handler", self.on_location_info)
        self.call("set_location_checked_handler", self.on_location_checked)
        self.call("set_data_package_changed_handler", self.on_data_package_changed)
        self.call("set_print_handler", self.on_print)
        self.call("set_print_json_handler", self.on_print_json)
        self.call("set_bounced_handler", self.on_bounced)
        self.call("set_retrieved_handler", self.on_retrieved)
        self.call("set_set_reply_handler", self.on_set_reply)

    def setUp(self) -> None:
        super().setUp()

    def tearDown(self) -> None:
        if hasattr(self, "client"):
            del self.client
            self.lua.gccollect()  # doesn't release the GIL? FIXME: explicit close() in the client?


class NotConnectedTestCase(ClientTestCase):
    def poll(self) -> None:
        raise RuntimeError("Do not poll to stay disconnected")

    def setUp(self) -> None:
        super().setUp()
        self.connect()


class E2ETestCase(ClientTestCase):
    slot: str = "Player1"
    items_handling: int = 0

    server: APServer

    socket_connected = False
    got_room_info = False
    slot_connected = False

    def poll(self) -> None:
        self.server.check()
        super().poll()

    def wait_room_info(self) -> None:
        for _ in TimeoutLoop(lambda: not self.got_room_info):
            self.poll()

    def wait_slot_connected(self) -> None:
        for _ in TimeoutLoop(lambda: not self.slot_connected):
            self.poll()

    def on_socket_connected(self) -> None:
        self.socket_connected = True

    def on_room_info(self) -> None:
        print("on_room_info")
        self.got_room_info = True

    def on_slot_connected(self, slot_data: LuaTable) -> None:
        print("on_slot_connected")
        self.slot_connected = True

    def _connect_slot(self) -> None:
        self.call(
            "ConnectSlot",
            self.slot,
            "",
            self.items_handling,
            self.lua.table("Test"),
            self.lua.table(0, 6, 3)
        )

    def setUp(self) -> None:
        super().setUp()
        self.server = APServer()
        try:
            self.server.start()
            for _ in TimeoutLoop(lambda: self.server.port == 0):
                pass

            self.uri = f"ws://localhost:{self.server.port}"
            self.connect()

            self.wait_room_info()
            self.assertTrue(self.socket_connected)
            self._connect_slot()
            self.wait_slot_connected()
        except:
            self.tearDown()
            raise

    def tearDown(self) -> None:
        super().tearDown()
        self.server.stop()
