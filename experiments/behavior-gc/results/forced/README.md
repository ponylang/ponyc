# Forced GC at one behavior boundary

The corrected private experiment was reproduced on Linux x86-64 WSL from a source-only copy at ponyc commit 3276ae583. The compiler published a fieldless actor receiver and a local reference to a mutable Cell containing one U64. The native probe created a temporary through a Pony helper and waited for that helper to return before the selected collection.

| Configuration | GC enabled | Baseline | Evidence |
| --- | ---: | ---: | --- |
| Native debug runtime, debug and optimized Pony | 2 | 2 | [Fresh-copy log](logs/verified-fresh.log) |
| Merged release-style runtime bitcode, full and ThinLTO, debug and optimized Pony, one and two schedulers | 8 | 8 | [Merged-runtime log](logs/bitcode.log) |
| Native ASan/UBSan runtime, debug and optimized Pony, one scheduler | 2 | 2 | [Sanitizer log](logs/asan-fixture.log) |

Each enabled trace contains one complete collection inside Probe.measure. The subsequent allocation reused the helper temporary's exact 32-byte slot. Each baseline contains zero such collections and used a different slot. The live local retained its identity and read 137 after the replacement read independently as 211. Leak detection was disabled for the sanitizer build.

The four original review findings and five follow-up findings were addressed. The corrected controls reject mismatched payloads, contradictory trace expectations, malformed trace events, incomplete recordings and missing observation categories. Guard checks exercised exact startup and stop boundaries, termination of nested process groups, and SIGTERM, SIGINT and SIGHUP. [Independent review](final-review.md) · [Calibration results](logs/final-counterfactuals.log)

The ordinary checkout passed all 20 ci-core tests and seven tools suites. The sole failure in the run of all 28 CTests was examples. The multipart-upload example calls a partial method without handling the error. Its call and the library method were unchanged from the research branch's base. An older release compiler reproduced the same front-end error. A separate tree containing only the example correction compiled all 70 example packages through the project's own CMake script. The checkout's optimized full-program and stdlib reruns both passed. [Stock regression](logs/tests.log) · [Optimized tests](logs/optimized.log) · [Example correction](multipart-upload.patch) · [All examples in the corrected copy](logs/all-examples-candidate.log)

The experiment validates forced collection at this site. Ordinary threshold-driven activation in a normal allocating behavior, broader root and capability coverage, and measured real compiler memory relief are separate milestones.
