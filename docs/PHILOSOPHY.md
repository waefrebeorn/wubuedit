# WuBuOffice / WuBuPad Philosophy
## A waefrebeorn OS Project — Ground-Up C11, Native-Only, No Vendoring

*"teach you how to slerm"* — WaefreBeorn

---

## 1. What Is WuBuOffice?

WuBuOffice is not an office suite. It is a **shell ecosystem** — a collection
of small, focused, native C11 tools that together form a working desktop
environment. It replaces the idea of "installing LibreOffice" with the idea
of "building each tool yourself, byte by byte, from a single clean compiler."

The suite consists of:
- **WuBuOS** — a custom shell (`wubuos`) that is the session manager
- **WuBuPad** — a text editor built from scratch (not a fork of anything)
- **WuBuEdit** — a video editor with from-scratch timeline model (FFmpeg/libav linked as system library, like `-lz`)
- **WuBuOCR** — an OCR pipeline (CRNN+CTC in C11)
- **WuBuTerm** — a terminal emulator

## 2. The SLERM Way

Every component is built using the **SLERM methodology**:

1. **Read the reference** — study how FFmpeg, Kdenlive, or Vim does it, for the
   architecture patterns and conventions. Read it, understand it, then close it.

2. **Write every byte yourself** — do NOT fork. Do NOT vendor. Do NOT copy
   headers from upstream repos. Your code is yours, from the `#include` guards
   down to the `free()` call.

3. **Opaque structs in public headers** — users see `wedit_timeline_t*`, never
   the internal fields. This is not encapsulation for security; it is
   encapsulation for honesty. If we change the struct, we change our code, not
   theirs.

4. **Native-only build guards** — the build system HARD-FAILS if someone tries
   to compile with Electron, a JVM, or any web runtime. We do not do those.

5. **System libraries are fine** — FFmpeg/libav, libpng, zlib. These are
   system libraries. We link them. We don't fork them. We don't vendor them.
   Our project is still from-scratch C11 — we just use the compiler's standard
   mechanism for libraries.

6. **Verify with independent oracles** — if we write a codec wrapper, we test
   it against known-good FFmpeg outputs. If we write an editor, we check it
   against existing video files. The reference informs, it does not constrain.

## 3. WuBuPad Philosophy

WuBuPad is a text editor. That is all. It does these things and nothing else:

- Open files, edit text, save files
- Syntax highlighting (minimal, via regex matchers we write ourselves)
- Terminal-based (TUI), using raw VT100 escape sequences — no ncurses dependency
- Modal editing (inspired by Vim, but simpler: insert mode and normal mode)
- Small (<10K LOC for the core), fast (single binary, no runtime)

### Why Not Use Vim/Emacs?
Because those are someone else's code. WuBuPad is ours. Every keybind, every
render loop, every buffer operation is hand-written. When it breaks, we know
exactly where. When we want to add a feature, we don't fight a 30-year-old
plugin API.

### Architecture
```
wubupad/
├── src/
│   ├── buffer.c/h    — text buffer (rope or gap buffer, our choice)
│   ├── renderer.c/h  — VT100 TUI renderer (raw escape sequences)
│   ├── input.c/h     — keyboard input parser (esc sequences)
│   ├── editor.c/h    — main editor state and command loop
│   └── main.c        — entry point
├── tests/
│   └── test_buffer.c — unit tests for buffer operations
├── CMakeLists.txt
└── docs/PHILOSOPHY.md (this file, adapted)
```

## 4. WuBuEdit Philosophy

WuBuEdit is a video editor. Its distinguishing features:

### Timeline Model (Pure C11, Zero FFmpeg)
- `wedit_timeline_t*` — opaque handle to a multi-track timeline
- Tracks contain clips, each with a source path, time position, and in/out points
- Clips can be added/removed/moved at arbitrary time positions
- The timeline model knows nothing about codecs, containers, or pixels
- **Why?** Because the timeline is our idea, our data structure, our code

### Media Pipeline (C11 + FFmpeg/libav)
- `wedit_media*` — opaque handle to an open media file (demuxed streams)
- `wedit_decoder*` — decodes packets into raw AVFrames
- `wedit_encoder*` — encodes raw frames into packets
- `wedit_muxer*` — writes packets into MP4/MOV containers
- **Why FFmpeg?** Codec support is a solved problem. We don't reinvent H.264
  encoding. We link libavcodec the same way we link libc.

### Render Command Builder
- `wedit_timeline_build_render_command()` — generates an FFmpeg command string
  from the timeline model, translating clip positions to `trim`/`concat` filters
- This is the bridge: our timeline → FFmpeg filter_complex → encoded output
- **Future:** replace the command-string bridge with direct libav calls

### Why Not Use Kdenlive/Olive/Shotcut?
Those are someone else's code. WuBuEdit is ours. We write the timeline model,
we write the clip management, we write the render command builder. FFmpeg is
just the codec layer, like `-lz` for compression.

## 5. The WuBuOS Shell

WuBuOS (`wubuos`) is a session manager — the process you get when you log in.
Instead of bash + desktop environment, you get:

- **WuBuPad** as the primary interface
- **WuBuTerm** for running commands
- **WuBuEdit** launched with `wubuedit file.wed`
- **WuBuOCR** launched with `wubuocr image.png`

The shell is not a shell — it is a session manager that knows which WuBuTool
to launch for which file type. It uses `fork()`/`exec()` to manage processes.
No X11, no Wayland, no systemd. Just POSIX and C11.

## 6. No Monoliths

From soul.md §10: "No monoliths." Each WuBuTool is:
- A separate static library (`libwedit_timeline.a`, `libwedit_media.a`)
- A separate binary (`wubuedit`, `wubupad`, `wubuos`)
- Built with a single top-level CMake file that includes subdirectory CMakeLists
- Tested independently — each tool has its own `tests/` directory

## 7. The WuBu Philosophy in One Sentence

> We build every tool ourselves, from scratch, in C11. We link system libraries
> like FFmpeg the same way we link libc — as dependencies, not as our code.
> We never fork, never vendor, never copy. The reference teaches us, but we
> ship only what we wrote.

---

*Research basis: 50+ web searches covering FFmpeg libav API, MLT, GStreamer,
  Kdenlive, Shotcut, OpenShot, Olive, FFmpeg.wasm, WebCodecs, Canva, Kapwing,
  Veed, Clideo, WeVideo — all informing the WuBuEdit/WuBuPad SLERM design.*
