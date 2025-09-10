from .bases import E2ETestCase


class TestCompression(E2ETestCase):
    def test_compressed(self) -> None:
        from websockets.extensions.permessage_deflate import PerMessageDeflate

        self.assertTrue(self.server._connections)
        for connection_data in self.server._connections:
            conn = connection_data.connection
            self.assertTrue(any(isinstance(extension, PerMessageDeflate) for extension in conn.protocol.extensions))
