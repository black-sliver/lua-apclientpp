# Wait, what?

Tests for lua-apclientpp are written in Python using lupa.

## But why?

We are testing the real lib with real Lua.
Getting a websocket server to work across all supported Lua versions has proven difficult.

## How to run tests

```sh
pip install -r test/requirements.txt pytest pytest-xdist
pytest -nauto
```

## Coverage

The lib can be built with coverage enabled.
The output can then be parsed by a tool.

E.g. with gcc and gcovr
```sh
CFLAGS="-fprofile-arcs -ftest-coverage" ./build_posix.sh  # build with instrumentation
pytest -nauto  # this will then generate coverage data
gcovr --exclude subprojects  # generate report
```

We aim for high coverage to hopefully not miss any native crashes.
Beware that branch coverage is unlikely to be a valuable metric because it includes error states and exceptions that are
very hard to produce in tests.

## ASAN, UBSAN

ASAN (address sanitizer) and UBSAN (undefined behavior sanitizer) allow finding memory errors during execution.
You can build the lib with ASAN and UBSAN enabled, but need to load them when executing Python.
See CI.yaml for an example on Linux.

**Warning:** ASAN and UBSAN have an impact on compile time and runtime, so it's only recommended to use them during
testing, not during development nor in production.

## Encryption tests

To test encrypted connections, generate a trusted and an untrusted pair of PEM files.
And set the SSL_CERT_FILE env var to the trusted cert.

```sh
openssl req -x509 -newkey ed25519 -days 1 \
  -noenc -keyout trusted-key.pem -out trusted.pem -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

openssl req -x509 -newkey ed25519 -days 1 \
  -noenc -keyout untrusted-key.pem -out untrusted.pem -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

SSL_CERT_FILE=trusted.pem pytest -nauto
```

## Contributing to tests

Contributions are welcome. The existing tests should hopefully be a good template to write new tests.

What's missing from tests:

* See TODOs in test_properties.py
* Isolated tests for GetDataPackage. See server.py for a current bug.
* All the things in render_json
* Figure out why running ConnectSlot while not connected crashes
  * see `@skipIf(True, ...)`
* Test more function calls while not connected
* Fuzz/test behavior when server is sending garbage
* call that triggers assign_set and contains
