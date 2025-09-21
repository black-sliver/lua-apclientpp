import os.path
import socket
import ssl
from enum import Enum
from typing import Optional
from unittest import skipIf

from .server import WSServer
from .bases import ClientTestCase
from .util import LuaTable, TimeoutLoop


# apclientpp has special handling for localhost, so need a "real" IP
try:
    local_ip = [
        ip
        for ip in socket.gethostbyname_ex(socket.gethostname())[2]
        if not ip.startswith("127.") and ip != "::1"
    ][0]
except IndexError:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 53))
        local_ip = s.getsockname()[0]
        s.close()
    except socket.error:
        local_ip = "127.0.0.1"


class SocketState(Enum):
    NONE = 0
    CONNECTED = 1
    DISCONNECTED = 2
    ERROR = -1


class ConnectionTest(ClientTestCase):
    socket_state: SocketState = SocketState.NONE
    server: WSServer

    def start_server(self, cert: str = "", key: str = "") -> int:
        ssl_context: Optional[ssl.SSLContext] = None
        if cert or key:
            ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            ssl_context.load_cert_chain(cert, keyfile=key)
        self.server = WSServer(ssl=ssl_context)
        self.server.start()
        for _ in TimeoutLoop(lambda: self.server.port == 0):
            pass
        return self.server.port

    def tearDown(self) -> None:
        super().tearDown()
        if hasattr(self, "server"):
            self.server.stop()

    def on_socket_connected(self) -> None:
        self.socket_state = SocketState.CONNECTED

    def on_socket_error(self, reason: str) -> None:
        self.socket_state = SocketState.ERROR

    def on_socket_disconnected(self) -> None:
        # self.socket_state = SocketState.DISCONNECTED
        pass

    # Test connection behavior when server is not running
    def test_auto_connect_error_when_not_running(self) -> None:
        unused_port = 38289
        self.uri = f"127.0.0.1:{unused_port}"
        self.connect()
        with self.assertRaises(TimeoutError):
            # NOTE: needs a long timeout because this may test both WS and WSS
            for _ in TimeoutLoop(lambda: self.socket_state != SocketState.CONNECTED, timeout=3.5):
                self.poll()
        self.assertEqual(self.socket_state, SocketState.ERROR)

    def test_ws_connect_error_when_not_running(self) -> None:
        unused_port = 38289
        self.uri = f"ws://127.0.0.1:{unused_port}"
        self.connect()
        for _ in TimeoutLoop(lambda: self.socket_state != SocketState.ERROR):
            self.poll()

    def test_wss_connect_error_when_not_running(self) -> None:
        unused_port = 38289
        self.uri = f"wss://127.0.0.1:{unused_port}"
        self.connect()
        for _ in TimeoutLoop(lambda: self.socket_state != SocketState.ERROR):
            self.poll()

    # Test connection behavior for server not using TLS
    def test_auto_connect_ok_when_plain(self) -> None:
        port = self.start_server()
        self.uri = f"127.0.0.1:{port}"
        self.connect()
        # NOTE: needs a long timeout because this may test both WS and WSS
        for _ in TimeoutLoop(lambda: self.socket_state != SocketState.CONNECTED, timeout=3.5):
            self.poll()

    def test_ws_connect_ok_when_plain(self) -> None:
        port = self.start_server()
        self.uri = f"ws://127.0.0.1:{port}"
        self.connect()
        for _ in TimeoutLoop(lambda: self.socket_state != SocketState.CONNECTED):
            self.poll()

    def test_wss_connect_error_when_plain(self) -> None:
        port = self.start_server()
        self.uri = f"wss://127.0.0.1:{port}"
        self.connect()
        for _ in TimeoutLoop(lambda: self.socket_state != SocketState.ERROR):
            self.poll()

    # Test connection behavior for server using TLS with untrusted certificate
    @skipIf(not os.path.exists("untrusted.pem"), "Please generate untrusted.pem")
    def test_wss_connect_localhost_even_if_untrusted(self) -> None:
        port = self.start_server("untrusted.pem", "untrusted-key.pem")
        self.uri = f"wss://127.0.0.1:{port}"
        self.connect()
        for _ in TimeoutLoop(lambda: self.socket_state != SocketState.CONNECTED):
            self.poll()

    @skipIf(local_ip == "127.0.0.1", "Could not get local IP address")
    @skipIf(not os.path.exists("untrusted.pem"), "Please generate untrusted.pem")
    def test_auto_connect_error_when_untrusted(self) -> None:
        port = self.start_server("untrusted.pem", "untrusted-key.pem")
        self.uri = f"{local_ip}:{port}"
        self.connect()
        with self.assertRaises(TimeoutError):
            for _ in TimeoutLoop(lambda: self.socket_state != SocketState.CONNECTED, timeout=3.5):
                self.poll()
            if self.socket_state == SocketState.CONNECTED:
                raise RuntimeError("Connected to untrusted host")
        self.assertEqual(self.socket_state, SocketState.ERROR)

    @skipIf(not os.path.exists("untrusted.pem"), "Please generate untrusted.pem")
    def test_ws_connect_error_when_untrusted(self) -> None:
        port = self.start_server("untrusted.pem", "untrusted-key.pem")
        self.uri = f"ws://127.0.0.1:{port}"
        self.connect()
        for _ in TimeoutLoop(lambda: self.socket_state != SocketState.ERROR, timeout=1):
            self.poll()

    @skipIf(local_ip == "127.0.0.1", "Could not get local IP address")
    @skipIf(not os.path.exists("untrusted.pem"), "Please generate untrusted.pem")
    def test_wss_connect_error_when_untrusted(self) -> None:
        port = self.start_server("untrusted.pem", "untrusted-key.pem")
        self.uri = f"wss://{local_ip}:{port}"
        self.connect()
        for _ in TimeoutLoop(lambda: self.socket_state != SocketState.ERROR, timeout=1):
            self.poll()

    # Test connection behavior for server using TLS with trusted certificate
    # IMPORTANT: set SSL_CERT_FILE env var, i.e. `SSL_CERT_FILE=trusted.pem pytest`
    @skipIf(local_ip == "127.0.0.1", "Could not get local IP address")
    @skipIf(not os.path.exists("trusted.pem"), "Please generate trusted.pem")
    def test_auto_connect_ok_when_trusted(self) -> None:
        port = self.start_server("trusted.pem", "trusted-key.pem")
        self.uri = f"{local_ip}:{port}"
        self.connect()
        for _ in TimeoutLoop(lambda: self.socket_state != SocketState.CONNECTED, timeout=3.5):
            self.poll()

    @skipIf(local_ip == "127.0.0.1", "Could not get local IP address")
    @skipIf(not os.path.exists("trusted.pem"), "Please generate trusted.pem")
    def test_wss_connect_ok_when_trusted(self) -> None:
        port = self.start_server("trusted.pem", "trusted-key.pem")
        self.uri = f"wss://{local_ip}:{port}"
        self.connect()
        for _ in TimeoutLoop(lambda: self.socket_state != SocketState.CONNECTED, timeout=1):
            self.poll()
