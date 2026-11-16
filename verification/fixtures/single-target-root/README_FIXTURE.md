# Historical single-target root Makefile

The root `Makefile` was extended during multi-target integration (`phyb`, `rkos`,
`rkof`, `aopf`, `acib`) with delegated build and verify targets. The `phyb` build
flags and compiler/linker rules did not change.

The archived rebuild report (`verification/evidence/build-image-equivalence.json.gz`)
records the hash of the `Makefile` it was produced with. That unmodified file is kept
here, and `verification/input_manifest.json` maps the logical path `Makefile` to it
through `source_relocations`, so `tools/verify_archive.py` can still check the report's
inputs.
