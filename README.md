# WubuEdit — A SLERM Ground-Up C11 Video Editor

[![License](https://img.shields.io/badge/License-Waefrebeorn%20Umbrella%20v3.0-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/Language-C11-orange.svg)](https://en.cppreference.com/w/c)
[![FFmpeg](https://img.shields.io/badge/FFmpeg-system%20lib-003366.svg)](https://ffmpeg.org/)

WubuEdit is a from-scratch (SLERM) C11 video editor. It uses FFmpeg/libav as a
**system library** (linked like `-lz`) for codec/muxer plumbing — never vendored
source. The timeline model, project format, effects pipeline, and CLI are all
written byte-for-byte by hand.

> **~WuBu~ AGI** — one system, many repositories. This is the **SLERM ground-up C11 video editor**. The Body is [WuBuOS](https://github.com/waefrebeorn/WuBuOS), the Brain wubuwizard, the Compiler WuBuNOS.

## Philosophy

See [`docs/PHILOSOPHY.md`](docs/PHILOSOPHY.md) for the WuBuOffice / WuBuPad /
WuBuEdit manifesto, and [`docs/SLERM_ARCHITECTURE.md`](docs/SLERM_ARCHITECTURE.md)
for the full architecture and research synthesis.

- **Native C11 core** — zero JS/web runtime, zero Electron
- **FFmpeg/libav as system library** — like linking `-lz`, not copying source
- **From-scratch timeline model** — our own `.wed` project file format
- **Opaque structs** — users see `wedit_timeline_t*`, never internal layout
- **Native-only build guard** — CMake hard-fails if `ELECTRON` env var is set

## Build

```bash
cd wubuedit
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## CLI

```bash
# Show timeline info
./wubuedit info

# Render a timeline to video (generates FFmpeg command)
./wubuedit render output.mp4
```

## Architecture

```
wubuedit/
├── CMakeLists.txt         — native-only guard, modular static libs
├── LICENSE                — Waefrebeorn Umbrella License v3.0
├── docs/
│   ├── PHILOSOPHY.md      — WuBuOffice/WuBuPad/WuBuEdit manifesto
│   └── SLERM_ARCHITECTURE.md — architecture + 50+ research sources
└── src/
    ├── wedit_timeline.h/c — PURE C11 timeline model (zero FFmpeg deps)
    ├── wedit_effects.h/c   — PURE C11 effects pipeline (color grade, LUT3D, drawtext, overlay)
    ├── wedit_project.h/c   — PURE C11 .wed JSON serializer (save/load projects)
    ├── wedit_demux.h/c     — libavformat wrapper (open/demux/read)
    ├── wedit_decode.h/c    — libavcodec wrapper (decode packets → frames)
    ├── wedit_encode.h/c    — libavcodec wrapper (encode frames → packets)
    ├── wedit_mux.h/c       — libavformat wrapper (write containers)
    ├── wedit_audio.h/c     — libswresample wrapper (audio resampling)
    ├── wedit_filter_graph.h/c — libavfilter wrapper (AVFilterGraph)
    ├── wedit_internal.h    — internal struct definitions (not public API)
    ├── wedit_main.c        — CLI entry point
    └── tests/test_timeline.c — 13 unit tests
```

### Modules

| Module | Pure C11 | FFmpeg Dep | Lines | Description |
|--------|----------|------------|-------|-------------|
| `wedit_timeline` | ✅ | No | 811 | Timeline model: tracks, clips, transitions, render command builder |
| `wedit_effects` | ✅ | No | 259 | Color grading, LUT3D, drawtext, overlay, audio ducking, deinterlace |
| `wedit_project` | ✅ | No | 274 | `.wed` JSON project file serializer/parser |
| `wedit_media` | ❌ | Yes | ~1100 | demux/decode/encode/mux/audio/filter-graph (libav wrappers) |

## Testing

```bash
cd build
ctest --output-on-failure
```

## License

All Waefrebeorn work is covered by the **Waefrebeorn Umbrella License v3.0**.
See [`LICENSE`](LICENSE) for the full text.

- **Populace** (individuals, non-profits, governments, small businesses): Full
  freedom to use, modify, distribute, sell, and integrate — no restrictions.
- **Corporations**: Must provide attribution and comply with Corporate Obligations
  (§4) at scale.
- **Prohibited activities**: Surveillance, censorship, autonomous weapons,
  fraud, discrimination — Waefrebeorn disassociates from these uses.

SPDX-Identifier: Waefrebeorn-Umbrella-3.0
