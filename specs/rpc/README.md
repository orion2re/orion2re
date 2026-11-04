# Optional local RPC foundation

The listener uses libwebsockets v4.5.4 with the existing pinned project libuv.
The exact backend revision, source URL, license and build options are in
[backend.json](backend.json). It is an external checkout, not downloaded by CMake.
MIT and retained permissive component notices are copied from its `LICENSE` to
the build's `data/licenses/libwebsockets.txt`; include that file in distributions.
TLS, compression, client support and backend test applications are disabled.

## Build and configure

```sh
git clone --branch v4.5.4 https://github.com/warmcat/libwebsockets.git /path/to/libwebsockets
cmake -S . -B out/build/rpc-debug -DCMAKE_BUILD_TYPE=Debug \
  -DORION2RE_BUILD_TESTS=ON -DORION2RE_ENABLE_RPC=ON \
  -DORION2RE_RPC_BACKEND_DIR=/path/to/libwebsockets
cmake --build out/build/rpc-debug --target orion2re test_rpc test_config_snapshot
```

The backend must match the pinned revision and have no tracked modifications.
RPC is off by default. Disabled and router-only builds do not require or link
libwebsockets, even if the router-only configuration sets `ORION2RE_ENABLE_RPC`.

Enable the listener in a private local configuration:

```text
interface rpc_enabled = true;
interface rpc_port = 17362;
interface rpc_key = "replace-with-your-private-key";
```

An empty key prevents startup. Settings require restart and are excluded from
saves and multiplayer synchronization. Configuration keys are CP850 strings
(maximum 128 bytes), converted to UTF-8 at service startup. The service copies
its startup configuration; runtime reloads do not mutate the listening thread.
Bind failure logs a redacted diagnostic and leaves the game usable.

## Current protocol

Connect to `ws://127.0.0.1:17362/rpc`, optionally selecting subprotocol
`orion2.rpc`. Only loopback Host headers matching the listening port and absent
or HTTP(S) loopback Origins are accepted. Query strings are forbidden. Send
JSON-RPC 2.0 text messages, starting with:

```json
{"jsonrpc":"2.0","id":1,"method":"rpc.hello","params":{"key":"your-configured-key","requestedRole":"reader"}}
```

`requestedRole` is `reader` (default) or `controller`. Only one connection may
hold the controller role; disconnect releases it. Roles cannot change on an
authenticated connection. Invalid credentials return unauthorized and close.
Authenticate within five seconds. Never put the key in URLs or logs.

Authenticated `rpc.describe` takes no parameters (or `{}`) and advertises only
`rpc.hello` and `rpc.describe`, under provisional contract `orion2.rpc/0`.
Game queries, actions, event selection and state notifications are not yet
implemented or advertised. Controller status grants no game operations yet.

Requests use a nonempty string ID up to 64 bytes or a JSON integer within the
JavaScript exact-integer range; null and fractional IDs and batches are rejected.
Duplicate keys, unknown envelope/parameter members and excessive nesting are
rejected. Valid notifications are ignored without authentication or side effects.
Errors never include request payloads or credentials.

Limits: four connections, 64 KiB input per message, depth 32, 32 messages and
128 receive callbacks per second per connection. Each connection has at most
16 queued responses totaling 64 KiB. Oversize, binary, invalid UTF-8, flooding
or output overflow closes the connection. Fragmented input and ordered output
are handled on the service thread.

The service owns a libuv loop and attaches libwebsockets to it. Its only periodic
application timer checks authentication deadlines; it never reads game state.
Startup precedes the game worker, and shutdown wakes and joins the service after
the game loop returns. No game-thread polling, world cache or subscription
fingerprints are introduced.

## Verification

```sh
ctest --test-dir out/build/rpc-debug --output-on-failure \
  -R '^(test_rpc|rpc_transport|test_config_snapshot)$'
cmake --build out/build/rpc-debug --target config-generated-check
```

`rpc_transport` uses a standalone asset-free test server and Python's standard
library. Repeat with `ORION2RE_ENABLE_ASAN=ON` in a separate Debug build for
AddressSanitizer/UBSan checks. Also build with RPC disabled and router-only.
Cross-platform builds and interactive game startup require separate validation.
