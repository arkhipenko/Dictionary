# Dictionary - Unit Tests

Native (host-side) unit tests for the Dictionary library, using
[Google Test](https://github.com/google/googletest). This mirrors the
TaskScheduler test harness: a mock `Arduino.h` in this directory lets the
library be compiled and run off-device, so logic can be validated on every
push without hardware. On-device compilation of the `examples/` is handled
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
| `Arduino.h` | Host shim: `String` (std::string-backed), `Print`/`Stream`, timing stubs |
| `test-dictionary-basic.cpp` | Core CRUD, positional access, operators, sizes, scale |
| `test-dictionary-json.cpp` | `json()` / `jload()` / `jsize()` / `esize()`, escaping, comments, CRLF, errors |
| `test-dictionary-delete.cpp` | `remove()` cases (leaf / one-child / two-child), bulk-delete idiom, `destroy()` |
| `test-dictionary-oom.cpp` | Out-of-memory safety via `malloc` fault injection (`--wrap=malloc`) |
| `test-dictionary-compress.cpp` | SHOCO / SMAZ compression round-trips (built twice) |
| `CMakeLists.txt` | Defines every suite/target, including config variants |

Config variants reuse `test-dictionary-basic.cpp` recompiled under different
`-D` defines (`dict_crc16`, `dict_crc64`, `dict_packed`, `dict_longlen`).

## Test plan

### Implemented

- **Core CRUD** - insert/search/update-in-place, existence operator, missing-key
  behavior, value grow/shrink (buffer reuse vs realloc).
- **Positional access** - `d(i)` / `d[i]` / `key(i)` / `value(i)` in insertion
  order; out-of-bounds returns empty.
- **Validation** - zero-length key rejected, over-`_DICT_KEYLEN` rejected,
  exact max-length key/value accepted, prefix-collision keys stay distinct.
- **Operators** - `==`, `!=`, assignment (`=`), `merge()`.
- **Sizes** - `size()`, `jsize()` (>= actual JSON length), `esize()` exact.
- **JSON** - deterministic emit order; `jload()` from `String` and from a
  `Stream`; partial load (`aNum`); unquoted keys/values; `#` comments; Windows
  CRLF; empty object; `"`/`\` escaping with `json()` -> `jload()` round-trip;
  newline-in-quote error (`DICTIONARY_QUOTE`).
- **Delete** - leaf, root-leaf, and two-child (in-order successor promotion);
  remove-non-existent no-op; bulk-delete idiom; remove-half integrity;
  `destroy()` + reuse; delete-then-reinsert; interleaved insert/remove churn.
- **Out-of-memory** - `malloc` fault injection proves insert survives failure at
  every allocation point (no crash/corruption), a failed insert leaves existing
  entries intact, and two-child delete is atomic on allocation failure. Also run
  under ASan to catch invalid free / use-after-free.
- **Configuration matrix** - default (CRC32), CRC16, CRC64, packed structures,
  and wide length-counter types (`_DICT_KEYLEN=300`, `_DICT_VALLEN=1000`).
- **Compression** - SHOCO and SMAZ round-trips: search, update, delete, `json()`
  decompression, and bulk round-trip.
- **Sanitizers** - the entire suite runs under ASan + UBSan in CI.

### Planned / future

- **Differential (oracle) testing** - drive random insert/remove/update against a
  `std::map` reference and assert equivalence after each op.
- **Full `jload` error-code coverage** - one case each for `DICTIONARY_COMMA`,
  `COLON`, `QUOTE`, `BCKSL`, `FMT`, `EOF`; malformed/truncated inputs.
- **`_DICT_ASCII_ONLY`** - non-ASCII bytes ignored on load.
- **Boundary lengths** - keys/values at exactly `_DICT_KEYLEN` / `+1`, and the
  length-type width transitions (uint8 -> uint16 at 255).
- **jload fuzzing** - random byte streams must never crash (return codes only).
- **Compression edge cases** - incompressible/expanding inputs, `DICTIONARY_OOB`.
- **Benchmark regression** (optional) - track lookup/insert timings.
- **On-device** - PSRAM allocation path and real-core memory limits (hardware
  only; not reproducible on host).
