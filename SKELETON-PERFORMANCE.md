# Skeleton performance changes

Changes to the runtime skeletons (`skeletons/`) that make conversion between UPER and
XER/JER cheaper. The compiler and the generated code are not affected. The encoded output
is unchanged.

The changes fall into four groups, plus tests.

## 1. Integer formatting without `snprintf`

- **`asn_internal.c` / `.h`**: new `asn__format_imax` and `asn__format_umax`, which write
  decimal digits backwards into a small caller buffer.
- **`NativeInteger_xer.c`, `NativeInteger_jer.c`, `NativeInteger_print.c`**: use the
  formatter in place of `snprintf("%ld")`.
- **`INTEGER.c`** (`INTEGER__dump`): uses the formatter for plain numbers, and writes
  enumerated names directly instead of through `asn__format_to_callback` (a `vsnprintf`).
- **`NativeEnumerated_xer.c`, `NativeEnumerated_jer.c`**: write `<name/>` and `"name"` with
  `ASN__CALLBACK3` instead of `vsnprintf`.
- **`BIT_STRING_jer.c`**: the `"length"` value uses the formatter.

## 2. NativeInteger UPER without a temporary `INTEGER_t`

- **`NativeInteger_uper.c`**, decode: a constrained whole number is read straight into the
  `long`. Previously it went through a heap-allocated `INTEGER_t` (two malloc/free pairs
  per integer) and was converted back.
- **`NativeInteger_uper.c`**, encode: a value inside its constrained range is written
  directly, without building an `INTEGER_t` first.
- Unconstrained, semi-constrained and extension values still use the old path. The
  extension bit is read once and the fallback is told not to look for it again.

## 3. Fewer output callback calls

- **`asn_internal.c` / `.h`**: new `asn__callback3`, which joins up to three short pieces
  in a stack buffer and delivers them in one call. `ASN__CALLBACK2` and `ASN__CALLBACK3`
  now use it, so `<`, name, `>` is one call instead of three in every XER/JER encoder.
- **`asn_internal.c` / `.h`**: new `asn__text_indent`; `ASN__TEXT_INDENT` emits the newline
  and the whole indent in one call instead of one call per level.

An output callback now receives larger pieces than before. The bytes and their order are
the same.

## 4. Arena allocator (only with `-DASN_ARENA`)

- **`asn_internal.h`**: with the flag, `MALLOC`/`CALLOC`/`REALLOC`/`FREEMEM` point to
  arena-aware functions; without it they are `malloc`/`free` as before.
- **`asn_internal.c`**: the allocator itself. It is a per-thread bump allocator that starts
  in a caller-supplied buffer and spills to heap chunks. Freeing an arena pointer is a
  no-op, and heap pointers are still freed normally.
- **`asn_application.h`**: the public API (`asn_arena_t`, `asn_arena_init`,
  `asn_arena_use`, `asn_arena_reset`) with usage rules in the comment.
- **`asn_application.c`**: `asn_encode_to_new_buffer` calls `malloc`/`realloc`/`free`
  directly, so the buffer it returns is always heap memory the application can `free`.

Usage:

```c
char buffer[65536];
asn_arena_t arena;
asn_arena_init(&arena, buffer, sizeof(buffer));
asn_arena_use(&arena);
/* decode, check constraints, encode */
asn_arena_use(0);
asn_arena_reset(&arena);
```

Rules:

- A structure allocated from an arena dies with `asn_arena_reset()`. `ASN_STRUCT_FREE()` is
  not needed for it. Calling it is harmless while the same arena is still in use by the
  calling thread, and is an error after that.
- Memory allocated before `asn_arena_use()` is freed as usual.
- An arena is used by one thread at a time.

## Tests (`tests/tests-skeletons/`)

- **`check-INTEGER.c`**: compares the formatter with `snprintf` for edge values (0, ±1,
  `LONG_MIN`, `INTMAX_MIN`, and so on).
- **`check-UPER-INTEGER.c`**: for every existing case, checks that NativeInteger produces
  the same bits as the INTEGER codec and reads them back. It covers in-range, extension,
  refused, unconstrained, semi-constrained and truncated input.
- **`check-arena.c`** (new): alignment, in-place and moving realloc, freeing the last
  block, overflow to the heap, oversized allocations, reset and reuse, and heap pointers
  mixed with arena pointers.
- **`Makefile.am`**: registers `check-arena` and its 32-bit variant.

## Measured effect

J2735 2024 `MessageFrame` messages (small BSM, PSM, SPAT, MAP), converted with
`convert_bytes` of `j2735-ffm-java`. CPU cycles per call (`perf stat`, user mode, clang 18
`-O3`, WSL2), relative to the skeletons before these changes:

| Direction | Groups 1-3 | Groups 1-3 + arena |
|---|---|---|
| UPER to JER | -32% to -40% | -47% to -53% |
| UPER to XER | -30% to -37% | -41% to -50% |
| JER to UPER | -9% to -10% | -15% to -18% |
| XER to UPER | -3% to -5% | -11% to -14% |

Heap allocations per message without the arena:

| Message | Decode, UPER input | Encode, UPER output |
|---|---|---|
| Small BSM | 47 to 11 | 22 to 4 |
| SPAT | 1968 to 1084 | 455 to 13 |
| MAP | 2710 to 1360 | 688 to 13 |

## Not done

- The XML and JSON tokenizers (`pxml_parse`, `pjson_parse`), which are where most of the
  remaining text-to-UPER time is.
- `NativeInteger_decode_xer` and `_jer` still decode through a temporary `INTEGER_t`.
- The arena has not been built for Windows/MinGW, and the 32-bit test variants were not
  run.
