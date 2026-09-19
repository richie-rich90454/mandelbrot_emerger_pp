# Mandelbrot Emerger (C++20 / SDL3)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![SDL3](https://img.shields.io/badge/SDL3-latest-green)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Web-green)
![License](https://img.shields.io/badge/license-MIT-green)

A faithful native port of [drasimov/mandelbrot-emerger](https://github.com/drasimov/mandelbrot-emerger) (p5.js) as an SDL3 desktop application.

Most Mandelbrot viewers use a fixed `MAX_ITERATIONS` cap and lose integrity when zooming. This program has none: every pixel's point iterates `z = z^2 + c` once per pass, in lockstep, forever - you watch the set take shape in real time, and resolution grows for as long as you leave it running. There is no precision floor either: viewport coordinates are kept in arbitrary-precision fixed point and pixels are integrated as perturbations of one high-precision reference orbit, so the camera can keep falling inward indefinitely.

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
emcmake cmake -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/web
python -m http.server 8000 -d build-web    # then open http://localhost:8000/
```

Windows - cmd (with the emsdk environment active):
```bat
emcmake cmake -B build\web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build\web
python -m http.server 8000 -d build-web    :: then open http://localhost:8000/
```

Windows - PowerShell:
```powershell
emcmake cmake -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/web
python -m http.server 8000 -d build-web    # then open http://localhost:8000/
```

If the `emsdk` tools are not yet on `PATH`, run `emsdk install latest`, `emsdk activate latest`, then `emsdk_env.bat` (cmd) or `.\emsdk_env.ps1` (PowerShell) from the emsdk folder first.

The build emits two WebAssembly modules and a hand-written page loader. `threaded.js` / `threaded.wasm` use pthreads (one worker per logical core) and therefore need a cross-origin-isolated page; `fallback.js` / `fallback.wasm` are single-threaded and need nothing but WebAssembly. `index.html` boots the threaded module whenever the page is already cross-origin isolated, lets the bundled `coi-serviceworker.js` inject COOP/COEP and reload once on hosts that cannot set headers (localhost and HTTPS static hosts), and otherwise boots the single-threaded module. Plain-HTTP LAN addresses, `file://`, private mode and browsers without service workers all fall back automatically.

The CMake build tree lives in `build/web` and the artifacts are written straight into `build-web/`, so that directory is the complete static dist - `index.html`, `threaded.js`, `threaded.wasm`, `fallback.js`, `fallback.wasm` and `coi-serviceworker.js`, nothing else. Point any static file server at it (static-web-server, Caddy, nginx, GitHub Pages, ...) or zip it and drop it on a host; no headers, rewrites or MIME configuration are needed. The loader resolves its assets against its own directory, so the dist also works under any mount prefix, with or without a trailing slash in the address.

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
| Web (WASM) | Emscripten 4+ | SDL3 port, pthreads + single-thread fallback | none (any static host, no config) | `build-web/` static dist |

## Implementation notes

- Every frame advances each point by exactly one iteration - like the original - on both the desktop and the browser, so the fade, the zoom glides and the autopilot cadence are identical everywhere; brightness comes from each point's escape age, which is independent of how fast a machine iterates.
- The simulation buffer is supersampled as far as a core-count-scaled pixel budget allows, so small displays get extra samples, a 2.5K screen runs 2x, and very large displays step down instead of stalling; the browser scales against its real backing-store pixels, so 4K and high-density canvases never over-render.
- Row threads are pooled for the process lifetime instead of being spawned per pass, and the desktop prefers Direct3D 12, which keeps the streaming upload inside the 60fps budget (Direct3D 11 allocates a staging texture on every lock and misses it).
- Pixels are written in the active scheme's own palette - the default is a bright amber-to-white ramp, `C` cycles through the others - with no renderer-side tinting, so the rendered frame, the browser and the saved PNG are byte-identical. Brightness falls off monotonically with the escape count relative to the passes since the last reframe, so the slowest escapers are always the brightest and every filament reads as a bright ridge against the dimmer field. The per-count value is a lookup table extended one entry per pass, and the palette is a precomputed 256-entry ramp, so a pixel costs a load and a multiply.
- The browser build creates a high-density canvas, so the canvas backing store runs at the display's native pixel count instead of being upscaled by the browser, and pointer coordinates are converted into renderer pixels before hit-testing.
- Zooming has no precision floor. The viewport lives in arbitrary-precision binary fixed point whose limb count grows with depth (about 96 bits of margin below the current span), the reference orbit - the view center, or a probe-chosen point near it - is iterated at that precision, and every pixel advances as a perturbation of it: `z = Z + w` with `w` in doubles, `w' = 2Zw + w^2 + dc`, and the magnitude test evaluated as `|Z|^2 - 4 + 2Z·w + |w|^2` so it never has to form the cancelling sum. When a pixel orbit passes near the critical point the delta is re-anchored to the orbit start (Zhuoran rebasing), which keeps deep orbits glitch-free. At shallow depths the arithmetic reduces to ordinary doubles, exactly like JavaScript numbers.
- The simulation writes straight into the locked streaming texture, so a frame never pays for an intermediate pixel copy, and it stores only each point's current perturbation delta, reference index and escape age: pixel coordinates come from per-row and per-column offset tables instead of a second full-frame array, which keeps the memory footprint close to the original.
- The pre-zoom frame an animation flies over is copied on the GPU into a render-target texture before the next iteration overwrites the streaming texture, because locking a streaming texture on the Direct3D backends hands back undefined staging memory.
- Screenshots are written by a dependency-free PNG encoder (stored deflate blocks).
- Autopilot probes 256 random plane points per hop, all evaluated through the high-precision reference orbit, and centers the next zoom on the slowest escaper inside the iteration budget - a proxy for filament proximity - so it endlessly follows branch structure at any depth. A full-horizon survivor (or, when none exists, the slowest escaper itself) becomes the next reference orbit, and the wait between hops is capped just below that reference point's own lifetime, so the perturbation stays valid while the camera keeps falling inward. A hop only starts once the current view has had enough passes to show its structure, and if a probe finds no slow escaper at all the camera pulls back, bounded by the full view.
- Zooms glide instead of cutting: every target the captured frame contains - magnifying zooms and equal-span pans alike - presents an eased crop of the pre-zoom frame while the field silently resolves toward the destination, crossfading into fresh detail on arrival; only targets the frame cannot cover (zoom-outs, offset rectangles) cross through black instead. Exactly one reframe happens per zoom - before a dive, or at the black crossing of a cross-fade - so no simulation state is reset while imagery is visible, and autopilot windows are slid back inside the view so its zooms always glide.

## License
[License](LICENSE) \
Port of MIT-licensed work by drasimov; this port follows the same license intent.
