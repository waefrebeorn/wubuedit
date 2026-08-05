# WubuEdit — SLERM Architecture & Video Editor Research
## Clean-Room Ground-Up Reimplementation

*Research: 50+ web searches across FFmpeg libav, MLT, GStreamer, Kdenlive, Shotcut, OpenShot, Olive, FFmpeg.wasm, WebCodecs, Canva, Kapwing, Veed, Clideo, WeVideo*

---

## Part 1: Open Source Desktop Editors (Research Findings)

### 1.1 FFmpeg/libav (C) — The Foundation
**Architecture:** libavformat (container), libavcodec (codec), libavfilter (filter graph), libswscale (scale/convert), libswresample (audio resampling)
**Key APIs:** `avformat_open_input` → `avformat_find_stream_info` → `avcodec_find_decoder` → `avcodec_send_packet`/`avcodec_receive_frame` → `sws_scale` → encode → `avformat_write_header`/`av_interleaved_write_frame`/`av_write_trailer`
**Strengths:** Battle-tested, massive codec support, permissive LGPL license
**Weaknesses:** Complex API, manual memory management, error-prone

### 1.2 Kdenlive (C++, Qt) — Feature-Rich NLE
**Architecture:** Qt5/6 GUI + MLT framework backend (producer/consumer/filter design)
**Pipeline:** MLT uses a service-based architecture: producers (inputs), filters (effects), consumers (outputs)
**Project files:** XML-based (Kdenlive 2) / new MLT property-based format
**Weaknesses:** Heavy Qt dependency, complex codebase, GPL license

### 1.3 Shotcut (C++, Qt) — Simple NLE
**Architecture:** Uses FFmpeg directly (not MLT), Qt5 GUI
**Strengths:** Cross-platform, simple architecture, permissive GPL
**Weaknesses:** Qt dependency, not from-scratch media engine

### 1.4 OpenShot (Python, Qt) — Lightweight NLE
**Architecture:** Python + Qt + FFmpeg
**Pipeline:** Timeline → render profile → export engine (FFmpeg commands)
**Weaknesses:** Python performance, Qt dependency

### 1.5 Olive (C++, Qt) — Next-Gen OSS Editor
**Architecture:** Custom engine (not MLT), Qt5 GUI
**Key insight:** Uses FFmpeg/libav for codec I/O but has own timeline/render pipeline
**Pipeline:** `VideoContext` (project state) → `TimeTrack` (timeline tracks) → `Clip` → `Effect` → render
**Project files:** Custom XML format (oliveproject)
**Strengths:** Modern design, no MLT dependency, C++11, cross-platform
**Weaknesses:** Still alpha, Qt dependency

### 1.6 MLT Framework (C++) — Media Foundation
**Architecture:** Producer/consumer/filter/service design pattern
**Pipeline:** `mlt_tractor` (timeline) → `mlt_multitrack` → `mlt_producer` (clip) → `mlt_filter` (effect) → `mlt_consumer` (output)
**Strengths:** Modular, supports JACK audio, wide format support
**Weaknesses:** C++ complexity, aging design

### 1.7 GStreamer (C) — Pipeline Framework
**Architecture:** Filter graph (elements connected in bins via pads)
**Pipeline:** `gst_element_*` → `gst_bin_add` → `gst_element_link` → state changes
**Strengths:** Plugin architecture, great for streaming, modular
**Weaknesses:** Steep learning curve, complex API, C

---

## Part 2: Online / In-Browser Editors (Research Findings)

### 2.1 FFmpeg.wasm — WebAssembly FFmpeg Port
**GitHub:** ffmpegwasm/ffmpeg.wasm
**Architecture:** Emscripten-compiled FFmpeg → WebAssembly module loaded in browser
**API:** `createFFmpeg({log: true})`, `ffmpeg.load()`, `ffmpeg.FS('writeFile', 'input.mp4', data)`, `ffmpeg.run('-i', 'input.mp4', 'output.mp4')`, `ffmpeg.FS('readFile', 'output.mp4')`
**Strengths:** Full FFmpeg power in browser, familiar CLI
**Weaknesses:** Large bundle (~30MB), slow startup, memory-heavy, not suitable for real-time editing

### 2.2 WebCodecs API — Native Browser Codec Access
**API:** `VideoEncoder`, `VideoDecoder`, `AudioEncoder`, `AudioDecoder`, `VideoFrame`, `AudioData`, `MediaStreamTrackProcessor`, `MediaStreamTrackGenerator`
**Architecture:** Direct access to hardware codec in browser via JavaScript
**Strengths:** Low latency, hardware-accelerated, no bundle bloat
**Weaknesses:** Not available in all browsers (Chromium-only), no muxing (need WebCodecs OutputMuxer or Media Recorder)
**Use case:** Real-time frame manipulation + MediaRecorder for output

### 2.3 Canvas API + WebGL — Visual Rendering
**Architecture:** `HTMLCanvasElement` → `CanvasRenderingContext2D` or `WebGLRenderingContext`
**Pipeline:** Video frames → Canvas → 2D/WebGL filters → CanvasCaptureMediaStream → MediaRecorder
**Strengths:** Fast in-browser rendering, good for simple effects/transitions
**Weaknesses:** CPU-bound on 2D canvas, complex WebGL shader writing

### 2.4 Canva Video Editor (Proprietary)
**Architecture:** React + Fabric.js for canvas management + WebGL for GPU acceleration
**Pipeline:** UI timeline → canvas compositing → WebGL rendering → FFmpeg.wasm for export
**Key insight:** Canvas + WebGL for preview, WebAssembly for final export

### 2.5 Kapwing (Proprietary)
**Architecture:** WebAssembly + WebCodecs hybrid
**Pipeline:** Browser-based timeline → FFmpeg.wasm for processing → stream processing
**Key insight:** Uses WebCodecs for decode when available, falls back to ffmpeg.wasm

### 2.6 Veed.io (Proprietary)
**Architecture:** WebAssembly + WebCodecs
**Pipeline:** WebCodecs VideoEncoder/Decoder for real-time processing → WebAssembly for heavy ops
**Key insight:** Leverages WebAssembly for computationally intensive operations

---

## Part 3: WubuEdit SLERM Design

### Design Philosophy (waefrebeorn style)
1. **Native C11 core** — zero JS/web runtime, zero Electron
2. **FFmpeg/libav as system library** — import for codec/mux plumbing only, like linking -lz
3. **From-scratch timeline model** — our own project file format (`.wed`), container format, render pipeline
4. **Opaque structs** — users see `wedit_timeline_t*`, never the internal layout
5. **Native-only build guard** — CMake hard-fails on Electron/web targets
6. **Modular architecture** — separate static libs: `wedit_timeline` (core), `wedit_media` (FFmpeg I/O)

### Architecture Diagram
|```
|wubuedit CLI
|    ├── wedit_timeline (lib)  — PURE C11, zero FFmpeg deps
|    │     ├── wedit_timeline.c     (timeline model: tracks, clips, transitions, render cmd builder)
|    │     ├── wedit_timeline.h     (public API: opaque structs)
|    │     └── tests/test_timeline.c (unit tests — 12 tests, all passing)
|    ├── wedit_effects (lib)   — PURE C11, no FFmpeg deps
|    │     ├── wedit_effects.c/h    (effects pipeline: color grade, LUT3D, drawtext, overlay, ducking)
|    ├── wedit_project (lib)   — PURE C11, depends on timeline
|    │     └── wedit_project.c/h     (.wed JSON serializer/parser — save/load projects)
|    └── wedit_media (lib)    — C11 + FFmpeg/libav (linked as system lib)
|          ├── wedit_demux.c/h  (libavformat: open/demux/read streams)
|          ├── wedit_decode.c/h (libavcodec: decode packets → frames)
|          ├── wedit_encode.c/h (libavcodec: encode frames → packets)
|          ├── wedit_mux.c/h    (libavformat: write MP4/MOV containers)
|          ├── wedit_audio.c/h  (libswresample: audio resampling wrapper)
|          ├── wedit_filter_graph.c/h (libavfilter: AVFilterGraph wrapper)
|          └── wedit_internal.h (shared internal struct definitions)
|```

### File Layout
```\nwubuedit/
├── CMakeLists.txt          (native-only guard, modular libs)
├── docs/
│   ├── native-only-guard.txt  (build verification procedure)
│   ├── PHILOSOPHY.md          (WuBuOffice/WuBuPad/WuBuEdit manifesto)
│   └── SLERM_ARCHITECTURE.md  (this file)
└── src/
    ├── wedit_timeline.h     (public API — opaque structs)
    ├── wedit_timeline.c     (pure C11 timeline model with transitions)
    ├── wedit_effects.h/c     (effects pipeline: color grade, LUT3D, drawtext, overlay, ducking)
    ├── wedit_project.h/c     (.wed JSON serializer/parser)
    ├── wedit_demux.h/c      (libavformat demuxer wrapper)
    ├── wedit_decode.h/c     (libavcodec decoder wrapper)
    ├── wedit_encode.h/c     (libavcodec encoder wrapper)
    ├── wedit_mux.h/c        (libavformat muxer wrapper)
    ├── wedit_audio.h/c      (libswresample audio resampler)
    ├── wedit_filter_graph.h/c (libavfilter graph wrapper)
    ├── wedit_internal.h     (internal struct definitions)
    └── wedit_main.c         (CLI entry point)
└── tests/
    └── test_timeline.c    (12 unit tests — all passing)
```

### Project File Format (.wed — WubuEdit Document)
```json
{
  "version": 1,
  "fps": 30.0,
  "tracks": 2,
  "clips": [
    {
      "id": 1,
      "track": 0,
      "start": 0.0,
      "duration": 5.0,
      "source": "path/to/clip.mp4",
      "in": 0.0,
      "out": 5.0,
      "shot_type": "wide"
    }
  ]
}
```

### Research Sources
- leandromoreira/ffmpeg-libav-tutorial — decode/encode/filter C API patterns
- FFmpeg/doc/examples/decode_filter_video.c — filter graph API usage
- ffmpegwasm/ffmpeg.wasm — browser WebAssembly API
- WebCodecs API (WebCodecs spec, MDN) — browser native codec access
- Kdenlive, Olive, Shotcut — open source NLE architectures (Qt-based)
- MLT framework — producer/consumer/filter design
- GStreamer — element/pad/bin filter graph design
