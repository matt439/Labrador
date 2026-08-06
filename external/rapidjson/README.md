# RapidJSON (vendored)

Header-only JSON library from https://github.com/Tencent/rapidjson

Vendored from `master` on 2026-08-06 (reports version 1.1.0). Only `include/`
and `license.txt` are copied; the upstream tests, docs and build files are not
needed.

This used to be pulled from a machine-local `C:\VSIncludes` folder via the
`IncludePath` property in `ArtAttack.vcxproj`, which meant the project only
built on one machine. It is now checked in and referenced with a path relative
to the project (`$(ProjectDir)..\external\rapidjson\include`).

To update: replace `include/rapidjson/` with a newer copy from upstream.
