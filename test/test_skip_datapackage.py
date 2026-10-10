from typing import Any

from .bases import E2ETestCase


class WithoutDataPackageTest(E2ETestCase):
    def connect(self, *args: Any) -> None:
        super().connect(True)

    def test_did_not_request_datapackage(self) -> None:
        self.assertFalse(self.server._connections[0].requested_datapackage)


class WithDataPackageTest(E2ETestCase):
    def connect(self, *args: Any) -> None:
        super().connect(False)  # explicit false

    def test_did_not_request_datapackage(self) -> None:
        self.assertTrue(self.server._connections[0].requested_datapackage)


class DefaultDataPackageTest(E2ETestCase):
    def test_requested_datapackage(self) -> None:
        self.assertTrue(self.server._connections[0].requested_datapackage)
