# ASTRA

A native C++20 space demo: four procedural GPU scenes, cinematic transitions, an original stereo soundtrack, and optional retro presentation effects. Built for Windows with SDL3 GPU, Direct3D 12, HLSL, and Dear ImGui.

## Build and run

Requires Windows 10/11 x64, Visual Studio 2022 **Desktop development with C++**, CMake tools, and a Windows SDK containing DXC. The first build downloads pinned SDL3 and Dear ImGui releases from their official repositories and checks their SHA-256 hashes.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Run
```

The executable is `build/Release/Astra.exe`. Start it directly after building. Keep its `SDL3.dll`, `shaders`, and `config` alongside it. Create a portable ZIP with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Package
```

The package includes SDL3 and statically links the C++ runtime, so no separate Visual C++ runtime installer is needed. It requires a Direct3D 12-capable GPU supported by SDL3; it does not require a discrete GPU or ray tracing. Other OS backends are not compiled in this first release.

## Experience

- **Departure:** perspective starfield with trails, camera roll, and distant nebulae.
- **Interstellar:** lateral parallax, violet dust, and a crescent planet.
- **Solar approach:** rotating procedural surface, turbulent corona, and prominence arcs.
- **Galactic orbit:** spiral dust, a luminous core, and thousands of GPU-instanced stars.

The default 44-second timeline loops. `config/demo.cfg` contains scene IDs, durations, and crossfade lengths. Change the copy beside the executable for an immediate change on next launch; change the source copy to preserve the change through future builds. Invalid files produce a useful error rather than silently changing the show.

The original synthesized score, **Signals from home**, uses layered pads, sub-bass, arpeggios, gentle percussion, and stereo delay. It is generated once before playback. No downloads, sampled recordings, or external music assets are needed. Playback position drives the timeline while audio is available; a monotonic clock takes over without audio. Pause, seek, restart, and mute preserve synchronization. The music is a looping ambient composition, not an individually scored cue for every possible user-defined timeline.

## Controls

| Control | Action |
| --- | --- |
| Space | Pause / play |
| 1–4 or chapter labels | Jump to a chapter |
| Drag a chapter progress line | Scrub within the chapter |
| Left / Right | Previous / next chapter |
| R | Restart |
| M | Mute |
| Tab | Hide / show interface |
| V | Visual controls |
| F / F11 | Fullscreen |
| F12 | Screenshot beside the executable |
| Esc | Close settings, leave fullscreen, or quit |

The visual panel contains cinematic/CRT/retro presets, independent CRT, pixel, palette, and grain toggles, intensity controls, bloom, exposure, resolution quality, and audio volume. The interface remains sharp when the scene is pixelated. Preferences are saved in `config/preferences.cfg` beside the executable (portable writable folder recommended).

Balanced renders at 75% output resolution, Low at 50%, and High at 100%. Adaptive resolution can reduce this further when the measured frame rate drops; it never changes the selected artistic pixel size. High-cost effects and bloom run at reduced resolution. Scene pipelines are prepared at launch, and both sides of a transition are rendered only during the overlap.

## Development and verification

```powershell
# Deterministic capture; exits after five frames
build/Release/Astra.exe --no-audio --at 25 --capture solar.bmp

# Eight captures covering all scenes and transitions
build/Release/Astra.exe --no-audio --gallery captures

# Presets, hidden interface, fixed quality, or an open settings panel
build/Release/Astra.exe --no-audio --at 37 --preset crt --settings --capture crt.bmp
build/Release/Astra.exe --no-audio --at 25 --no-ui --quality 2 --capture sun.bmp

# Exercise event handling, scene changes, resizing, pause, and mute
build/Release/Astra.exe --frames 240 --exercise-controls

# Export the original score to a 48 kHz stereo PCM WAV
build/Release/Astra.exe --export-audio signals-from-home.wav
```

Other switches: `--width`, `--height`, `--config path`, `--debug-gpu`. Startup failures, audio availability, frame timings, and screenshot paths are written to `astra.log` beside the executable. `--debug-gpu` requires the optional Windows Graphics Tools for D3D12 validation.

CPU tests cover timeline boundaries, looping, transition continuity, malformed configuration, and soundtrack samples. GPU capture and control-exercise modes complement them. Reported frame timings include presentation and are not GPU timestamp measurements; the 60 FPS laptop target needs measurement on the intended hardware.

Local validation on an AMD Radeon 880M integrated GPU completed 6,400 frames with audio and a full timeline loop, averaging 8.35 ms per frame at Balanced quality (1440 × 900 output). A separate solar-scene run at full 1920 × 1080 rendering averaged 8.64 ms per frame. Automated control checks and the D3D12 debug run completed without reported errors. These are measurements on this machine, not a guarantee for every laptop.

## Adding a scene

1. Add scene metadata to `sceneRegistry()` in `src/core.cpp`.
2. Add a matching HLSL background shader under `shaders/` and a `compile_shader` entry in CMake.
3. In `Renderer::initialize`, configure its particles or extend `SceneRenderer` with additional passes/resources. The shared uniform block provides resolution, local time, progress, scene ID, and quality.
4. Insert its ID and timing in `config/demo.cfg`.

Every scene renders to an HDR texture. The timeline supplies continuous local time across transitions (including a negative preview interval); a shared compositor handles crossfades, bloom, tone mapping, and visual styles. The application shell, audio, and presentation controls do not need to change. More complex scenes can extend the scene renderer without introducing dynamic plugins or a full game engine.

## Dependencies and assets

- SDL 3.4.18: zlib license; downloaded into `external/`.
- Dear ImGui 1.93.0: MIT license; downloaded into `external/`.
- System Segoe UI and Consolas fonts are loaded from Windows; fonts are not redistributed. ImGui's built-in font is the fallback.
- All scene visuals and the soundtrack are generated by the application.

The portable package includes dependency license notices. The source does not vendor the downloaded libraries or generated build products.
