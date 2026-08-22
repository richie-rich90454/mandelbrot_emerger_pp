# Mandelbrot Emerger (C++20 / SDL3)

A faithful native port of [drasimov/mandelbrot-emerger](https://github.com/drasimov/mandelbrot-emerger) (p5.js) as an SDL3 desktop application.

Most Mandelbrot viewers use a fixed `MAX_ITERATIONS` cap and lose integrity when zooming. This program has none: every pixel's point iterates `z = z^2 + c` once per frame, in lockstep, forever - you watch the set take shape in real time, and resolution grows for as long as you leave it running.

## Controls

| Input | Action |
|---|---|
| Mouse click x2 (corners) | Zoom into rectangle |
| `C` | Cycle color scheme (grayscale / thermal / alpha) |
| `Enter` | Save screenshot to `mandelbrot_<timestamp>.png` |
| `F11` | Toggle fullscreen / windowed |
| `Esc` | Quit |

## Building

Requires CMake >= 3.25, a C++20 compiler, and network access on first configure (SDL3 is fetched automatically).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/mandelbrot_emerger
```

Produces a statically linked executable with no runtime DLL dependencies.

## Implementation notes

- The escape-brightness fade (`escapeFrame/frameCount`) runs on a virtual 60 Hz clock matching p5.js' `frameRate(60)`, while iteration itself is uncapped and multithreaded across all cores - identical visuals, faster convergence.
- Iteration math uses IEEE doubles, exactly like JavaScript numbers, so point trajectories are bit-for-bit identical to the original.
- Screenshots are written by a dependency-free PNG encoder (stored deflate blocks).

## License

Port of MIT-licensed work by drasimov; this port follows the same license intent.
