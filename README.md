<div align="center">

# 🌑 hode-hd

**An HD-capable, scriptable fork of the [`hode`](https://github.com/usineur/hode) engine for *Heart of Darkness* (Amazing Studio, 1998).**

[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20WSL-blue)](#building)
[![Language](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Backend](https://img.shields.io/badge/SDL2-2.x-informational)](https://www.libsdl.org/)
[![Status](https://img.shields.io/badge/build-passing-brightgreen)](#building)
[![Upstream](https://img.shields.io/badge/upstream-usineur%2Fhode-lightgrey)](https://github.com/usineur/hode)

*Faithful to the original 12.5 Hz logic. Modern presentation, persistent cache, scriptable from Python.*

</div>

---

```bash
$ ./hode --4k --hd-wide --smooth --hd-cache=./cache --prerender
HD mode: 15x scale (3840x2880), 16:9 widescreen, disk cache, prerender
prerender level 0 (rock) [████████████████████████████████] 5320/5320 (100%) 6.9s
```

The original engine logic is **preserved exactly** — same 12.5 Hz tick, same `.lvl` / `.sss` / `.mst` / `.paf` data path, same per-level scripted callbacks. This fork adds a **configurable presentation pipeline** on top: chained-xBRZ HD upscaling with MLAA edge smoothing, 16:9 widescreen with palette-aware borders, decoupled 60 Hz interpolated render, persistent disk cache, sprite + cutscene prerender, plus a programmable Unix-socket automation API and a handful of menu / input / portability fixes.

> [!IMPORTANT]
> **Game data not included.** You must own the original *Heart of Darkness* PC release. Place `HOD.PAF`, `SETUP.DAT`, and the per-level files next to the binary or pass `--datapath=PATH`.

---

## ✨ Highlights

<table>
<tr>
<td width="50%">

🖼️ **HD upscaling**
Chained xBRZ at 6× / 8× / 10× / 15×, MLAA edge smoothing, per-screen background cached once.

📺 **16:9 widescreen**
Palette-derived gradient borders. No fake gameplay.

⏱️ **Smooth animation**
60 Hz render with sprite-position interpolation, 12.5 Hz logic.

🎬 **Cutscene HD cache**
PAF frames upscaled and stored under `cache/paf/<scale>x/`.

</td>
<td width="50%">

🚀 **Prerender**
Single console progress bar covers every sprite (incl. per-screen background animations) and the in-gameplay PAF clips.

🤖 **Automation API**
Unix-domain JSON: `get_state`, `input`, `step`, `screenshot`, `set_level`.

🧰 **Menu/input fixes**
Bind any key (incl. Space/Enter/Tab/Backspace/Win); OK/Cancel/Test row works; defaults stay live.

🍎 **macOS support**
Builds on Apple Silicon and Intel via `<libkern/OSByteOrder.h>` shim.

</td>
</tr>
</table>

---

## 📚 Table of contents

<details>
<summary>Click to expand</summary>

- [✨ Highlights](#-highlights)
- [🛠️ Building](#%EF%B8%8F-building)
  - [🍎 macOS (Apple Silicon & Intel)](#-macos-apple-silicon--intel)
  - [🐧 Linux](#-linux)
  - [🪟 Windows (WSL)](#-windows-wsl)
- [💾 Game data files](#-game-data-files)
- [🚀 Running](#-running)
  - [Quick examples](#quick-examples)
  - [CLI flag reference](#cli-flag-reference)
  - [`hode.ini` reference](#hodeini-reference)
- [🎨 Display & rendering modes](#-display--rendering-modes)
- [🗂️ Disk cache & prerender](#%EF%B8%8F-disk-cache--prerender)
- [🤖 Automation API](#-automation-api)
- [🧪 Test scripts](#-test-scripts)
- [🏗️ Engine architecture](#%EF%B8%8F-engine-architecture)
- [🗺️ Source file map](#%EF%B8%8F-source-file-map)
- [🎮 Game world reference](#-game-world-reference)
- [📦 Data formats](#-data-formats)
- [💾 Save state](#-save-state)
- [🔧 Troubleshooting](#-troubleshooting)
- [📋 Differences from upstream](#-differences-from-upstream)
- [⚠️ Known limitations](#%EF%B8%8F-known-limitations)
- [🤝 Contributing](#-contributing)
- [👏 Credits](#-credits)
- [⚖️ License & legal](#%EF%B8%8F-license--legal)

</details>

---

## 🛠️ Building

The project uses a hand-written GNU `Makefile`. **The only external dependency is SDL2.** C++11 with warnings turned up (`-Wall -Wextra -Wpedantic`); the build is warning-free on macOS clang and Linux gcc.

```bash
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
```

Outputs `./hode` (~800 KB binary). `make clean` removes objects and dependency files.

<details>
<summary>📈 Optimised release build</summary>

```bash
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)" \
  CPPFLAGS='-O2 -std=c++11 -Wall -Wextra -Wno-unused-parameter -Wpedantic -MMD'
```
</details>

### 🍎 macOS (Apple Silicon & Intel)

> [!NOTE]
> Upstream `hode` does **not** build on macOS — clang's libc has no glibc-style `<endian.h>`. This fork ships the fix in [`intern.h`](intern.h):
>
> ```cpp
> #elif defined(__APPLE__)
>   #include <libkern/OSByteOrder.h>
>   #define le16toh(x) OSSwapLittleToHostInt16(x)
>   #define le32toh(x) OSSwapLittleToHostInt32(x)
>   #define htole16(x) OSSwapHostToLittleInt16(x)
>   #define htole32(x) OSSwapHostToLittleInt32(x)
>   static const bool kByteSwapData = (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__);
> ```

**Step-by-step on a fresh macOS install:**

```bash
# 1. Xcode Command Line Tools (clang + git)
xcode-select --install

# 2. Homebrew SDL2
brew install sdl2

# 3. Build
make -j"$(sysctl -n hw.ncpu)"

# 4. Run (data files alongside the binary)
./hode --hd
```

If `brew` is in `/opt/homebrew` (Apple Silicon default), make sure it's on `PATH` so `sdl2-config` resolves:

```bash
echo 'export PATH="/opt/homebrew/bin:$PATH"' >> ~/.zprofile
source ~/.zprofile
```

<details>
<summary>🍎 macOS-specific notes</summary>

| Concern | Notes |
|---|---|
| **Display backend** | SDL2 uses Metal via `SDL_RENDERER_ACCELERATED`; nothing extra required. |
| **Endianness** | `<libkern/OSByteOrder.h>` is the macOS-blessed equivalent of `<endian.h>`. The fork's `intern.h` selects the right header per platform. |
| **Hardened runtime / Gatekeeper** | The Makefile produces an unsigned binary. First Finder launch may show "cannot be opened" — right-click → Open, or run from Terminal. No entitlements required. |
| **Audio** | Uses SDL's CoreAudio backend by default. `SDL_AUDIODRIVER=dummy` disables for headless / CI use. |
| **Apple Silicon** | Native arm64 build; clang auto-vectorisation benefits the xBRZ scalers. No Rosetta required. |
| **Headless** | macOS has no easy `Xvfb` equivalent. Use `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` for non-interactive runs (still requires a window server). For real headless CI, run the Linux build in a container with `xvfb-run`. |
| **Bundle / `.app`** | Ships as a CLI binary, not a `.app`. Wrap with [`appify`](https://gist.github.com/mathiasbynens/674099) if you want Finder integration. |
| **Debug build** | `make clean && make -j"$(sysctl -n hw.ncpu)" CPPFLAGS='-g -O0 -std=c++11 -Wall -Wextra -Wno-unused-parameter -Wpedantic -MMD' && lldb -- ./hode --hd` |

A clean macOS build is normally a few seconds:
```text
$ make clean && make -j$(sysctl -n hw.ncpu) 2>&1 | tail -3
c++  -g -std=c++11 ... -c -o video.o video.cpp
c++  -o hode andy.o automation_api.o ... `sdl2-config --libs`
```

</details>

### 🐧 Linux

```bash
# Debian / Ubuntu / WSL
sudo apt install build-essential libsdl2-dev pkg-config

# Fedora / RHEL
sudo dnf install gcc-c++ SDL2-devel

# Arch
sudo pacman -S base-devel sdl2

make -j"$(nproc)"
```

Headless CI:

```bash
SDL_AUDIODRIVER=dummy xvfb-run -a ./hode --automation=/tmp/hode.sock --hd
```

### 🪟 Windows (WSL)

WSL2 + WSLg (built into Windows 11) works out of the box:

```bash
sudo apt install build-essential libsdl2-dev pkg-config
make -j"$(nproc)"
```

Native Windows builds (MinGW / MSYS2) are not currently maintained — the Makefile assumes POSIX. Patches welcome.

---

## 💾 Game data files

> [!CAUTION]
> The data files are **not redistributed**. You must own a legitimate copy of *Heart of Darkness*.

Place these next to the binary or pass `--datapath=PATH`:

| File | Description | Size (retail) |
|---|---|---:|
| `HOD.PAF` | All cutscenes (Packed Animation File container) | ~411 MB |
| `SETUP.DAT` | Strings, fonts, hint images, loading screen, sound metadata | ~5.5 MB |
| `*_HOD.LVL` ×9 | Per-level geometry, sprite tables, screen layout | 0.7 – 5 MB each |
| `*_HOD.SSS` ×9 | Sound script (SssBank/SssSample) | 3 – 7 MB each |
| `*_HOD.MST` ×9 | Monster + scripting tables | 10 – 150 KB each |

PSX disc data (`*.dax`, MDEC backgrounds, SPU ADPCM) is also supported — the HD upscaler works for both PC and PSX paths.

`RELEASES.yaml` (carried over from upstream) lists SHA-1 hashes for every game version validated against (French / German / English Win32, demos, PSX). If your data files act weird, compare them there.

---

## 🚀 Running

### Quick examples

```bash
# Vanilla 256×192 windowed (upstream behaviour)
./hode

# HD (1536×1152) at default 6× scale
./hode --hd

# 4K with 16:9 borders, 60 Hz interpolation, persistent cache, prerender first run
./hode --4k --hd-wide --smooth --hd-cache=./cache --prerender

# Skip menu, jump to Island level, checkpoint 2, in HD
./hode --level=isld --checkpoint=2 --hd

# Custom 9× scale (2304×1728)
./hode --hd-scale=9

# Scriptable mode — opens a Unix socket and disables menu/loading/cutscenes
./hode --automation=/tmp/hode.sock --hd

# Read data files from elsewhere
./hode --datapath=/Volumes/HOD-CD --savepath="$HOME/Library/Application Support/hode"
```

### CLI flag reference

> All flags use the GNU long form (`--name` or `--name=value`); there are no short flags.

#### Path & navigation

| Flag | Argument | Default | Effect |
|---|---|---|---|
| `--datapath=PATH` | path | `.` | Directory containing game data files |
| `--savepath=PATH` | path | `.` | Directory for `setup.cfg` and screenshots |
| `--level=NUM\|NAME` | int 0–8 or name | — | Skip menu, start at given level |
| `--checkpoint=NUM` | int | 0 | Checkpoint within `--level` |

Level names (also accept indices): `rock`, `fort`, `pwr1`, `isld`, `lava`, `pwr2`, `lar1`, `lar2`, `dark`.

#### HD rendering

| Flag | Argument | Default | Effect |
|---|---|---|---|
| `--hd` | — | off | Enable HD compositor at default 6× scale |
| `--hd-scale=N` | int 2–16 | — | HD compositor at custom scale; implies `--hd` |
| `--fullhd` | — | off | Shortcut: HD at 8× (2048×1536) |
| `--4k` | — | off | Shortcut: HD at 15× (3840×2880) |
| `--hd-wide` | — | off | 16:9 framebuffer with palette-gradient borders; sizes window 16:9 |
| `--hd-cache=PATH` | path | — | Read/write upscaled artifacts under `PATH/<scale>x/` |
| `--prerender` | — | off | Pre-fill sprite + relevant PAF caches per level |
| `--smooth` | — | off | 60 Hz render with sprite-position interpolation |

#### Scripting & debug

| Flag | Argument | Default | Effect |
|---|---|---|---|
| `--automation=PATH` | unix-socket | — | Open JSON automation socket; disables menu/loading/cutscenes |
| `--debug=MASK` | int | 0 | OR'd into `g_debugMask` ([see below](#debug-bitmask)) |
| `--cheats=MASK` | int | 0 | OR'd into cheat flags ([see below](#cheat-flags)) |

> CLI flags **override** matching `hode.ini` keys.

### `hode.ini` reference

Place a `hode.ini` next to the binary (or in `--datapath`) for persistent configuration:

```ini
[engine]
disable_paf       = false   ; skip cutscenes (auto-true if HOD.PAF missing)
disable_mst       = false   ; skip monster scripting (debugging only)
disable_sss       = false   ; skip sound script (debugging only)
disable_menu      = false   ; boot straight into the game
max_active_sounds = 16      ; SSS object pool cap
difficulty        = 1       ; 0=easy, 1=normal, 2=hard
frame_duration    = 80      ; ms per game tick (default 80 = 12.5 Hz)
loading_screen    = true    ; show "please wait" image during level load

[display]
scale_factor      = 3       ; integer upscaler factor (legacy non-HD path)
scale_algorithm   = nearest ; nearest | linear | xbr | nearest+blur | …
gamma             = 1.0     ; output gamma (1.0 = neutral)
fullscreen        = false
widescreen        = false   ; legacy 16:9 path (blur-stretch). Prefer hd_widescreen.
hd_mode           = false   ; same as --hd
hd_scale          = 0       ; same as --hd-scale=N (0 = default 6)
hd_widescreen     = false   ; same as --hd-wide
hd_cache          = ./cache ; same as --hd-cache
smooth_anim       = false   ; same as --smooth
automation_socket = /tmp/hode.sock ; same as --automation
```

> [!TIP]
> Booleans accept `true`/`false`, `t`/`f`, `1`/`0` (case-insensitive). Lines starting with `#` are comments. `disable_paf` is auto-set to `true` if the engine can't open `HOD.PAF`.

---

## 🎨 Display & rendering modes

### Standard mode

`./hode` (no flags) gives you the upstream behaviour: 256×192 indexed-colour framebuffer, integer-upscaled `scale_factor` times by SDL through the chosen software scaler. Behaviour and visuals match upstream `hode` exactly.

### HD upscaling

`--hd` / `--fullhd` / `--4k` / `--hd-scale=N` route every visible pixel through `HdCompositor` ([`hd_compositor.{h,cpp}`](hd_compositor.cpp)):

- Background bitmap is upscaled **once per screen change** and reused.
- Each sprite is upscaled **once per (content + dimensions + flip + palette) tuple** and cached.
- HD framebuffer is RGBA32; SDL2 streams it to a `SDL_Texture` per frame.

### Scale chains

xBRZ scales by 2×, 3×, 4× or 5× per pass; arbitrary scales decompose into two passes:

| Preset / `--hd-scale` | Pass 1 | Pass 2 | Output (256×192) |
|---:|:--|:--|:--|
| 2 | 2× | — | 512 × 384 |
| 3 | 3× | — | 768 × 576 |
| 4 | 4× *(2×·2×)* | — | 1024 × 768 |
| 5 | 5× *(nearest)* | — | 1280 × 960 |
| **6 (`--hd`)** | 3× | 2× | **1536 × 1152** |
| 7 | 4× | 2× → crop | 1792 × 1344 |
| **8 (`--fullhd`)** | 4× | 2× | **2048 × 1536** |
| 9 | 3× | 3× | 2304 × 1728 |
| 10 | 4× | 3× → crop | 2560 × 1920 |
| 11 | 4× | 3× → crop | 2816 × 2112 |
| 12 | 4× | 3× | 3072 × 2304 |
| 13 | 4× | 4× → crop | 3328 × 2496 |
| 14 | 4× | 4× → crop | 3584 × 2688 |
| **15 (`--4k`)** | 4× | 4× → SDL downscale | **3840 × 2880** |
| 16 | 4× | 4× | 4096 × 3072 |

After the chained scale, [`mlaa_smooth()`](edge_smooth.cpp) runs a morphological-edge-AA pass that softens stair-stepping artefacts on diagonal edges (cape, hair, plasma traces).

### Widescreen 16:9 borders

`--hd-wide` switches the HD framebuffer from 4:3 to 16:9. The 4:3 game stays centred; left and right margins are filled with a **vertical gradient** sampled from the top and bottom rows of the **current screen's palette**, then darkened to ~⅓ brightness so it reads as ambient atmosphere instead of fake gameplay. Borders re-sample whenever the screen changes, so they track lighting/mood per area.

> [!NOTE]
> This fork **resizes the SDL window itself to 16:9** when `--hd-wide` is on. Without that change (upstream behaviour), the wide framebuffer was squished into a 4:3 window and the colored borders were clipped to invisibility.

### Smooth (60 Hz) animation

`--smooth` decouples logic from rendering:

```text
        ┌───── 12.5 Hz ─────┐         ┌───── ~60 Hz ─────┐
        │  game tick        │         │   render frame   │
        │  ‒ AI / physics   │   ──►   │   ‒ interpolate  │
        │  ‒ sprite list    │         │     prev → curr  │
        │  ‒ snapshot pos   │         │   ‒ blit to HD   │
        └───────────────────┘         └──────────────────┘
```

- Game tick stays at 80 ms (12.5 Hz). Collision, AI, sound, scripted callbacks behave exactly as upstream.
- `Game::saveInterpolationState()` snapshots `(prev, curr)` for Andy and every visible sprite at each tick.
- `Game::renderInterpolatedFrame(t)` — called at the render rate — blends with `t ∈ [0,1]`.

The result is fluid on-screen motion with **zero impact on gameplay timing**.

---

## 🗂️ Disk cache & prerender

`--hd-cache=PATH` makes every upscaled artifact persistent.

### On-disk layout

```text
PATH/
├── 6x/                                  ← sprites at the active scale
│   └── spr_<key:016x>_<w>x<h>.raw      ← header (int32 w, int32 h) + w*h*4 RGBA
├── 8x/
└── paf/
    ├── 6x/
    │   ├── v00/                          ← cutscene 0 (intro)
    │   │   ├── f0000.raw
    │   │   └── …
    │   ├── v22/                          ← Canyon Andy-falling-with-cannon
    │   └── v24/                          ← Island Andy-falling
    └── 8x/
```

Each `<scale>x/` directory is independent — switching `--hd-scale` populates a new directory without touching existing ones.

### Cache key

```text
key = FNV-1a-64(
        decoded indexed bytes (w × h)
      ⊕ w little-endian uint16
      ⊕ h little-endian uint16
      ⊕ flags & 3                      ← horizontal-flip bit
      ⊕ palette FNV-1a-64
      )
```

Including the **palette hash** is the reason the same sprite content rendered with different on-screen palettes (between screens, cross-fade, cutscenes) gets distinct cache entries. Without this, low scales (HD/FullHD) would occasionally show *wrong-coloured* sprite frames as a stale entry was reused after a palette transition.

> [!WARNING]
> This changes the on-disk format compared to upstream and earlier builds of this fork. **Delete any pre-existing `cache/` directory** before the first run with palette-hashed keys.

### Prerender flow

`--prerender` populates the cache up-front, **once per level** (tracked via `Game::_hdPrerenderedMask`). After `Game::restartLevel()` finishes loading the level and before the gameplay loop starts:

1. **Sprite phase** — walk every `(sprite-type, frame, flip)` tuple across:
   - the main level table `_resLevelData0x2988PtrTable[0..31]`
   - every screen's `backgroundLvlObjectDataTable[0..7]` (per-screen background animations: trees, water, ambient effects). Without this, those animations only upscaled the first time they appeared on-screen.
2. **PAF phase** — fast-forward decode the small in-gameplay clips (Canyon falling with cannon, Canyon falling, Island falling) plus the level's intro cutscene. Each not-yet-cached cutscene is decoded full-speed without audio/display/sleep so the existing HD frame callback writes every frame to disk.
3. Both phases share **one progress bar**:
   ```text
   prerender level 0 (rock) [████████████████████████████....] 4720/5320 (88%) 5.2s eta 0.7s
   ```

`_playedMask` (watched cutscenes) is snapshotted/restored across each PAF prerender so your save isn't polluted with movies you didn't actually watch. After the bar completes, the engine pumps SDL once and re-baselines `inp.prevMask = inp.mask` so any held keys don't fire phantom press/release events on the first gameplay tick.

---

## 🤖 Automation API

`--automation=/tmp/hode.sock` opens a non-blocking `AF_UNIX` `SOCK_STREAM` listening for one client at a time. **Newline-delimited JSON** in both directions.

`--automation` implies:

- 🚫 `disable_menu = true` — boot straight into gameplay
- 🚫 `loading_screen = false` — no "please wait"
- 🚫 `disable_paf = true` — cutscenes skipped

### Protocol

- One JSON object per line, terminated with `\n`.
- Server only responds to `get_state` and `screenshot`. Other commands are fire-and-forget.
- One client at a time. Connection drop is detected on next `read()`; engine continues and re-`accept()`s.

### Commands

| `cmd` | Body fields | Reply | Effect |
|---|---|---|---|
| `get_state` | – | JSON object *(see below)* | Snapshot Andy + monsters + level/checkpoint/screen |
| `input` | `dir`, `act`, `frames`, `raw` | – | Inject input for `frames` ticks |
| `step` | `count` | – | Enable step mode and step `count` ticks |
| `screenshot` | – | header line + `width*height*3` raw RGB bytes | Capture 256×192 framebuffer |
| `set_level` | `level`, `checkpoint` | – | Restart at given level/checkpoint |

`get_state` reply:

```json
{
  "andy": {
    "x": 128, "y": 96, "screen": 0,
    "anim": 0, "frame": 0, "sprite": 0,
    "hasCannon": true, "dying": false
  },
  "level": 0, "checkpoint": 0, "screen": 0,
  "endLevel": false,
  "monsters": [
    {"x": 200, "y": 100, "type": 1, "i": 0},
    {"x": 220, "y": 100, "type": 2, "i": 0}
  ],
  "monsterCount": 2
}
```

Input bit layout:

| Bit  | `dir`  | `act`  | Raw `SYS_INP_*`               |
| ---: | :----- | :----- | :---------------------------- |
| 0x01 | UP     | RUN    | `SYS_INP_UP=0x01`, `RUN=0x10` |
| 0x02 | RIGHT  | JUMP   | `SYS_INP_RIGHT=0x02`, `JUMP=0x20` |
| 0x04 | DOWN   | SHOOT  | `SYS_INP_DOWN=0x04`, `SHOOT=0x40` |
| 0x08 | LEFT   | –      | `SYS_INP_LEFT=0x08`           |
| 0x80 | –      | –      | `SYS_INP_ESC=0x80` (raw only) |

### Python client

```python
import socket, json

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect("/tmp/hode.sock")
f = s.makefile("rwb", buffering=0)

def send(j): f.write((json.dumps(j) + "\n").encode())
def recv_line(): return json.loads(f.readline().decode())

# State snapshot
send({"cmd": "get_state"})
state = recv_line()
print(state["andy"], "monsters:", state["monsterCount"])

# Walk right + run for 30 ticks
send({"cmd": "input", "dir": 2, "act": 1, "frames": 30})

# Frame-by-frame step for 10 ticks
send({"cmd": "step", "count": 10})

# Screenshot — header then raw RGB
send({"cmd": "screenshot"})
hdr = recv_line()           # {"width":256,"height":192,"format":"rgb","size":N}
img = b""
while len(img) < hdr["size"]:
    img += s.recv(4096)
open("frame.rgb", "wb").write(img)

# Jump to a specific level
send({"cmd": "set_level", "level": 3, "checkpoint": 0})
```

### Headless testing

```bash
# Linux
SDL_AUDIODRIVER=dummy xvfb-run -a ./hode --automation=/tmp/hode.sock --hd

# macOS (still needs a window server; for true headless use Linux in CI)
SDL_AUDIODRIVER=dummy ./hode --automation=/tmp/hode.sock
```

---

## 🧪 Test scripts

Drop-in Python drivers using the automation API. All require a running `./hode --automation=/tmp/hode.sock`.

| Script | Purpose |
|---|---|
| [`test_walkthrough.py`](test_walkthrough.py) | Drive Andy through level 1 end-to-end |
| [`test_all_levels.py`](test_all_levels.py)  | Boot each of the 9 levels, verify gameplay starts |
| [`test_combat_bot.py`](test_combat_bot.py)  | Reactive AI: fires when monsters in range |
| [`test_hd_replay.py`](test_hd_replay.py)    | Compare HD vs SD rendering frame-by-frame |

---

## 🏗️ Engine architecture

### High-level dataflow

```text
┌──────────────────── main.cpp ────────────────────┐
│  parse CLI / hode.ini → readConfigIni()         │
│  open SETUP.DAT       → Resource::loadSetupDat │
│  init SDL2/audio      → System::init / setupAudio│
│  if --hd : new HdCompositor                      │
│  if --automation : new AutomationApi             │
│  loop:                                           │
│    Game::loadSetupCfg                            │
│    apply keymap                                  │
│    Menu::mainLoop  (unless --automation)         │
│    Game::mainLoop(level, checkpoint)             │
└──────────────────────────────────────────────────┘
                  │
                  ▼
┌──────────────────── Game ────────────────────────┐
│  Resource (.lvl/.sss/.mst/.paf parsers)          │
│  Video (256×192 indexed framebuffer + palette)   │
│  PafPlayer (cutscene streaming)                  │
│  Mixer (software audio mix)                      │
│  HdCompositor* (optional HD path)                │
│  AutomationApi* (optional socket server)         │
│  per-level callbacks (level1_rock.cpp .. level9) │
└──────────────────────────────────────────────────┘
                  │
                  ▼
        ┌─────────────────────┐
        │   System (abstract) │  ← System_SDL2 (this build)
        │                     │     System_PSP, System_Wii (legacy)
        └─────────────────────┘
                  │
                  ▼
              SDL2 (Metal/OpenGL/D3D)
```

### Core types

| Type | File | Responsibility |
|---|---|---|
| `Game` | [`game.h`](game.h) / [`game.cpp`](game.cpp) | Orchestrator; ticks the level main loop |
| `Resource` | [`resource.h`](resource.h) | LVL/SSS/MST/PAF parsing, sprite tables, palettes |
| `Video` | [`video.h`](video.h) | 256-colour framebuffer + palette + layers |
| `PafPlayer` | [`paf.h`](paf.h) | Streams Packed Animation File format |
| `HdCompositor` | [`hd_compositor.h`](hd_compositor.h) | **NEW** — HD framebuffer, presets, borders, prerender |
| `SpriteUpscaler` | [`sprite_upscaler.h`](sprite_upscaler.h) | **NEW** — chained xBRZ + cache |
| `AutomationApi` | [`automation_api.h`](automation_api.h) | **NEW** — Unix socket JSON server |
| `System` | [`system.h`](system.h) | Abstract platform — input, audio, screen |

### HD pipeline

```text
Original logic (256×192, 8-bit indexed)
        │
        ▼  drawScreen() collects sprite list (z-sorted)
        │
        ▼  HdCompositor::beginFrame(bgLayer, palette, screenNum, bgId)
        │   ‒ expand palette 6→8-bit
        │   ‒ if screen changed: SpriteUpscaler::upscaleBackground()
        │   ‒ memcpy _hdBackground → _hdFramebuffer
        │
        ▼  for each sprite:
        │     SpriteUpscaler::getOrUpscale(sprData, w, h, flags, palette)
        │       ‒ decodeSprToTemp(): RLE → indexed
        │       ‒ key = FNV-1a(indexed + dims + flags + palette hash)
        │       ‒ RAM lookup → disk lookup → upscale chain → MLAA → cache
        │     blitHdSprite(): alpha-test composite into _hdFramebuffer
        │
        ▼  HdCompositor::endFrame()
        │   ‒ if widescreen: compositeWidescreen() draws gradient borders
        │
        ▼  System_SDL2::copyRectRGBA(): SDL texture stream → present
```

### PAF cutscene flow

```text
HOD.PAF
  │   one container, k cutscenes, indexed by uint32 LE offset
  ▼
PafPlayer::preload(num)
  │   read sub-header, allocate 4 page buffers + demux blocks
  ▼
PafPlayer::mainLoop()                          ← _prerenderMode skips audio/display/sleep
  │   per frame:
  │     ‒ read N file blocks into demux buffers (video or ADPCM audio)
  │     ‒ decodeVideoFrame() → write current page buffer
  │     ‒ callback(frame) → gamePafFrameCallback (game.cpp)
  │           ├── (legacy) g_system->copyRect to 256×192 layer
  │           └── (HD) cache hit? → present
  │                   else upscale → save to disk → present
  │     ‒ if !prerender: setPalette + updateScreen + sleep frameMs
  ▼
PafPlayer::unload()
```

---

## 🗺️ Source file map

<details>
<summary><b>Original engine (preserved)</b></summary>

| File(s) | Responsibility |
|---|---|
| `andy.cpp` | Player state machine / animation FSM |
| `benchmark.cpp` | CPU benchmark used during the loading screen |
| `defs.h` | On-disk struct layouts (`Lvl*`, `Sss*`, `Mst*`, `SetupConfig`) |
| `fileio.cpp/h` | Buffered binary file reader |
| `fs.h`, `fs_posix.cpp`, `fs_android.cpp` | Filesystem abstraction |
| `intern.h` | LE byte-swap helpers; this fork adds a macOS branch |
| `level.h`, `level1_rock.cpp` … `level9_dark.cpp` | Per-level scripted callbacks |
| `lzw.cpp` | LZW decompressor |
| `mdec.cpp/h`, `mdec_coeffs.h` | PSX MDEC video decoder |
| `menu.cpp/h` | Settings menus, cutscene replay, controls bind |
| `mixer.cpp/h` | Software audio mixer |
| `monsters.cpp` | Monster AI / animation tables |
| `paf.cpp/h` | PAF cutscene format |
| `random.cpp/h` | Game logic PRNG |
| `resource.cpp/h` | LVL / SSS / MST loader, sprite tables |
| `scaler.h`, `scaler_xbr.cpp` | Legacy software scalers |
| `screenshot.cpp/h` | PNG/BMP dump |
| `sound.cpp` | SSS interpreter |
| `staticres.cpp` | Baked-in tables |
| `system.h` | Abstract platform interface |
| `system_sdl2.cpp` | Active SDL2 backend |
| `system_psp.cpp`, `system_wii.cpp` | Legacy backends, **not built** by this Makefile |
| `util.cpp/h` | Debug print, error helpers |
| `video.cpp/h` | Legacy framebuffer + palette |

</details>

<details open>
<summary><b>Added by this fork</b></summary>

| File(s) | Responsibility |
|---|---|
| `hd_compositor.cpp/h` | HD framebuffer; multi-resolution presets; 16:9 widescreen with palette borders; prerender driver; unified progress bar |
| `sprite_upscaler.cpp/h` | xBRZ chained scalers (`2x`, `3x`, `4x = 2x·2x`, `5x ≈ nearest`, `Nx via decompose`); RAM LRU + disk cache with FNV-1a content key |
| `edge_smooth.cpp/h` | MLAA edge smoothing |
| `automation_api.cpp/h` | Unix-socket JSON server; input injection; step mode; screenshot |
| `test_walkthrough.py` | Level 1 walkthrough using the automation API |
| `test_all_levels.py` | Per-level boot smoke test |
| `test_combat_bot.py` | Reactive combat AI |
| `test_hd_replay.py` | HD vs SD comparison harness |

</details>

<details>
<summary><b>Modified vs upstream</b></summary>

| File | Why |
|---|---|
| `Makefile` | Add the four new `.cpp` sources to `SRCS` |
| `intern.h` | macOS `<libkern/OSByteOrder.h>` shim |
| `main.cpp` | New CLI flags + INI keys; wire `_video->_font` after `loadSetupDat()` |
| `game.cpp/h` | HD compositor begin/end-frame hooks; smooth-anim 60 Hz interpolated render loop; per-level sprite + PAF prerender driver with shared progress bar |
| `paf.cpp/h` | HD frame callback path; `_prerenderMode` skips audio/display/sleep; `peekFramesCount(num)` |
| `menu.cpp/h` | Keyboard binding screen overhaul (OK/Cancel/Test, one-key-one-action, Space/Enter/Tab/Backspace/Win bindable, fallback labels); font init guard |
| `system.h` | Expose `applyKeyboardControls` and `waitForKeyPress` on the abstract interface |
| `system_sdl2.cpp` | Default mappings stay live alongside user keys; `waitForKeyPress` clears edge state on return; `copyRectRGBA` for HD presents |
| `video.cpp` | Zero-init `_font` so the menu can't dereference garbage before `Game::mainLoop` wires it |

</details>

---

## 🎮 Game world reference

### Levels

| # | Code | Display name | Notes |
|---:|:--|:--|:--|
| 0 | `rock` | Canyon | Tutorial / first level |
| 1 | `fort` | Fort | |
| 2 | `pwr1` | Power 1 | The Swamp |
| 3 | `isld` | Island | |
| 4 | `lava` | Lava | |
| 5 | `pwr2` | Power 2 | Underwater |
| 6 | `lar1` | Lair 1 | |
| 7 | `lar2` | Lair 2 | |
| 8 | `dark` | Dark | Final level (single checkpoint) |

Level callbacks dispatch from `Game::callLevel_*` into the `levelN_<code>.cpp` matching `_currentLevel`.

### Cutscenes

<details>
<summary>All 25 PAF videos in <code>HOD.PAF</code></summary>

| # | Symbol | Purpose |
|---:|:--|:--|
| 0 | `kPafAnimation_intro` | Game intro |
| 1 | `kPafAnimation_cine14l` | Cinematic transition |
| 2 | `kPafAnimation_rapt` | Cinematic |
| 3 | `kPafAnimation_glisse` | Slide |
| 4 | `kPafAnimation_meeting` | Meeting cinematic |
| 5 | `kPafAnimation_island` | Island intro |
| 6 | `kPafAnimation_islefall` | Falling on Island |
| 7 | `kPafAnimation_vicious` | |
| 8 | `kPafAnimation_together` | |
| 9 | `kPafAnimation_power` | |
| 10 | `kPafAnimation_back` | |
| 11 | `kPafAnimation_dogfree1` | |
| 12 | `kPafAnimation_dogfree2` | |
| 13 | `kPafAnimation_meteor` | |
| 14 | `kPafAnimation_cookie` | |
| 15 | `kPafAnimation_plot` | |
| 16 | `kPafAnimation_puzzle` | |
| 17 | `kPafAnimation_lstpiece` | |
| 18 | `kPafAnimation_dogfall` | |
| 19 | `kPafAnimation_lastfall` | |
| 20 | `kPafAnimation_end` | |
| 21 | `kPafAnimation_cinema` | |
| 22 | `kPafAnimation_CanyonAndyFallingCannon` | In-gameplay: Canyon, falls with cannon |
| 23 | `kPafAnimation_CanyonAndyFalling` | In-gameplay: Canyon, falls without cannon |
| 24 | `kPafAnimation_IslandAndyFalling` | In-gameplay: Island falling |

The per-level intro cutscene uses `_cutscenes[] = { 0, 2, 4, 5, 6, 8, 10, 14, 19 }`.

</details>

### Default key bindings

| Key | Action |
|---|---|
| Arrow keys | Move (Up = climb, Down = crouch) |
| `Left Ctrl`, `F` | Run |
| `Left Alt`, `G`, `Enter` | Jump |
| `Left Shift`, `H` | Shoot |
| `D`, `Space` | Special (Run + Shoot) |
| `Esc` | Pause / quit menu |
| `S` | Screenshot |

### Settings menu

`Menu → Settings → Keyboard Controls` lets you bind any of Run / Jump / Shoot / Special:

- ✅ **Letters, digits, Shift, Ctrl, Alt** – have icon glyphs in the engine's bitmap font.
- ✅ **Space, Enter, Tab, Backspace, Cmd/Win** – bindable; menu shows short text labels (`SP`, `EN`, `TB`, `BS`, `WN`).
- ✅ **Two slots per action** – first bind goes into slot 1, second into slot 2.
- ✅ **One key, one action** – binding a key already used by another action automatically clears the prior binding.
- ✅ **Defaults stay live** – `LCtrl`, `F`, `LAlt`, `G`, `LShift`, `H`, `D`, `Space` remain mapped to their defaults alongside any custom keys, so the in-menu Select handler always works while you're rebinding the rest.
- ✅ **OK / Cancel / Test row** – navigable via ←/→. Select on **OK** keeps changes, **Cancel** reverts, **Test** enters live key-press visualisation.
- ✅ **Esc cancels bind** – inside the bind prompt cancels the bind only; doesn't propagate to the outer menu.

### Cheat flags

`--cheats=N` is a bitmask:

| Bit | Symbol | Effect |
|---:|:--|:--|
| `1 << 0` | `kCheatSpectreFireballNoHit` | Spectre fireballs don't hit Andy |
| `1 << 1` | `kCheatOneHitPlasmaCannon` | One-shot kill with plasma cannon |
| `1 << 2` | `kCheatOneHitSpecialPowers` | One-shot kill with special powers |
| `1 << 3` | `kCheatWalkOnLava` | Walk on lava without dying |
| `1 << 4` | `kCheatGateNoCrush` | Gates won't crush Andy |
| `1 << 5` | `kCheatLavaNoHit` | Lava doesn't damage |
| `1 << 6` | `kCheatRockShadowNoHit` | Shadow monsters in rock don't damage |

### Debug bitmask

`--debug=N` is OR'd into `g_debugMask`:

| Bit | Symbol | Output |
|---:|:--|:--|
| `1 << 0` | `kDebug_GAME` | Game / level state transitions |
| `1 << 1` | `kDebug_RESOURCE` | Resource loader |
| `1 << 2` | `kDebug_ANDY` | Andy state machine |
| `1 << 3` | `kDebug_SOUND` | SSS interpreter |
| `1 << 4` | `kDebug_PAF` | PAF cutscene player |
| `1 << 5` | `kDebug_MONSTER` | Monster AI |
| `1 << 6` | `kDebug_SWITCHES` | `lar1` / `lar2` switches |
| `1 << 7` | `kDebug_MENU` | Menu state machine |

`--debug=255` enables everything.

---

## 📦 Data formats

<details>
<summary><b><code>*_HOD.LVL</code></b> — level geometry & sprite tables</summary>

- 4-byte tag `0x4D5A4448` ('HDZM') at offset 0
- Header counts: `screensCount`, `staticLvlObjectsCount`, `otherLvlObjectsCount`, `spritesCount`
- `_screensGrid[N][4]` at 0x08 — per-screen up/right/down/left neighbour table
- `_screensBasePos[N]` at 0xA8 — per-screen world coordinate
- `_screensState[N]` at 0x1E8 — per-screen flags
- `_resLvlScreenObjectDataTable[]` at 0x288 — 96-byte `LvlObject` entries
- Sprite type table at `_lvlSpritesOffset = 0x288 + 96 * (96 or 104)` — 32 × 16-byte entries
- Background table at `_lvlBackgroundsOffset = _lvlSpritesOffset + 32*16` — 40 × 16-byte entries

</details>

<details>
<summary><b><code>*_HOD.SSS</code></b> — sound script</summary>

- Sound-script bytecode interpreted by `sound.cpp`
- `SssBank`, `SssSample`, `SssPcm`, `SssPreloadList`, `SssPreloadInfoData` defined in `resource.h`
- ADPCM sample data (PSX) or PCM samples (PC) referenced by offset

</details>

<details>
<summary><b><code>*_HOD.MST</code></b> — monsters & scripting</summary>

- Monster + scripting tables
- Op codes interpreted by `executeMstCode()` in `monsters.cpp`

</details>

<details>
<summary><b><code>HOD.PAF</code></b> — Packed Animation File</summary>

- Container of N cutscenes, each indexed by a uint32 LE offset at the start
- Each cutscene starts with: `Packed Animation File V1.0\n(c) 1992-96 Amazing Studio\n`
- Header at offset `+0x84..0xAC`: `framesCount`, `frameDuration`, `startOffset`, `preloadFrameBlocksCount`, `readBufferSize`, `maxVideoFrameBlocksCount`, `maxAudioFrameBlocksCount`, `frameBlocksCount`
- Followed by `frameBlocksCountTable[framesCount]`, `framesOffsetTable[framesCount]`, `frameBlocksOffsetTable[frameBlocksCount]`
- Each frame is delta-encoded against the previous; 4-page rotation buffer; 4 op codes (0..3) for partial vs full blits and palette updates
- Audio (when present) is 22 kHz ADPCM in interleaved blocks

</details>

<details>
<summary><b><code>SETUP.DAT</code></b> — fonts, hints, loading screen</summary>

- Versioned (10 or 11)
- Header has counts (`iconsCount`, `menusCount`, `cutscenesCount`, `levelsCount`, `levelCheckpointsCount[8]`, `yesNoQuitImage`, `soundDataSize`, `loadingImageSize`)
- Followed by aligned blocks: loading image (with palette), font (1024 × 16-byte glyphs), menu/options bitmaps, hint images, sound metadata

</details>

---

## 💾 Save state

`setup.cfg` is the engine's binary save file (212 bytes, `SetupConfig` in [`defs.h:55`](defs.h#L55)). It tracks per player slot:

- Per-level progress (highest checkpoint reached)
- Last-played level + checkpoint
- Watched-cutscenes bitmask
- 32 bytes of controls (16 bytes joystick, 8 bytes keyboard scancodes, 8 unused)
- Difficulty, stereo, volume, last-level

Up to **4 player slots**, selected via the menu.

---

## 🔧 Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `fatal error: 'endian.h' file not found` (macOS) | Use this fork's `intern.h` (has the `__APPLE__` branch). `git pull && make clean && make`. |
| First-run 4K is sluggish | Expected — every sprite/cutscene frame upscales once. Use `--hd-cache=./cache --prerender`; second run is instant. |
| Widescreen "borders are still black" | Use `--hd-wide` (not `--widescreen`). `--widescreen` is the legacy blur-stretch path. |
| Cutscenes display on screen during `--prerender` | Should not happen with this fork. If it does, you're running an older build — `make clean && make`. |
| Bound a key in the menu but in-game it does nothing | Custom keys are *additive*; defaults still work. Verify nothing else captured the key (e.g. macOS Cmd+Space, OS hotkeys). |
| Game freezes on the loading screen with `--prerender` | Should not happen post-fix. If it does, `make clean && make` and rerun. |
| `setup.cfg` corrupt or weird default keymap | Delete `setup.cfg`; the engine recreates a default one. |
| Audio is silent / distorted | Set `SDL_AUDIODRIVER=dummy` to disable, or `coreaudio` (macOS) / `pulseaudio` (Linux) to force a backend. |
| `Repository not found` when pushing to a fork | Make sure the GitHub fork exists at `https://github.com/<user>/hode` before `git push`. |

---

## 📋 Differences from upstream

A high-level diff vs `usineur/hode` master:

- **🎁 New modules** — `hd_compositor`, `sprite_upscaler`, `edge_smooth`, `automation_api`, plus four Python automation drivers.
- **🚩 New CLI flags** — `--hd`, `--hd-scale`, `--hd-wide`, `--hd-cache`, `--fullhd`, `--4k`, `--smooth`, `--automation`, `--prerender`.
- **📝 New INI keys** — `hd_mode`, `hd_scale`, `hd_widescreen`, `hd_cache`, `smooth_anim`, `automation_socket`.
- **🔑 Sprite cache key** changed from heap pointer to FNV-1a(content + dims + flags + palette hash). The upstream pointer-keyed disk cache effectively never produced cross-run hits.
- **📐 Widescreen window sizing** — `--hd-wide` now also sizes the SDL window 16:9 (otherwise the wide framebuffer was squashed back into a 4:3 window and gradient borders were invisible).
- **🎨 Palette-correct beginFrame** — `HdCompositor::beginFrame()` now expands the engine's 6-bit palette to 8-bit instead of treating it as already 8-bit, so border colours and HD sprite colours are not ~4× too dim.
- **🎬 PAF HD path** with disk cache and a fast-forward `prerender(num)` that skips audio / display / sleep.
- **🌊 Smooth animation** — 60 Hz interpolated render with 12.5 Hz logic.
- **🧰 Menu / input fixes** — OK / Cancel / Test sub-buttons, one-key-one-action enforcement, Space / Enter / Tab / Backspace / Cmd-Win bindable, default keys stay live alongside custom keys, font-pointer init hardened, edge state cleared on `waitForKeyPress`.
- **🍎 macOS portability** — `intern.h` selects `<libkern/OSByteOrder.h>` on `__APPLE__`.

---

## ⚠️ Known limitations

- The HD compositor only supports the **SDL2** backend. PSP / Wii are not HD-capable in this fork.
- `--4k` is internally **16× cropped to 15×** to fit the 4K UHD frame; visible artifacts are below 1 px.
- Cutscene prerender at 4K can use **multiple GB of disk** per cutscene. For the full PAF set, prefer `--hd` or `--fullhd`.
- The legacy `--widescreen` (blur-stretch) is preserved for parity but `--hd-wide` is preferred when the HD compositor is on.
- The automation API supports **one client at a time**.

---

## 🤝 Contributing

1. Fork the repo on GitHub
2. Branch off `master`:
   ```bash
   git checkout -b feature/<short-name>
   ```
3. Make changes; follow the existing style:
   - **Tabs** for indentation, K&R braces
   - `snake_case` for free functions, `lowerCamelCase` for methods, `_member` for fields
   - **Plain C++11** — no STL containers in hot paths; raw arrays + `malloc`/`free` are fine
4. Build cleanly with `-Wall -Wextra -Wpedantic` (no new warnings)
5. Commit with a descriptive message and open a PR against this fork or `usineur/hode`

---

## 👏 Credits

- **Original engine** — reverse-engineered by **Gregory Montoir** (cyx@users.sourceforge.net). See [`usineur/hode`](https://github.com/usineur/hode), upstream `README.txt`, and `CHANGES.txt` (preserved in this tree).
- **Original game** — *Heart of Darkness* by **Amazing Studio** (1998), published by Infogrames / Ocean.
- **xBRZ-style scaling** — based on Zenju's xBRZ algorithm — https://sourceforge.net/projects/xbrz/
- **MLAA edge smoothing** — classic anti-aliasing technique (Reshetov 2009 / Jimenez et al.)

External links:

- 🎮 [MobyGames: Heart of Darkness](https://www.mobygames.com/game/heart-of-darkness)
- 🌐 [heartofdarkness.ca](http://heartofdarkness.ca/) — fan resource
- 🐙 [usineur/hode upstream](https://github.com/usineur/hode)

---

## ⚖️ License & legal

The engine source is provided under the same terms as upstream `hode` (no explicit `LICENSE` file in upstream — treat it as "use at your own risk; please credit the original author"). The HD additions in this fork inherit the same terms.

The *Heart of Darkness* game data (`HOD.PAF`, `SETUP.DAT`, `*_HOD.*`) is **copyrighted by Amazing Studio / Infogrames** and is **not redistributed** here. You must own a legitimate copy of the original game to play.

---

<div align="center">

*Made with care for a 1998 cinematic platformer that still holds up.*

</div>
