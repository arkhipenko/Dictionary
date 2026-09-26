# Dictionary - Unit Tests

Native (host-side) unit tests for the Dictionary library, using
[Google Test](https://github.com/google/googletest). A mock `Arduino.h` in this
directory lets the library be compiled and run off-device, so logic is validated on
every push without hardware. On-device compilation of the `examples/` is handled
separately by `.github/workflows/main.yml`.

## Running locally

Requires `cmake`, a C++14 compiler, and Google Test (`libgtest-dev`).

```bash
cd tests
cmake -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Sanitized (AddressSanitizer + UndefinedBehaviorSanitizer) build:

```bash
cmake -B build-asan -DDICT_SANITIZE=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

## Layout

| File | Purpose |
|------|---------|
| `Arduino.h` | Host shim: `String` (std::string-backed), `Print`/`Stream`, `F()`, timing stubs |
| `TestStream.h` | In-memory `Stream` and `Print` for the tests |
| `test-dictionary-basic.cpp` | 3.x API: CRUD, positional access, operators, sizes, scale (compatibility) |
| `test-dictionary-json.cpp` | 3.x API: `json()` / `jload()`, escaping, comments, CRLF (compatibility) |
| `test-dictionary-delete.cpp` | 3.x API: `remove()`, bulk-delete idiom, `destroy()` (compatibility) |
| `test-dictionary-api.cpp` | 4.0 API: typed set/get, conversion, `peek`/`has`/`type`, positional order, `reserve`, merge, equality, move, sizes, JSON output |
| `test-dictionary-jload.cpp` | 4.0 parser: fixes for the 3.x defects, escapes, error codes, token cap, typing, streams |
| `test-dictionary-oracle.cpp` | Differential test: random set/update/remove/destroy against a `std::map` + insertion-order model |
| `test-dictionary-compress.cpp` | Value codec: table rules, round trips (including random bytes), packed never larger |
| `test-dictionary-oom.cpp` | Out-of-memory safety via fault injection into `malloc` and `operator new` |
| `test-dictionary-split.cpp` | Split-header mode: sees only `DictionaryDeclarations.h`, links `src/Dictionary.cpp`, calls every public method |
| `compile-deprecated.cpp`, `compile-new-api.cpp` | Compile-only checks for the deprecation warnings |
| `bench-example-host.cpp` | Builds `examples/Dict_Benchmark` against the shim and runs it; exit code = failed checks |
| `CMakeLists.txt` | Defines every target, including the configuration variants |

## Targets

| Target | Source | Build flags |
|---|---|---|
| `dict_basic`, `dict_json`, `dict_delete` | 3.x suites | `_DICT_NO_DEPRECATION_WARNINGS` |
| `dict_longlen` | basic | long limits (`_DICT_KEYLEN=300`, `_DICT_VALLEN=1000`) |
| `dict_legacy_flags` | basic | `_DICT_CRC=16`, `_DICT_PACK_STRUCTURES` (ignored with a warning) |
| `dict_api`, `_strict`, `_wide`, `_compress` | api | default, `_DICT_STRICT_GET`, `_DICT_WIDE_NUMBERS`, `_DICT_COMPRESS` |
| `dict_jload`, `_typed`, `_ascii`, `_bigbuf` | jload | default, `_DICT_TYPED_JSON`, `_DICT_ASCII_ONLY`, `_DICT_VALLEN=1000` (heap token buffer) |
| `dict_oracle`, `_noindex`, `_index1`, `_compress` | oracle | default, `_DICT_INDEX_MIN=0`, `_DICT_INDEX_MIN=1`, `_DICT_COMPRESS` |
| `dict_compress`, `dict_compress_legacy` | compress | `_DICT_COMPRESS`, `_DICT_COMPRESS_SMAZ` (3.x flag, mapped) |
| `dict_oom`, `dict_oom_compress` | oom | fault injection, default and `_DICT_COMPRESS` |
| `dict_split` | split | `_DICT_HEADER_AND_CPP` |
| `dict_bench_example`, `_compress`, `_noindex` | bench-example-host | the benchmark sketch: 10,000 pairs; 2,000 with `_DICT_COMPRESS`; 1,000 with `_DICT_INDEX_MIN=0` |
| `dict_deprecation_warns` | compile-deprecated | must fail with `-Werror=deprecated-declarations` |
| `dict_deprecation_silenced` | compile-deprecated | must compile with `_DICT_NO_DEPRECATION_WARNINGS` |
| `dict_new_api_no_warnings` | compile-new-api | must compile with `-Werror=deprecated-declarations` |

## Test plan

### Implemented

- **3.x compatibility** - the 3.x suites run unchanged against the deprecated wrappers.
- **Typed values** - every setter overload (all integer widths, float, double, bool,
  text, `String`, `F()`, null), integers beyond the numeric type stored as text,
  type changes on replace, invalid keys and values.
- **Conversion** - the full getter conversion table, and the strict variant.
- **Positional order** - preserved by `remove()`; `keyAt`/`typeAt`/`key`/`value`.
- **Hash index** - on, off and always on, checked by the differential test over
  random operations (3000 steps x 8 seeds, key spaces crossing the threshold).
- **JSON output** - typed values, escaping of control characters, exact `jsize()`,
  `json(Print&)`, `nan`/`inf` written as `null`.
- **JSON input** - empty values, trailing comments, last pair without a separator,
  standard and `\u` escapes (surrogate pairs), control-character round trip, token
  length cap, nested structures, every error code, partial loads, duplicates,
  streams, typed loading, ASCII filter, heap token buffer.
- **Codec** - table invariants, round trips of typical, incompressible and random
  input, packed only when smaller, keys never packed, merge and move of packed entries.
- **Out of memory** - insert, update, reserve, jload and merge fail cleanly at every
  allocation point; remove, destroy, move and construction never allocate; a failed
  index allocation falls back to scanning.
- **Split-header build** - every public method links from a declarations-only unit.
- **Deprecation** - wrappers warn, the flag silences them, the new API is warning-free.
- **Sanitizers** - the entire suite runs under ASan + UBSan (with leak detection) in CI.

### Planned / future

- **jload fuzzing** - random byte streams must never crash (return codes only).
- **Benchmark regression** (optional) - track insert/lookup/remove timings.
- **On-device** - PSRAM placement and real-core memory limits (hardware only).
