import copy
import json
import time
from dataclasses import dataclass
from ssl import SSLContext
from threading import Thread
from typing import Any, Dict, List, Optional, Set

from websockets.sync.server import Server, ServerConnection


def is_version(o: Any) -> bool:
    return (
        isinstance(o, dict) and
        o.get("class", None) == "Version" and
        isinstance(o.get("major", None), int) and
        isinstance(o.get("minor", None), int)  and
        isinstance(o.get("build", None), int)
    )


class WSServer(Thread):
    port: int = 0
    _server: Optional[Server] = None
    _ssl: Optional[SSLContext] = None

    def __init__(self, ssl: Optional[SSLContext] = None):
        super().__init__()
        self._ssl = ssl

    def run(self) -> None:
        from websockets.sync.server import serve
        with serve(self.handler, host="0.0.0.0", port=self.port, ssl=self._ssl) as self._server:
            self.port = self._server.socket.getsockname()[1]
            # print(f"Listening on {self.port}")
            self._server.serve_forever()
        self._server = None

    def handler(self, conn: ServerConnection) -> None:
        pass

    def stop(self) -> None:
        assert self._server is not None, "Already stopped"
        if self._server is None:
            return
        self._server.shutdown()


@dataclass
class Connection:
    connection: ServerConnection
    slot: int
    tags: List[str]
    version: Dict[str, Any]
    items_handling: int
    status: int


class APServer(WSServer):
    _connections: List[Connection]
    _exception: Optional[Exception] = None
    password: Optional[str] = None
    player_names = ["Player1"]
    player_games = ["Game"]
    player_start_items: List[List[Dict[str, Any]]] = [
        [
            {"item": 1, "location": 1, "player": 1, "flags": 1},
        ],
    ]
    player_items: List[List[Dict[str, Any]]]
    data_storage: Dict[str, Any]
    set_notify: Set[str]  # TODO: this should be per connection

    def __init__(self) -> None:
        super().__init__()
        self._connections = []
        self.player_items = self.player_start_items
        self.data_storage = {}
        self.set_notify = set()

    def check(self) -> None:
        if self._exception:
            raise self._exception

    def stop(self) -> None:
        for connection_data in self._connections:
            connection_data.connection.close()
        self._connections.clear()
        super().stop()

    def handler(self, conn: ServerConnection) -> None:
        connection_data = Connection(conn, -1, [], {}, 0, 0)
        self._connections.append(connection_data)
        try:
            name = "Player1"
            auth = False
            items_handling: int  # = 0
            tags: List[str]  # = []
            self.send_room_info(conn)
            for data in conn:
                print(data)
                cmds = json.loads(data)
                for args in cmds:
                    cmd = args["cmd"]
                    if cmd == "Connect":
                        connect_errors: List[str] = []
                        if args["name"] not in self.player_names:
                            connect_errors.append("InvalidSlot")
                        assert args["password"] is None or isinstance(args["password"], str), "Missing password field"
                        if self.password and args["password"] != self.password:
                            connect_errors.append("InvalidPassword")
                        if not args.get("game", None):
                            connect_errors.append("InvalidGame")
                        assert isinstance(args["uuid"], str), "UUID must be a string"
                        assert is_version(args["version"]), "not a valid version"
                        items_handling = args.get("items_handling", 0)
                        assert isinstance(items_handling, int), "items_handling must be an integer if provided"
                        tags = args.get("tags", [])
                        assert isinstance(tags, list), "tags must be a list if provided"
                        for tag in tags:
                            assert isinstance(tag, str), "all tags must be a string"
                        if connect_errors:
                            self.send_connection_refused(conn, connect_errors)
                        else:
                            connection_data.slot = 1
                            connection_data.tags = tags
                            connection_data.version = args["version"]
                            connection_data.items_handling = items_handling
                            auth = True
                            self.send_connected(conn)
                            self.send_items(conn, 0, self.player_items[connection_data.slot - 1])
                    elif cmd == "GetDataPackage":
                        # FIXME: enable this check once apclientpp is fixed and send requested games
                        #for game in args["games"]:
                        #    if not isinstance(game, str):
                        #        raise ValueError("game must be a string")
                        self.send_datapackage(conn, [])
                    elif not auth:
                        raise RuntimeError("Command requires auth")
                    elif cmd == "Bounce":
                        self.send_bounce(
                            args.get("games", []),
                            args.get("slots", []),
                            args.get("tags", []),
                            args["data"],
                        )
                    elif cmd == "ConnectUpdate":
                        if args.get("items_handling", None) is not None:
                            connection_data.items_handling = args["items_handling"]
                        if args.get("tags", None) is not None:
                            connection_data.tags = args["tags"]
                    elif cmd == "Get":
                        args["keys"] = {key: self.data_storage[key] for key in args["keys"]}
                        self.send_retrieved(conn, args)
                    elif cmd == "LocationChecks":
                        for location_id in args["locations"]:
                            player_items = self.player_items[connection_data.slot - 1]
                            i = len(player_items)
                            player_items.append({
                                "item": location_id,
                                "location": location_id,
                                "player": connection_data.slot,
                                "flags": 0,
                            })
                            self.send_items(conn, 0, [player_items[i]])
                        self.send_room_update(conn, checked_locations=args["locations"])
                    elif cmd == "LocationScouts":
                        items: List[Dict[str, Any]] = []
                        for location_id in args["locations"]:
                            items.append({
                                "item": location_id,
                                "location": location_id,
                                "player": connection_data.slot,
                                "flags": 0,
                            })
                            self.print_json(conn, [{"text": "*insert hint here*"}], {})
                        self.send_location_info(conn, items)
                    elif cmd == "UpdateHint":
                        for arg in ("player", "location", "status"):
                            if not isinstance(args[arg], int):
                                raise ValueError(f"Invalid argument {arg} to UpdateHint")
                        self.print_json(conn, [{"text": "*insert hint here*"}], {})
                    elif cmd == "CreateHints":
                        if not all(isinstance(location, int) for location in args["locations"]):
                            raise ValueError(f"Invalid argument locations to CreateHints")
                        if not len(args["locations"]):
                            raise ValueError(f"Expected >0 locations for CreateHints")
                        for arg in ("player", "status"):
                            if not isinstance(args.get(arg, 0), int):
                                raise ValueError(f"Invalid argument {arg} to CreateHints")
                        self.print_json(conn, [{"text": "*insert hints here*"}], {})
                    elif cmd == "Say":
                        text = f"{name}: {args['text']}"
                        self.print_json(conn, [{"text": text}], {})
                    elif cmd == "SetNotify":
                        self.set_notify = set(args["keys"])
                    elif cmd == "Set":
                        key = args["key"]
                        value = self.data_storage.get(key, args["default"])
                        args["original_value"] = copy.copy(value)
                        operations = args["operations"]
                        if len(operations) > 1:
                            raise NotImplementedError()
                        elif len(operations) == 1:
                            op = operations[0]["operation"]
                            op_arg = operations[0]["value"]
                            if op != "replace":
                                raise NotImplementedError()
                            value = op_arg
                        args["value"] = self.data_storage[key] = value
                        args["slot"] = connection_data.slot
                        if key in self.set_notify or args["want_reply"]:
                            self.send_set_reply(conn, args)
                    elif cmd == "StatusUpdate":
                        connection_data.status = args["status"]
                    elif cmd == "Sync":
                        self.send_items(conn, 0, self.player_items[connection_data.slot - 1])
                    # ...
                    else:
                        raise RuntimeError("Unknown command")
        except Exception as ex:
            self._exception = ex
        finally:
            if connection_data in self._connections:
                self._connections.remove(connection_data)
            conn.close()

    def send_room_info(self, conn: ServerConnection) -> None:
        conn.send(json.dumps([{
            "cmd": "RoomInfo",
            "seed_name": "seed",
            "time": time.time(),
            "version": {"major": 0, "minor": 6, "build": 3, "class": "Version"},
            "tags": ["Test"],
            "hint_cost": 50,
            "games": sorted(set(self.player_games)),
            "permissions": {
                "release": 0,
                "collect": 0,
                "remaining": 0,
            },
        }]))

    def send_connected(self, conn: ServerConnection) -> None:
        conn.send(json.dumps([{
            "cmd": "Connected",
            "team": 0, "slot": 1,
            "players": [
                {"team": 0, "slot": 1, "alias": "Player1", "name": "Player1"},
            ],
            "missing_locations": [],
            "checked_locations": [],
            "slot_info": {
                "1": {
                    "name": "Player1",
                    "game": "Game",
                    "type": 1,
                    "group_members": [],
                }
            },
            "hint_points": 7,
        }]))

    def send_connection_refused(self, conn: ServerConnection, connect_errors: List[str]) -> None:
        conn.send(json.dumps([{
            "cmd": "ConnectionRefused",
            "errors": connect_errors,
        }]))

    def send_datapackage(self, conn: ServerConnection, games: List[str]) -> None:
        conn.send(json.dumps([{
            "cmd": "DataPackage",
            "data": {"games": {name: {} for name in games}},
        }]))

    @staticmethod
    def print_json(
            conn: ServerConnection,
            data: List[Dict[str, Any]],
            additional_arguments: Dict[str, Any]
    ) -> None:
        conn.send(json.dumps([{
            "cmd": "PrintJSON",
            "data": data,
            **additional_arguments,
        }]))

    def print_all(self, message: str) -> None:
        for connection_data in self._connections:
            connection_data.connection.send(json.dumps([{
                "cmd": "Print",
                "text": message,
            }]))

    def send_bounce(self, games: List[str], slots: List[int], tags: List[str], data: Dict[str, Any]) -> None:
        for connection_data in self._connections:
            if (connection_data.slot not in slots and
                    not any(tag in tags for tag in connection_data.tags) and
                    self.player_games[connection_data.slot - 1] not in games):
                continue
            connection_data.connection.send(json.dumps([{
                "cmd": "Bounced",
                "games": games,
                "slots": slots,
                "tags": tags,
                "data": data,
            }]))

    def send_items(self, conn: ServerConnection, index: int, items: List[Dict[str, Any]]) -> None:
        conn.send(json.dumps([{
            "cmd": "ReceivedItems",
            "index": index,
            "items": items,
        }]))

    def send_room_update(
            self,
            conn: ServerConnection,
            checked_locations: Optional[List[int]] = None,
            permissions: Optional[Dict[str, int]] = None,
    ) -> None:
        packet: Dict[str, Any] = {
            "cmd": "RoomUpdate",
        }
        if checked_locations is not None:
            packet["checked_locations"] = checked_locations
        if permissions is not None:
            packet["permissions"] = permissions
        conn.send(json.dumps([packet]))

    def send_location_info(self, conn: ServerConnection, items: List[Dict[str, Any]]) -> None:
        packet: Dict[str, Any] = {
            "cmd": "LocationInfo",
            "locations": items,
        }
        conn.send(json.dumps([packet]))

    def send_retrieved(self, conn: ServerConnection, data: Dict[str, Any]) -> None:
        data["cmd"] = "Retrieved"
        conn.send(json.dumps([data]))

    def send_set_reply(self, conn: ServerConnection, data: Dict[str, Any]) -> None:
        data["cmd"] = "SetReply"
        conn.send(json.dumps([data]))
