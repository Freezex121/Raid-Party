# Raid Paper Legends — Itch.io Web Deployment Guide

Deploy the game as an itch.io browser embed while keeping the desktop build fully intact.

---

## Overview

All web-specific changes sit behind `#ifdef __EMSCRIPTEN__` guards. The desktop build (MSVC via CMake) is completely unaffected. You'll maintain two separate build directories:

```
build/          ← MSVC desktop (unchanged)
build-web/      ← Emscripten web (new)
```

---

## Step 1: Install Emscripten SDK

```powershell
cd C:\Users\shaun\OneDrive\Desktop
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
emsdk install latest
emsdk activate latest
```

**PowerShell note:** Use `.\emsdk` (with `.\` prefix) since PowerShell doesn't run EXEs from the current directory by default.

After install and activation, run `emsdk_env.bat` or restart your terminal to add `emcc`, `emcmake`, and `emmake` to PATH.

---

## Step 2: Build raylib for Web

```powershell
cd C:\raylib\raylib
cd 'C:\Users\shaun\OneDrive\Desktop\Raid Paper Legends\Builds\Web'

emcmake cmake C:\raylib\raylib -DCMAKE_BUILD_TYPE=Release -DPLATFORM=Web -DBUILD_EXAMPLES=OFF -G Ninja
emmake ninja -j4
```

This produces `C:\Users\shaun\OneDrive\Desktop\Raid Paper Legends\Builds\Web\libraylib.a` compiled for WebAssembly.

---

## Step 3: Add Web Build Target to CMakeLists.txt

Append after the existing `target_include_directories` line in `CMakeLists.txt`:

```cmake
# Web build (Emscripten)
if(EMSCRIPTEN)
    set_target_properties(RaidParty PROPERTIES
        LINK_FLAGS "--preload-file assets @${CMAKE_SOURCE_DIR}/web_linkflags.txt"
    )
    target_link_libraries(RaidParty PRIVATE -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH -sFORCE_FILESYSTEM -sASYNCIFY)
endif()
```

Create `web_linkflags.txt` in the project root:

```
# Emscripten linker flags for itch.io
--shell-file shell.html
-s MAX_WEBGL_VERSION=2
-s TOTAL_MEMORY=64MB
```

This keeps `CMakeLists.txt` clean — all Emscripten-specific flags live in the txt file.

---

## Step 4: Create an HTML Shell

Create `shell.html` in the project root. Itch.io embeds need proper canvas sizing and fullscreen support. Base it on raylib's default shell at `C:/raylib/raylib/src/shell.html`, or write a minimal one:

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
  <style>
    body { margin: 0; background: #000; overflow: hidden; }
    canvas { width: 100%; height: 100%; display: block; image-rendering: pixelated; }
    .emscripten { position: absolute; top: 50%; left: 50%; transform: translate(-50%,-50%); color: #aaa; font-family: monospace; }
  </style>
</head>
<body>
  <div class="emscripten" id="loading">Loading...</div>
  {{{ SCRIPT }}}
</body>
</html>
```

Key requirements:
- A `<canvas>` element is created by raylib when the module loads
- The `.emscripten` div shows loading progress
- Pixelated rendering for the retro art style

---

## Step 5: meta.c — Already Implemented

The web save/load persistence has already been added to `src/systems/meta.c`. It uses `#ifdef __EMSCRIPTEN__` guards:
- **Load:** reads `raidparty_save` key from `localStorage` via `EM_ASM_PTR`, parses the same key=value format as the desktop file path
- **Save:** serializes all fields to a string buffer via `snprintf`, writes to `localStorage` via `EM_ASM`

Desktop builds (MSVC) compile the `#else` path — standard file I/O to `RaidParty_Meta.sav`. No changes needed.

---

## Step 6: Build the Web Version

```powershell
cd C:\Users\shaun\OneDrive\Desktop\Raid Paper Legends
mkdir Builds\Web
cd Builds\Web

emcmake cmake ..\.. -DCMAKE_BUILD_TYPE=Release
emmake make -j4
```

On success, you'll get four files:

| File | Purpose |
|------|---------|
| `Builds/Web/RaidParty.html` | Entry point for itch.io |
| `Builds/Web/RaidParty.js` | Emscripten glue code |
| `Builds/Web/RaidParty.wasm` | Compiled game binary |
| `Builds/Web/RaidParty.data` | Preloaded assets (from `--preload-file assets`) |

All four must be present together at runtime.

---

## Step 7: Package for Itch.io

All four output files must be at the **root of the zip** (not inside a subdirectory):

```
RaidParty-Web.zip
├── RaidParty.html
├── RaidParty.js
├── RaidParty.wasm
└── RaidParty.data
```

Create the zip:

```powershell
cd C:\Users\shaun\OneDrive\Desktop\Raid Paper Legends\Builds\Web
Compress-Archive -Path RaidParty.html, RaidParty.js, RaidParty.wasm, RaidParty.data -DestinationPath ..\RaidParty-Web.zip
```

### Itch.io Project Settings

1. Create new project → **Kind: HTML**
2. Upload `RaidParty-Web.zip`
3. **Viewport size**: `640 x 360` (matches VIRT_W × VIRT_H)
4. **Permissions**: Enable **Fullscreen**
5. **Embed options**: Recommended — Auto-fit enabled, Show game in feed enabled

---

## Step 8: Test

### Desktop build (unchanged):
```powershell
cmake --build build --config Debug
```
Works exactly as before. No code changes affect the MSVC build path.

### Web build (local server for testing):
```powershell
cd C:\Users\shaun\OneDrive\Desktop\Raid Paper Legends\Builds\Web
python -m http.server 8000
# Open http://localhost:8000/RaidParty.html in a browser
```

Test:
- Game loads and renders at 640×360
- Save data persists across page reloads (check browser dev tools → Application → Local Storage → `raidparty_save`)
- Fullscreen works
- All screens render correctly

---

## Summary: Files Changed

| File | Change | Lines |
|------|--------|-------|
| `CMakeLists.txt` | Add `if(EMSCRIPTEN)` block | ~8 |
| New: `web_linkflags.txt` | Emscripten linker flags | ~3 |
| New: `shell.html` | Custom HTML shell | ~20 |
| `src/systems/meta.c` | Add `#include <emscripten.h>` + web paths in load/save | ~70 |

**Total new code:** ~100 lines. Zero existing code modified for desktop — all additions are `#ifdef __EMSCRIPTEN__` guarded.

---

## Potential Gotchas

| Issue | Cause | Fix |
|-------|-------|-----|
| **`.data` file loads slowly** | All assets are preloaded synchronously | Use `--preload-file assets` without compression, or switch to lazy loading |
| **Save doesn't persist** | MEMFS is in-memory only | The localStorage path in Step 5 fixes this |
| **Audio doesn't play** | Browser requires user gesture before audio context starts | raylib's `InitAudioDevice()` handles this on web; make sure first click starts it |
| **Canvas scaling looks blurry** | Default CSS scaling uses smooth interpolation | Set `image-rendering: pixelated` in CSS (already in shell.html template) |
| **Game crashes with memory error** | Not enough memory for assets | Increase `-s TOTAL_MEMORY` (e.g., 128MB or 256MB) |
