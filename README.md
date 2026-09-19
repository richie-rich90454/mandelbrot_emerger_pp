# Mandelbrot Emerger (C++20 / SDL3)

A faithful native port of [drasimov/mandelbrot-emerger](https://github.com/drasimov/mandelbrot-emerger) (p5.js) as an SDL3 desktop application.

Most Mandelbrot viewers use a fixed `MAX_ITERATIONS` cap and lose integrity when zooming. This program has none: every pixel's point iterates `z = z^2 + c` once per pass, in lockstep, forever - you watch the set take shape in real time, and resolution grows for as long as you leave it running.

## Controls

| Input | Action |
|---|---|
| Mouse click x2 (corners) | Zoom into rectangle (disengages autopilot) |
| `A` | Toggle autopilot: probes the current view for filament points and zooms into them every few seconds, cycling forever |
| `C` | Cycle color scheme (grayscale / thermal / alpha / rainbow / fire / ice) |
| `Enter` | Save screenshot to `mandelbrot_<timestamp>.png` |
| `F11` | Toggle fullscreen / windowed |
| `Esc` | Quit |

## Building

Requires CMake >= 3.25, a C++20 compiler (GCC 13+, Clang 16+, AppleClang 15+, or MSVC 19.36+), and network access on first configure (SDL3 is fetched automatically via `FetchContent`).

The resulting executable is statically linked against SDL3 and the C++ runtime - no third-party DLLs to ship. On Windows that means `libstdc++-6.dll` / `libgcc_s_seh-1.dll` / `libwinpthread-1.dll` are **not** needed at runtime; the binary imports only OS DLLs.

### Windows - MSYS2 / UCRT64 (recommended)

Produces a fully static, dependency-free `.exe`.

```sh
# from an "MSYS2 UCRT64" shell:
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja git
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/mandelbrot_emerger.exe
```

From a plain PowerShell, same commands work **if** `C:\msys64\ucrt64\bin` precedes any other `libwinpthread-1.dll` provider on `PATH`:

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\mandelbrot_emerger.exe
```

> **PATH gotcha.** `cc1.exe` (invoked internally by `gcc.exe`) resolves DLLs like `libwinpthread-1.dll` and `libzstd.dll` via the process `PATH`, not relative to `gcc.exe`. If another toolchain's `bin` (PostgreSQL, LLVM, a different MinGW, …) precedes `C:\msys64\ucrt64\bin` on `PATH`, `cc1` loads an incompatible DLL and aborts with `STATUS_ENTRYPOINT_NOT_FOUND`. Make sure `C:\msys64\ucrt64\bin` is near the front of the Machine or User `PATH`, then open a fresh terminal. Verify with:
>
> ```powershell
> $env:PATH -split ';' | Where-Object { $_ -match 'PostgreSQL|msys64' }
> ```
>
> `ucrt64\bin` should come before `PostgreSQL\…\bin`.

### Windows - MSVC (Visual Studio 2022)

From a *Developer PowerShell for VS 2022* (or after running `vcvars64.bat`):

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\mandelbrot_emerger.exe
```

`CMakeLists.txt` sets `CMAKE_MSVC_RUNTIME_LIBRARY` to `MultiThreaded$<$<CONFIG:Debug>:Debug>`, so the produced `.exe` statically links the CRT - no `vcruntime140.dll` / `msvcp140.dll` deployment needed.

To build in the IDE instead, open the generated `build\mandelbrot_emerger.sln`.

### Linux

```sh
sudo apt install cmake ninja-build g++ git   # Debian / Ubuntu
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/mandelbrot_emerger
```

The default build is static against SDL3 but dynamically links `libc`. For a **fully static** ELF with no dynamic dependencies:

```sh
sudo apt install musl-tools
CC=musl-gcc CXX=musl-g++ cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
# verify: ldd build/mandelbrot_emerger  → "not a dynamic executable"
```

### macOS

```sh
brew install cmake ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/mandelbrot_emerger
```

The `-static -static-libgcc -static-libstdc++` flags in `CMakeLists.txt` are guarded to apply on GCC/MinGW only, since AppleClang does not support them. macOS builds dynamically link `libSystem`, as every macOS binary does.

### Web (WebAssembly)

Requires the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) (emsdk 4.0.15+). SDL3 comes from the emsdk port, so nothing is fetched over the network during configure.

Linux / macOS:
```sh
emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python -m http.server 8000 -d build-web    # then open http://localhost:8000/
```

Windows - cmd (with the emsdk environment active):
```bat
emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python -m http.server 8000 -d build-web    :: then open http://localhost:8000/
```

Windows - PowerShell:
```powershell
emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python -m http.server 8000 -d build-web    # then open http://localhost:8000/
```

If the `emsdk` tools are not yet on `PATH`, run `emsdk install latest`, `emsdk activate latest`, then `emsdk_env.bat` (cmd) or `.\emsdk_env.ps1` (PowerShell) from the emsdk folder first.

The build enables pthreads (one worker per logical core) and emits `index.html`, `index.js` and `index.wasm`. Threads require a cross-origin-isolated page, so `coi-serviceworker.js` is copied next to the artifacts and injects the COOP/COEP headers on hosts that cannot set them (GitHub Pages, for example) with a single automatic reload. Hosts that do serve those headers (or serve over localhost) do not need it, but shipping it anyway is harmless. A plain-HTTP LAN address is not a secure context, so the service worker cannot register there - use localhost or HTTPS.

### Manual / any host

If Ninja isn't installed, drop `-G Ninja` and CMake will pick a default generator for the platform:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Portability matrix

| Target | Compiler | Link mode | Runtime deps | Ships as |
|---|---|---|---|---|
| Windows | MinGW-w64 (UCRT64) | `-static -static-libgcc -static-libstdc++` | none | single `.exe` |
| Windows | MSVC 19.36+ | static SDL3 + static CRT (`/MT`) | none | single `.exe` |
| Linux (glibc) | GCC / Clang | static SDL3, dynamic libc | glibc >= build host | single ELF |
| Linux (musl) | `musl-gcc` | fully static | none | single ELF |
| macOS | AppleClang | static SDL3, dynamic libSystem | OS-provided | single Mach-O |
| Web (WASM) | Emscripten 4+ | SDL3 port, pthreads | none (static HTTP host) | `index.html` + `.js` + `.wasm` |

## Implementation notes

- Every frame advances each point by exactly one iteration - like the original - on both the desktop and the browser, so the fade, the zoom glides and the autopilot cadence are identical everywhere; escape brightness is computed against the same monotonic frame counter.
- Row threads are pooled for the process lifetime instead of being spawned per pass, and the desktop prefers the OpenGL backend, whose streaming-texture upload avoids the per-frame staging allocation that made Direct3D 11 miss the 60fps budget.
- Iteration math uses IEEE doubles, exactly like JavaScript numbers, so point trajectories are bit-for-bit identical to the original.
- The simulation writes straight into the locked streaming texture, so a frame never pays for an intermediate pixel copy, and it stores only each point's current `z` and its escape frame: the seed `c` comes from per-row and per-column tables instead of a second full-frame array, which halves both the memory footprint and the per-pass memory traffic.
- Screenshots are written by a dependency-free PNG encoder (stored deflate blocks).
- Autopilot probes 256 random plane points per hop, iterates each up to 512 times, and centers the next zoom on the slowest escaper - a proxy for filament proximity - so it endlessly follows branch structure. When the view span approaches double-precision limits it restarts from full view, making generation truly infinite.
- Zooms glide instead of cutting: dives present an eased crop of the pre-zoom frame while the field silently resolves toward the destination, crossfading into fresh detail on arrival; zoom-outs, offset rectangles and any target the current frame cannot contain cross through black instead. Exactly one reframe happens per zoom - before a dive, or at the black crossing of a cross-fade - so no simulation state is reset while imagery is visible.

## License
[License](LICENSE) \
Port of MIT-licensed work by drasimov; this port follows the same license intent.