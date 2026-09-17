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

Requires CMake >= 3.25, a C++20 compiler, and network access on first configure (SDL3 is fetched automatically).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/mandelbrot_emerger
```

Produces a statically linked executable with no runtime DLL dependencies.

## Implementation notes

- Iteration runs in lockstep batches sized to the per-frame time budget (always at least one pass per frame), so iteration speed scales with available CPU instead of vsync; escape brightness uses the same monotonic frame counter, keeping fade ratios consistent at any speed. Colors are computed once per frame against the post-batch counter, which is bit-identical to rendering every individual pass.
- Iteration math uses IEEE doubles, exactly like JavaScript numbers, so point trajectories are bit-for-bit identical to the original.
- The simulation writes straight into the locked streaming texture, so a frame never pays for an intermediate pixel copy, and it stores only each point's current `z` and its escape frame: the seed `c` comes from per-row and per-column tables instead of a second full-frame array, which halves both the memory footprint and the per-pass memory traffic.
- Screenshots are written by a dependency-free PNG encoder (stored deflate blocks).
- Autopilot probes 256 random plane points per hop, iterates each up to 512 times, and centers the next zoom on the slowest escaper - a proxy for filament proximity - so it endlessly follows branch structure. When the view span approaches double-precision limits it restarts from full view, making generation truly infinite.
- Zooms glide instead of cutting: dives present an eased crop of the pre-zoom frame while the field silently resolves toward the destination, crossfading into fresh detail on arrival; zoom-outs, offset rectangles and any target the current frame cannot contain cross through black instead. Exactly one reframe happens per zoom - before a dive, or at the black crossing of a cross-fade - so no simulation state is reset while imagery is visible.

## License

Port of MIT-licensed work by drasimov; this port follows the same license intent.
