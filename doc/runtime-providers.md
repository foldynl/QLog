# Runtime providers

## Architecture

`RuntimeContext::sources()` → choose sources → `RuntimeContext::collect()` → queued request → `RuntimeProvider` in the owner's
thread → all field getters → queued reply → `contextReady()` → IPC/MCP.

| File | Responsibility |
| --- | --- |
| `mcpbridge/RuntimeProvider.h/.cpp` | Generic fields, metadata and reading in the owner's thread. |
| `mcpbridge/RuntimeContext.h/.cpp` | Named source registration, metadata, selected collection, timeouts and provider lifetime. |
| `mcpbridge/RuntimeServer.h/.cpp` | JSON transport at the fixed `qlog-runtime` endpoint. |
| `rig/RigRuntimeProvider.h/.cpp` | Ordinary factory function defining rig fields and their value semantics. |
| `rig/Rig.h/.cpp` | Owns the provider and the state being read. |
| `core/main.cpp` | Registers providers during subsystem startup. |

The core has no dependency on rig types or QLog domain objects. A provider is a
regular `QObject` child, with field descriptions and short reading functions.
There are no snapshot templates or provider subclasses.

## Add a field

Add one call where the owner's provider is configured:

```cpp
statusProvider->addNumber(
    "sensor_temperature",
    QStringLiteral("Latest temperature reported by the sensor, in degrees Celsius "
                   "(degC). Null when disconnected or no reading is available."),
    QStringLiteral("degC"),
    [this]()
    {
        return sensorStatus.isConnected ? QJsonValue(sensorStatus.temperatureC)
                                        : QJsonValue();
    });
```

| Method | Arguments | Schema type |
| --- | --- | --- |
| `addBoolean` | name, description, getter | `boolean` |
| `addNumber` | name, description, unit, getter | `number` |
| `addString` | name, description, getter | `string` |

- Getters return `QJsonValue`; ordinary `bool`, number and `QString` results also
  convert automatically. Return `QJsonValue()` for an unknown value.
- Metadata is generated automatically, including `nullable: true`. For a number
  without a unit, pass `QString()`. Do not define schema separately.
- Names must be globally unique, preferably prefixed by the subsystem.
- Names, descriptions and getters must be present. Adding an invalid or duplicate
  field returns `false` and logs a warning. Existing definitions remain intact.
- A wrong JSON value type, undefined value or non-finite number is returned as
  `null` and logged. Apply domain constraints such as a positive frequency in
  the getter. Explicitly returning `null` is normal and does not log a warning.
- Descriptions are fixed English `QStringLiteral` strings, without `tr()`.
  Explain the meaning, source, units, and when the value is unknown. Include an
  example when useful. Distinguish similarly named concepts. Describe stored
  state accurately; getters do not poll hardware.

Adding a field requires no changes to Context, IPC or QLog-MCP.

## Rig source

`sources=["rig"]` selects all fields below. Their definitions are advertised by
QLog through `runtime.get_schema` and `qlog.get_schema(domain="runtime", sources=["rig"])`.
The MCP client accepts the additional fields without a Python registry or new tools.

| Field | Meaning / unit |
| --- | --- |
| `rig_connected` | Ready driver connection; does not indicate radio power or PTT. |
| `rig_profile` | Active radio-control profile, not a logging-station profile. |
| `rig_backend` | Active control backend: Hamlib, Omnirig v1/v2, FLRig or TCI. |
| `rig_model` | Configured Hamlib manufacturer/model label, including driver version; not verified hardware identity or radio firmware. Null for other backends or unknown underlying NET rigctl models. |
| `rig_frequency` | Main VFO frequency, Hz. |
| `rig_mode` | Submode when available, otherwise operating mode. |
| `rig_power` | Backend-reported RF power setting, W; not an instantaneous RF meter reading. TCI estimates W from drive percentage and configured default power (100 W when unset). |
| `rig_key_speed` | Transceiver CAT keyer speed, WPM; not external-keyer speed. |
| `rig_vfo` | Backend's active VFO label; `Curr` does not identify A or B. |
| `rig_rit`, `rig_xit` | Effective signed tuning offsets, Hz. Zero does not identify the RIT/XIT switch state. |
| `rig_bandwidth` | Positive receiver filter width, Hz. TCI currently supplies no width. |
| `rig_split_enabled` | Reported split state; `false` is distinct from unknown. |
| `rig_tx_frequency` | Separate TX VFO frequency for the active split session, Hz; not a guarantee of final RF frequency after XIT or transverter settings. |

The `Rig::Status::*Known` flags distinguish initialized values from enabled,
supported driver reports. The existing handlers in `Rig::__openRig()` set them
after a report arrives, using that connection's profile settings and radio
capabilities. `Status::clear()` resets values and flags when the driver closes.
Disabling split invalidates the TX reading; a new split session needs a new TX
report. No getter queries the driver or the hardware.

`Rig::__openRig()` resolves the model label once when Hamlib becomes ready, using
the existing `getModelList()` and the original `controlledRigModel`. Local rigctld
sharing therefore retains the selected physical model. An external NET rigctl
connection with no known underlying model returns `null`; slot names from other
backends are not exposed as radio models. No new driver API or MCP change is needed.

When adding another optional rig reading:

1. Add its value and a `...Known = false` flag to `Rig::Status`.
2. Set the value and flag in the existing driver-report handler. Gate the flag
   with the connection's read setting and capability; do not mark a default as known.
   Ensure the driver emits its first valid report even when it equals a default
   such as `false` or zero. Failed reads must not produce invented readings.
   When a value combines multiple device reports (TCI RIT/XIT enable and offset),
   wait for the required reports before emitting it.
3. Add its getter in `RigRuntimeProvider.cpp`, requiring a connected rig, the
   flag and the value's domain constraints. Zero may be valid; do not use it as
   a universal unknown sentinel. Convert MHz to Hz for public frequency/offset fields.
4. Update the `rig` source description in `core/main.cpp` when coverage changes.
   Add focused availability, reset, unit and thread tests. MCP code needs no change.

Unsupported, disabled, not-yet-reported, disconnected or invalid readings return
`null`. Positive-only readings (CW speed, filter width, TX frequency) reject
zero; reported power and effective RIT/XIT offsets may be zero. Field descriptions
are the authority for these distinctions. Read live values again for each new
current-state question; only the source list and field schema may be reused.

## Add a new source

### 1. Create its provider in the owner

In the existing owner header, forward-declare `RuntimeProvider` and add:

```cpp
public:
    RuntimeProvider *getRuntimeProvider() const;

private:
    RuntimeProvider *statusProvider;
```

In the owner's `.cpp`, include `mcpbridge/RuntimeProvider.h`. In its constructor,
after initializing the state:

```cpp
statusProvider = new RuntimeProvider(this);
statusProvider->addBoolean(
    "sensor_connected",
    QStringLiteral("Whether the sensor has a ready connection. "
                   "Null if its state could not be collected."),
    [this]() { return QJsonValue(sensorStatus.isConnected); });
```

Add other fields with the methods above. Implement the getter:

```cpp
RuntimeProvider *Sensor::getRuntimeProvider() const
{
    FCT_IDENTIFICATION;
    return statusProvider;
}
```

No new class, `Q_OBJECT`, snapshot type or qmake entry is needed. For a large
set of fields, an ordinary factory function may keep the definitions together;
see `createRigRuntimeProvider()` for an example. Any referenced state must
outlive the provider. Never capture a temporary or constructor-local status.

### 2. Register during startup

Pass the existing `RuntimeContext &` to the subsystem startup function. Register
in the context's thread, after `moveToThread()` and before starting the worker:

```cpp
sensor->moveToThread(sensorThread);
if ( !runtimeContext.addProvider(
        QStringLiteral("sensor"),
        QStringLiteral("Latest sensor connection state and temperature in degrees Celsius. "
                       "Use for current temperature or whether the sensor has a ready "
                       "connection. Reads the latest state already held by the application, "
                       "without polling hardware. Temperature is null when disconnected or "
                       "unknown. This source contains no historical measurements."),
        sensor->getRuntimeProvider()) )
    qCWarning(runtime) << "Cannot register sensor runtime provider";
sensorThread->start();
```

Source names must be unique, non-empty and have no surrounding whitespace;
descriptions must be non-empty. Define both once at registration. `sources()`
automatically returns the current registry as `{"sources": {"sensor": {"description": ...}}}`.
Removal on provider destruction or worker shutdown also removes the source.
Describe the included state, useful questions, units, unknown values and what
is not exposed. Use fixed English `QStringLiteral`, without `tr()`; do not
infer names or descriptions from field prefixes or class names. In MCP, these
descriptions let an LLM choose a source before loading detailed field metadata.

For an object remaining in the main thread, omit the move and worker startup.
The provider follows its parent when moved and is destroyed with it. Do not
manually delete it or introduce another copy of the application state.

## Thread rules

- Define fields in the owner's thread before registering. Registration freezes
  the definitions, including a failed registration attempt. Later additions
  return `false`. Collecting directly also freezes the definitions.
- Metadata access and freezing are synchronized internally. The mutex protects
  field definitions, not the application's status. No lock is held while getters
  execute.
- Context requests and provider replies use queued connections. Getters run in
  the owner's thread; capturing `this` does not itself provide thread safety.
- Change status in its owning thread. Other threads request changes through
  queued signals/slots. If state is genuinely shared, its owner must provide a
  consistent synchronized read; separate locks for individual fields do not
  guarantee a coherent collection.
- All getters of a provider run consecutively in one event handler. Keep them
  short and read-only: no I/O, hardware polling, waiting, `processEvents()` or
  nested event loops. Queued status updates cannot interrupt this handler.
- Consistency is per provider, not atomic across different providers or threads.
- The owner needs a running event loop. An unresponsive worker produces a
  timeout; the Context does not block waiting for it.
- Register while the provider is guaranteed alive. Do not change its thread
  afterwards. Destruction or worker shutdown removes its registration.
- `RuntimeContext::schema()` belongs to the context's thread. Its `collect()`
  forwards calls from other threads; the object must remain alive during a call.

## IPC contract

`main()` starts the fixed endpoint `qlog-runtime`. Connect using `QLocalSocket`.
Protocol 1 uses UTF-8 JSON objects, each terminated by a newline. Example:

```json
{"id":"request-1","method":"runtime.get_context"}
```

Supported methods: `system.get_info`, `runtime.list_sources`, `runtime.get_schema`,
`runtime.get_context`. `system.get_info` returns `protocol`, `application`, and
`version`. `runtime.list_sources` returns source names and descriptions without
running getters. These two methods accept only absent or empty `params`.

Schema and context accept optional `params: {"sources": ["sensor", "rig"]}`.
The list must be non-empty and contain registered source names. Unknown names,
non-string items and unsupported parameters are rejected with `invalid_params`
before any getter runs. Repeated names read the provider only once. Omitted
selection (absent `params` or `{}`) preserves the previous all-source behavior.
Schema returns only selected fields, with `source` identifying their provider;
context returns only their `values` and `issues`. Selection occurs in Context
before dispatching requests to owner threads. An unselected provider is not
queued, read or awaited.

IDs are strings or integers within
[-9007199254740991, 9007199254740991]; use strings for larger identifiers.
Replies contain `{"id":...,"result":...}` or
`{"id":...,"error":{"code":...,"message":...}}`. Match replies by ID;
asynchronous replies may arrive out of order. Invalid IDs and malformed JSON
produce an error with `id: null`.

The server allows at most 16 active clients; additional connections are closed.
A connection is closed after 5 seconds without a complete newline-terminated
request. Partial bytes do not renew this deadline. The connection is also closed
if a request exceeds 64 KiB including its newline, or queued output would exceed
1 MiB. A collection uses the default 1000 ms provider timeout and returns the
`values`/`issues` structure described below.
Adding fields or providers requires no transport changes.

On Linux, startup recovers a socket left by a crash only when a connection probe
is refused and the endpoint is a socket. Live endpoints, ordinary files and
symlinks are not removed; other listen failures are logged. Other platforms do
not automatically remove endpoints: connection refusal can also indicate a full
live listener backlog on some Unix systems.

## Consume a collection inside QLog

Connect `RuntimeContext::contextReady(quint64 requestId, QJsonObject context)`
to a consumer QObject before calling `collect(requestId, timeoutMs)`.
Use `collect(requestId, timeoutMs, sources)` for selected source names.
The default empty `QStringList` means all sources for C++ compatibility; unlike
the wire API, it is not an explicit empty JSON selection. Validate names using
`hasSource()` before collecting or requesting `schema(sources)`. Both metadata
methods belong to the context thread. Default timeout: 1000 ms.
Calls can finish immediately, including busy or empty
collections; do not assume every reply is deferred.

The result contains `values` and `issues`, both keyed by field name.
`schema()` returns `{"fields": {...}}` with each field's metadata.

| Issue | Meaning |
| --- | --- |
| `context_busy` | Another collection is active. |
| `invalid_timeout` | Timeout is zero or negative, with no active collection. |
| `provider_busy` | A previous timed-out request still awaits its reply. |
| `provider_timeout` | This provider did not reply before the deadline. |
| `provider_unavailable` | Provider was removed while its reply was pending. |

Unavailable values are `null`. A field getter returning `null` does not add an
issue. Successful providers retain their values when another provider fails.
Late replies cannot complete a newer collection; each provider has at most one
outstanding request from a given context.

## MCP integration

Paths in this section are relative to the sibling `QLog-MCP` repository.

### Existing implementation

| File | Responsibility |
| --- | --- |
| `src/qlog_mcp/runtime.py` | `RuntimeClient`: source listing, selected requests, local connection and validation. `LiveSources`: named descriptions. `LiveContext`: generic `values` and `issues` maps. |
| `src/qlog_mcp/server.py` | Creates `RuntimeClient` and exposes `qlog.list_live_sources`, `qlog.get_live_context(sources=...)`, `qlog.get_schema(domain="runtime", sources=...)` and runtime capabilities. Contains LLM routing instructions and tool descriptions. |
| `src/qlog_mcp/errors.py` | `RuntimeUnavailableError` for connection/timeout failures; `RuntimeProtocolError` for invalid responses. |
| `tests/test_runtime.py` | Socket-peer tests of public MCP tools, new fields, metadata validation, fresh values, partial failures and unavailable QLog. |

The request path is:

```text
qlog.list_live_sources()            -> RuntimeClient.sources()        -> runtime.list_sources
qlog.get_live_context(sources)      -> RuntimeClient.context(sources) -> runtime.get_context
qlog.get_schema(runtime, sources)   -> RuntimeClient.schema(sources)  -> runtime.get_schema
```

Each call opens a connection to the fixed `qlog-runtime` endpoint, checks
`system.get_info` for application `QLog` and protocol `1`, then sends the requested
method. The client uses a two-second total timeout and a 1 MiB response limit.
There is no endpoint discovery and no connection during MCP server construction.
Runtime tools do not open the database or require a station scope.

`RuntimeClient.schema()` validates metadata received from QLog: `type` and
`description` must be strings, `nullable` must be a boolean, and optional `unit`
must be a string. Additional metadata is preserved. Optional `source` metadata
must be a string when present. Definitions belong to QLog; do not duplicate
them in Python. `RuntimeClient.sources()` validates a
`LiveSources` result: a dictionary of source names with non-empty string
descriptions. Additional source metadata is preserved.

`LiveContext.values` is `dict[str, JsonValue]`; `issues` is `dict[str, str]`.
Field names are dynamic dictionary keys, not separate Pydantic attributes.
The model rejects invalid context structure and non-finite numbers. New field
names do not require changes to this model or to MCP tool registration.

### Add a QLog field or provider

**No MCP implementation change is required.** Define and register it in QLog,
rebuild/restart QLog, and refresh the client's cached runtime schema.
The MCP server can keep running: it fetches the current response on each call.
Refresh `qlog.list_live_sources()` to discover the newly registered source.
Names are dynamic, not a Python enum or a separate MCP tool per provider.

For the `sensor_temperature` example, use these existing tools:

```text
qlog.list_live_sources({})
qlog.get_schema({"domain": "runtime", "sources": ["sensor"]})
qlog.get_live_context({"sources": ["sensor"]})
```

Their results have different envelopes. The schema is keyed by domain:

```json
{
  "runtime": {
    "fields": {
      "sensor_temperature": {
        "type": "number",
        "description": "Latest temperature reported by the sensor, in degrees Celsius (degC). Null when disconnected or no reading is available.",
        "nullable": true,
        "source": "sensor",
        "unit": "degC"
      }
    }
  }
}
```

The live result contains values and per-field collection issues:

```json
{
  "values": {"sensor_temperature": 21.5},
  "issues": {}
}
```

An in-process Python consumer can use the same public tools:

```python
from fastmcp import Client
from qlog_mcp.server import create_server

async with Client(create_server()) as client:
    available = await client.call_tool("qlog.list_live_sources")
    assert "sensor" in available.structured_content["sources"]
    schema = await client.call_tool(
        "qlog.get_schema", {"domain": "runtime", "sources": ["sensor"]}
    )
    live = await client.call_tool("qlog.get_live_context", {"sources": ["sensor"]})
    metadata = schema.data["runtime"]["fields"]["sensor_temperature"]
    value = live.structured_content["values"]["sensor_temperature"]
    issue = live.structured_content["issues"].get("sensor_temperature")
```

Run this example inside an async function with QLog exposing the sensor provider.
Only access a field after confirming it exists in the schema. For a known field,
`null` means unknown or unavailable, never an invented zero or `false`. Preserve
successful fields when another provider fails. A getter returning `null` can
have no issue; consult the QLog description for its domain meaning.

The LLM first uses source descriptions to choose relevant subsystems, then
loads the schema for those sources and requests their live context. Reuse the
source list and known field definitions; fetch a new source's schema when first
needed. `sources` is optional only for backward compatibility: omitted or null
selects all, while an empty list is invalid. On `qlog.get_schema`, a selection
is accepted only for `domain="runtime"`. No MCP-side filtering or provider
registry is used. If QLog rejects a selection, MCP asks the caller to refresh
`qlog.list_live_sources` and use available names.

The LLM learns field meanings from the runtime schema. The server instructions
route current-state questions to `qlog.get_live_context`; historical contacts use
the QSO tools. Refresh schema when QLog restarts or its providers change, and
read live context again for updated values. If QLog is not running, report that
plainly in the operator's language. A failed read leaves current state unknown;
do not silently substitute an earlier reading.

### Add a new operation later

Actions are not implemented. Adding a field does not create a setter or action.
For a real new operation, implement these parts together:

1. **QLog:** implement the handler in its owning thread and add the IPC method in
   `mcpbridge/RuntimeServer.cpp`, including parameter validation and a defined result.
   The current schema/context methods accept only the `sources` parameter;
   an action needs its own explicitly validated parameters.
2. **MCP client:** add a typed method and result model in `runtime.py`. Extend
   `_request()` to pass the action's arguments using `_exchange()`'s existing
   `params` support, preserving the handshake, request IDs, limits and error
   handling. Source-selection errors currently have dedicated handling; define
   appropriate errors for the new operation. Do not put action results into
   `LiveContext`.
3. **MCP tool:** register the operation in `server.py`, with typed parameters and
   English descriptions of inputs, result and effects. Use annotations matching
   the operation; a mutating action must not reuse `READ_ONLY_TOOL`.
4. **Contract:** add socket-peer tests through `fastmcp.Client`, update applicable
   capabilities and `docs/tools.md`, and extend `docs/architecture.md` if the data
   flow changes. A timeout does not prove an action was not executed; avoid
   automatic retries of non-idempotent operations.

## Verify the extension

Add focused cases in `tests/RuntimeContextTest/tst_runtimecontext.cpp` for the
new schema, value conversion, and unavailable state. Include new provider
sources/headers in `RuntimeContextTest.pro` if those tests instantiate it.
Reuse the existing worker, timeout, and destruction tests for the shared path.

In QLog-MCP, reuse `tests/test_runtime.py::local_peer` and its `responses` fixture
to supply the new field's schema and values. Call both public tools and verify
that metadata, units, values and issues reach the client. The existing
`test_live_context_and_schema_without_database` covers unknown field names and
fresh reads; metadata validation and partial-failure cases cover the generic
contract. Add a specific case when introducing new conversion or operation
behavior, rather than duplicating tests for every ordinary scalar field.
