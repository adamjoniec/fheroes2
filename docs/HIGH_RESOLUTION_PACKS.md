# Optional high-resolution graphics packs

Build with `-DENABLE_IMAGE=ON`. The normal default is unchanged: original artwork
and original game coordinates. Pack selection is currently a configuration-file
option; restart the game after changing it.

Add these lines to your active `fheroes2.cfg`:

```ini
graphics pack = D:/Work/codex/heroes2/upscaled/pixel
graphics scale = 2
screen scaling type = nearest
```

`graphics scale` accepts **1** (disabled), **2**, **3**, or **4**. Change `pixel` to
`smooth` for the fully neural variant. The path can point to the style directory
containing `x2/`, `x3/`, `x4/`, or directly to one scale directory. The latter must
match `graphics scale`. Paths are not quoted. Relative paths resolve against the
working directory; absolute paths are recommended.

For this workspace, the Windows launcher selects the scale and style and keeps
settings/saves in the repository's isolated `runtime-hd/profile` directory:

```powershell
# From the repository root, after building:
powershell -ExecutionPolicy Bypass -File script/windows/run_hd_pack.ps1 -Scale 2 -Style pixel
powershell -ExecutionPolicy Bypass -File script/windows/run_hd_pack.ps1 -Scale 3 -Style smooth
powershell -ExecutionPolicy Bypass -File script/windows/run_hd_pack.ps1 -Scale 4 -Style pixel
```

Optional launcher parameters: `-DataRoot` (contains `data/Heroes2.agg`), `-PackRoot`
(style directory), `-ExecutablePath`. Defaults match this workspace. The executable
needs SDL2, SDL2_image, SDL2_mixer and its compiler runtime DLLs next to it or on PATH,
plus the matching `files/data/resurrection.h2d`. Existing installed game builds
must be rebuilt to support these options.

## Pack format

```text
<pack>/x2/graphics/Heroes2/icn/PEASANT/0001.png
<pack>/x2/graphics/Heroes2/til/GROUND32/0000.png
<pack>/x2/graphics/Heroes2/bmp/ADVMBW01/0000.png
<pack>/x2/graphics/Heroes2x/icn/HEROES/0000.png
<pack>/x2/videos/Ivydes/0001.png
```

ICN/TIL/BMP frame names are zero-based, padded to four digits. Video names are
one-based, matching FFmpeg exports. Image dimensions must be exactly the original
frame dimensions multiplied by the selected scale. The original AGG headers remain
authoritative for offsets, frame numbering and animation definitions. The loader
uses the same base/expansion resource as the engine, including forced base-game
replacements; incomplete packs never substitute a different archive's artwork.
Missing, unreadable or incorrectly sized PNGs use the original resource.

Only PNGs are needed at runtime. The engine derives transparency, shadows,
highlights and cycling regions from the original resource's transform/index layers;
the extractor's approximate RGBA shadows are not treated as authoritative. The
original AGG and SMK resources remain required. Game animation order, timing, audio
and input coordinates are unchanged. Smacker frames use the current video palette.

## Rendering and current limits

Images keep their logical size and may carry a second image at the pack's scale.
Blits, clipping, copies/restorers, flips, cropping, resizing, alpha blending,
palette changes and common drawing operations update this detailed image alongside
the original. SDL uploads a texture at the detailed resolution, rather than reducing
the artwork back to the logical framebuffer. Window size grows to fit the monitor;
fullscreen and input still use the existing logical coordinate system.

This is an initial **palette-based** integration. Neural PNG colors are quantized
to the engine's 256-color palette, rather than retained as full RGB. Cycling areas
use original cycling indices to preserve their animation. Procedural images,
unsupported legacy direct pixel edits and missing frames fall back to original
pixels; this may leave some UI elements or engine-generated corrections at their
original detail. Raw writes discard the detailed layer to prevent stale graphics.
The feature increases CPU work and image-cache memory roughly with scale squared.
Start with 2x when assessing performance. Neural animation texture flicker remains
possible; the engine preserves alignment but does not add temporal stabilization.

## Validation

```powershell
cmake -S . -B build-hd -DENABLE_IMAGE=ON -DENABLE_HD_TESTS=ON
cmake --build build-hd
ctest --test-dir build-hd --output-on-failure
```

Tests cover original logical dimensions, subpixel detail, clipping/flips, cropping,
alpha/palette operations, copies/restorers, raw-write invalidation, and headless SDL
presentation. Optional `-DFHEROES2_HD_FIXTURES=D:/Work/codex/heroes2` also tests actual
2x/3x/4x creature PNGs, terrain, bitmap resources, expansion artwork, missing/wrong-size
fallback and Smacker frame reset/alignment against this workspace's packs. Tests
use SDL's dummy video driver and software renderer, without opening a game window.
