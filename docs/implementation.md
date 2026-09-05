# Implementation and validation

## Components

- `src/compat.c`, `forward.s`, `version.def`, `payload.h`: a Windows x64 proxy
  for the system version API. Patching runs only in the main CapCut process under
  Wine and checks the VECreator PE timestamp, image size, and all original resource
  bytes. The embedded QML resource is replaced in memory. Export forwarding remains
  available in other processes. The experimental software-rendering hook is off.
- `src/xcopy_compat.c`: native X11 drawable-depth tracking, 24/32-bit conversion,
  and dynamically resolved XRender format correction.
- `src/gc_tracking.inc`: destination-compatible graphics contexts that preserve
  supported clipping, operation, plane mask, and clip origin. Unknown contexts and
  unsupported bitmap clip masks pass through.
- `src/present_sync.inc`: native Vulkan proc-address interception, device feature
  and swapchain lifetime tracking, and completion waits for DXVK's existing
  presentation IDs before Wine copies the rendered X11 surface.
- `extras/kwin/capcut-overlay-instant`: KDE overlay animation suppression.

The native companion must load through `LD_PRELOAD`: a Windows DLL alone cannot
interpose the Linux-side Xlib and Vulkan calls. The drawable cache currently
assumes one X server per process. The library is x86-64, not a 32-bit companion.

## Presentation fix

Wine queues a host Vulkan presentation, then copies an offscreen X11 surface into
the visible window. A queued presentation may not yet be complete, so the copy
can show the previous frame until another app update. The companion waits using
`vkWaitForPresentKHR` only when the app already enabled both required features and
extensions and supplied a nonzero present ID.

The wait has a 100 ms timeout per eligible swapchain. The original present result
and caller data are preserved; unknown, unsupported, retired, or failed targets
pass through. Timeouts are logged and cannot guarantee a current copy. Waiting
for presentation may reduce throughput. No additional frames, device-wide idle
waits, or new Vulkan feature requests are injected.

References: [Wine's host presentation path](https://github.com/ValveSoftware/wine/blob/bleeding-edge/dlls/win32u/vulkan.c),
[Vulkan presentation completion](https://docs.vulkan.org/refpages/latest/refpages/source/vkWaitForPresentKHR.html).

## Validation of v0.4

The original prebuilt binaries were tested with CapCut 1.5.0.230, GE-Proton11-6,
Bottles' DXVK 3.1, KDE Wayland/XWayland, and an NVIDIA RTX 3070 Laptop GPU.

- X11 depth conversion, XRender formats, graphics-context depth, clipping,
  XOR, plane masks, clip reset, and unrelated `RTLD_NEXT` lookup tests passed.
- Presentation-state tests cover wait ordering, feature gating, proc lookup,
  IDs, mixed per-swapchain results, fatal failures, timeout/error handling,
  retirement, destruction, handle reuse, and unchanged caller results/data.
- The state test also passed AddressSanitizer and UndefinedBehaviorSanitizer.
- The real GPU test detected 1 stale copy in 12 isolated frames with the fix
  disabled and 0 in 12 with it enabled. The baseline race is timing-dependent.
- Live editing on a duplicate project showed a single-character replacement
  immediately in the text field and timeline label; undo restored the original.
  Fullscreen/normal transitions and 2400×1400 / 2600×1500 resizes completed.
- No X11 errors, device-loss errors, or presentation wait failures were recorded
  in that short live validation. The final native binary was verified loaded
  after restarting CapCut.

These checks are narrower than full application qualification. The installer,
every export path, different GPUs/desktops, and long editing sessions were not
validated by this suite.

## Upstream runner patches

The tested GE-Proton11-6 Wine runner already ships
[layered-overlay-wine.patch](https://github.com/GloriousEggroll/proton-ge-custom/blob/GE-Proton11-6/patches/game-patches/layered-overlay-wine.patch).
Its source tree also contains a
[DXVK overlay patch](https://github.com/GloriousEggroll/proton-ge-custom/blob/GE-Proton11-6/patches/dxvk/layered-overlay-dxvk.patch),
but the tested bottle uses Bottles' DXVK 3.1 files. Building this shim does not
apply either upstream patch or replace the runner's Wine/DXVK binaries.

## Package provenance

The checked-in prebuilt package contains the original tested v0.4 native binary
and unchanged v0.1 Windows proxy. The public packaging pass replaces the original
machine-specific launcher/documentation with portable instructions and includes
the maintained source tree. Binary SHA-256 values are in the package manifest.
A fresh Windows proxy build may differ because of PE linker timestamps; the
source logic is unchanged. This project does not include the CapCut installer,
full application DLLs, project media, or reverse-engineering databases.
