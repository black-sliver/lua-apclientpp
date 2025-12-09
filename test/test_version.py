from .bases import APITestCase


class TestEnums(APITestCase):
    def test_version(self) -> None:
        v = self.apclient["_VERSION"]
        self.assertIsInstance(v, str)
        self.assertIn(".", v)
        for part in v.split("."):
            self.assertTrue(part.isdigit())

    def test_api_version_matches(self) -> None:
        self.assertEqual(self.apclient["_VERSION"], self.api["_VERSION"])
