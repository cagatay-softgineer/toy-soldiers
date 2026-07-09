# AddressSanitizer (ASan) — Debug builds

**Roadmap:** v0.6 P2 #31

## Windows (MSVC)

1. Install **C++ AddressSanitizer** component in Visual Studio Installer  
   (Individual components → search “AddressSanitizer”).
2. Configure and build with the `windows-asan` preset:

```powershell
$env:Path = "C:\Program Files\CMake\bin;" + $env:Path
cd D:\path\to\toy-soldiers
cmake --preset windows-asan
cmake --build --preset windows-asan --parallel
```

3. Run from the repo root so data paths resolve:

```powershell
.\build-asan\bin\Debug\toy_soldiers.exe
# or
.\build-asan\bin\Debug\toy_sim.exe
```

### Notes

- ASan is **Debug-only** here; Release continuous CI stays clean/fast.
- Box3D is built as a subdirectory of this project — the same `/fsanitize=address` flags apply via the preset.
- If link fails with missing `clang_asan*`, the VS ASan component is not installed.
- Expect 2–3× slower runtime and higher memory use.

## Linux (optional)

```bash
cmake -S . -B build-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTOY_BOX3D_DIR=../box3d \
  -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build-asan --parallel
ASAN_OPTIONS=detect_leaks=1 ./build-asan/bin/toy_sim
```

## What we use ASan for

- Use-after-free / buffer overflow in UI string handling, net buffers, snapshot parse
- Catch regressions before tagging a release

Not a substitute for `scripts/verify.ps1` functional gates.
