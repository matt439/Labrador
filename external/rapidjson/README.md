# RapidJSON (vendored)

Header-only JSON library from https://github.com/Tencent/rapidjson

`include/rapidjson/` is upstream commit
`24b5e7a8b27f42fa16b96fc70aade9106cf7102f` (2025-02-05, "Fix out of bounds
read with kParseValidateEncodingFlag"), taken from `master` on 2026-08-06: its
git tree is identical to that commit's `include/rapidjson`. `rapidjson.h`
reports version 1.1.0, the last tagged release, which predates the commit.
`license.txt` is upstream's. Only those two are copied; the upstream tests,
docs and build files are not.

The engine reads it through `engine/CMakeLists.txt`, which adds
`external/rapidjson/include` to `LabradorEngine` as a `SYSTEM PRIVATE` include
directory; `bench/CMakeLists.txt` adds the same directory to
`LineSweeperFrameBench` and `D3D12DescriptorCountProbe`.

To update: replace `include/rapidjson/` with the same folder at a chosen
upstream commit, change the commit named above, and follow
[docs/dependency-updates.md](../../docs/dependency-updates.md).
