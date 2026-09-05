# capcut-linux

A compatibility shim for running **CapCut 1.5.0 on Linux through Bottles**.
The supported Windows build is **64-bit CapCut 1.5.0.230**, specifically.
Other 1.5.0 builds and newer CapCut versions are not supported by the current
binary patch. The proxy checks the application image and resource bytes before
patching; renaming another version's directory to `1.5.0.230` will not work.

The shim fixes editor-overlay behavior, selected Wine/X11 drawing errors, and
the window appearing one frame behind. It consists of a Windows `version.dll`
proxy and a Linux `libcapcut_xcopy.so` companion. Both are required for the full fix.

**[Download the prebuilt v0.4 package](https://github.com/ios7jbpro/capcut-linux/releases/tag/v0.4)** ·
[Build from source](#build-from-source) · [Troubleshooting](#troubleshooting)

## Requirements

| Component | Tested configuration |
| --- | --- |
| CapCut | **1.5.0.230, Windows x64**, with original `VECreator.dll` |
| Launcher | **Bottles**, Flatpak package `com.usebottles.bottles` |
| Bottle | 64-bit, Windows 10 |
| Wine runner | **GE-Proton11-6** (`GE-Proton11-6-x86_64` in the tested bottle) |
| Direct3D | **DXVK 3.1**, enabled in Bottles |
| Display | **Wine X11 driver**, including XWayland on a Wayland desktop |
| Tested desktop/GPU | KDE Plasma Wayland; NVIDIA RTX 3070 Laptop GPU |
| Native companion runtime | x86-64 glibc **2.34+**, X11/XRender and Vulkan runtime libraries |

Use the pinned versions to reproduce the working setup. Other desktops and GPUs
have not been validated. The stale-frame fix additionally needs the driver and
DXVK to enable `VK_KHR_present_id` and `VK_KHR_present_wait`; the companion skips
that fix if these features are unavailable.

GE-Proton11-6 already contains the Wine overlay support used here; this project
does not install or rebuild the runner. See its
[release](https://github.com/GloriousEggroll/proton-ge-custom/releases/tag/GE-Proton11-6)
and [overlay patch](https://github.com/GloriousEggroll/proton-ge-custom/blob/GE-Proton11-6/patches/game-patches/layered-overlay-wine.patch).

## Install the prebuilt package

### 1. Prepare Bottles and CapCut

Install [Bottles](https://docs.usebottles.com/getting-started/installation):

```bash
flatpak install flathub com.usebottles.bottles
```

Install GE-Proton11-6 from the release linked above. The tested installation
stores its extracted `GE-Proton11-6-x86_64` directory under
`~/.local/share/Steam/compatibilitytools.d/`. Give Bottles access to that directory
and restart Bottles so it can discover the runner:

```bash
flatpak override --user \
  --filesystem="$HOME/.local/share/Steam/compatibilitytools.d:ro" \
  com.usebottles.bottles
```

Use Bottles' runner selection to select **GE-Proton11-6**. If it is not listed,
check the extraction location and Bottles' Steam integration permissions; do
not silently substitute the default runner. See
[Bottles runners](https://docs.usebottles.com/components/runners) and
[Steam integration permissions](https://docs.usebottles.com/flatpak/cant-enable-steam-proton-manager).

Create a dedicated **64-bit** bottle named `CapCut`, set Windows to **Windows 10**,
and enable **DXVK 3.1**. Keep Wine's native Wayland driver **disabled**; this setup
uses X11/XWayland even when your Linux desktop uses Wayland.

Run your **CapCut 1.5.0.230** Windows installer using the bottle's **Run executable**
action. CapCut and its installer are not included in this repository. Keep this
version installed; an application update can invalidate the shim. The tested
bottle also has Bottles' `arial32`, `times32`, `courie32`, `mono`, and `gecko`
dependencies installed.

Add the installed `CapCut.exe` to the bottle's Programs list and name it `CapCut`.
Start it once to confirm the installation, then **close CapCut completely** before
installing the shim. Back up projects before experimenting.

### 2. Extract and verify the package

Download the linked ZIP, extract it, and open a terminal in its extracted
`CapCut-Wine-Compat-v0.4` directory:

```bash
sha256sum -c SHA256SUMS
```

The package contains the two prebuilt libraries, this guide, source, a build
script, and the optional KDE effect. Compilers are not needed for installation.

### 3. Copy the libraries

Set `capcut_dir` to the **Linux path to the directory containing `CapCut.exe` and
`VECreator.dll`**. Use Bottles' file browser to find it. The exact installation
path varies; do not use a Windows `C:\...` path in these commands.

```bash
capcut_dir='/absolute/path/to/CapCut/1.5.0.230'
shim_dir="$HOME/.local/share/capcut-linux"

sha256sum "$capcut_dir/VECreator.dll"
```

The expected original `VECreator.dll` SHA-256 is:

```text
c94175f5348a68d506a5f73d5b2249ce4ebc715c83bd16886848d5ebfae0d64d
```

If it differs, stop and check the CapCut build or restore its original DLL.
Do not install over an unknown `version.dll`; back it up first if one exists.
When upgrading this shim, back up the old companion too.

```bash
mkdir -p "$shim_dir"
if [ -e "$capcut_dir/version.dll" ]; then
    cp --backup=numbered "$capcut_dir/version.dll" "$capcut_dir/version.dll.backup"
fi
if [ -e "$shim_dir/libcapcut_xcopy.so" ]; then
    cp --backup=numbered "$shim_dir/libcapcut_xcopy.so" "$shim_dir/libcapcut_xcopy.so.backup"
fi
cp version.dll "$capcut_dir/version.dll"
cp libcapcut_xcopy.so "$shim_dir/libcapcut_xcopy.so"
printf 'LD_PRELOAD=%s/libcapcut_xcopy.so\n' "$shim_dir"
```

The Linux companion goes in a separate directory so a CapCut installation path
containing spaces is fine. The **companion's own absolute path must not contain
spaces or colons**, because `LD_PRELOAD` treats those as separators. Choose a
different directory if your home path contains either character.

For Flatpak, expose the companion and, if CapCut is installed outside Bottles'
private directory, its installation directory:

```bash
flatpak override --user --filesystem="$shim_dir:ro" com.usebottles.bottles
# Only needed for an external CapCut installation:
flatpak override --user --filesystem="$capcut_dir" com.usebottles.bottles
```

Give Bottles access to your media/project folders as needed using the same
[directory-permission mechanism](https://docs.usebottles.com/flatpak/expose-directories).

### 4. Set the bottle environment

In the CapCut bottle's settings, add these **environment variables**:

| Name | Value |
| --- | --- |
| `CAPCUT_XCOPY_COMPAT` | `1` |
| `LD_PRELOAD` | The full Linux path printed above, e.g. `/home/alice/.local/share/capcut-linux/libcapcut_xcopy.so` |
| `WINEDLLOVERRIDES` | `version=n,b` |
| `QSG_RENDER_LOOP` | `basic` |
| `QML_DISABLE_DISK_CACHE` | `1` |
| `WINE_LAYERED_OVERLAY_ALPHA` | `1` |
| `WINE_LAYERED_OVERLAY_INPUT_SHAPE` | `0` |

Enter literal values in the GUI, without shell quotes. `LD_PRELOAD` needs the
expanded absolute path, **not** `$HOME`, `~`, or a Windows path. Keep these settings
scoped to the CapCut bottle. If you already have unrelated DLL overrides, append
`;version=n,b` to them instead of replacing them.

Setting `WINE_LAYERED_OVERLAY_ALPHA=1` in the bottle environment replaces the
tested setup's program wrapper `WINE_LAYERED_OVERLAY_ALPHA=1 %command%`; either
way, the variable must reach CapCut. Leave the experimental
`CAPCUT_WINE_SOFTWARE` mode disabled: it caused rendering defects in testing.

### 5. KDE overlay animations

On KDE Plasma, install the included effect to suppress animations on CapCut's
editor overlays:

```bash
kpackagetool6 --type KWin/Effect --install extras/kwin/capcut-overlay-instant
```

Then open **System Settings → Desktop Effects** and enable
**CapCut overlay: instant transitions**. Use `--upgrade` instead of `--install`
if it is already installed. This is separate from the DLLs; other desktop
environments cannot use this KWin effect.

### 6. Launch

Open **Bottles → CapCut bottle → Programs → CapCut**. Always launch through this
bottle so the required environment is applied.

For a launch that also saves diagnostic output:

```bash
bash scripts/launch-with-log.sh CapCut CapCut
```

The first argument is your bottle name; the second is its saved program name.
Logs go to `${XDG_STATE_HOME:-$HOME/.local/state}/capcut-linux/`. The launcher
does not install or configure the shim. It uses the settings you saved in Bottles.

## Build from source

Build on **x86-64 Linux**, using Bash, GCC, Clang, LLD, LLVM's `llvm-dlltool`,
GNU binutils, X11/XRender headers, and Vulkan headers. No Windows SDK or MSVC
installation is needed. `src/payload.h` already contains the version-specific
patch data; the build does not require a copy of CapCut.

Typical package names (distribution releases may use versioned LLVM names):

```bash
# Debian / Ubuntu
sudo apt install build-essential clang lld llvm binutils libx11-dev libxrender-dev libvulkan-dev

# Arch Linux
sudo pacman -S --needed base-devel clang lld llvm libx11 libxrender vulkan-headers
```

Make sure `clang`, `lld-link`, `llvm-dlltool`, `gcc`, and `objdump` are on `PATH`.
Clone and build:

```bash
git clone https://github.com/ios7jbpro/capcut-linux.git
cd capcut-linux
./build.sh
```

Outputs are **`src/build/version.dll`** and **`src/build/libcapcut_xcopy.so`**.
Install them using the same instructions above, substituting those paths for the
two files from the prebuilt ZIP. Building does not install anything into Bottles.

The default build runs the X11 depth/conversion/clipping tests, dynamic-symbol
caller-scope test, and deterministic presentation-state tests. It needs a working
X11 `DISPLAY`; XWayland is fine. For a headless build or an additional GPU test:

```bash
CAPCUT_SKIP_TESTS=1 ./build.sh          # compile without running tests
xvfb-run -a ./build.sh                # optional Xvfb, installed separately
CAPCUT_TEST_PRESENT_GPU=1 ./build.sh   # real display + Vulkan driver required
```

The GPU test opens a small window and checks that isolated presented frames can
be copied immediately. It needs present-ID and present-wait support; it prefers
an NVIDIA GPU when available. Do not use Xvfb for that optional hardware test.
Keep optimization enabled: the `dlsym` wrapper requires a tail call to preserve
unrelated `RTLD_NEXT` lookups. The build saves its disassembly and tests that behavior.

To package your build, run `python3 scripts/package.py src/build`. The resulting
ZIP contains the build's libraries, source, instructions, and checksums.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No proxy log | `version.dll` must be beside the actual `CapCut.exe`; check `WINEDLLOVERRIDES` and launch through Bottles. |
| `REFUSED: unsupported VECreator image` / `overlay bytes differ` | Install exactly 1.5.0.230 and restore the original `VECreator.dll`; the shim refuses unknown builds. |
| `LD_PRELOAD` cannot load the companion | Check its expanded Linux path, Flatpak permissions, x86-64 architecture, glibc version, and absence of spaces/colons in its path. |
| A 32-bit helper reports `wrong ELF class` | The companion is x86-64. Confirm that the main CapCut process is x64 and loads it successfully. |
| Window still one frame behind | Look for `CAPCUT-PRESENT enabled completion wait` and `completed` in the launch log; verify DXVK 3.1 and GPU feature support. |
| Overlay blocks clicks or appears opaque | Check the pinned runner, X11 driver, `WINE_LAYERED_OVERLAY_ALPHA=1`, and `WINE_LAYERED_OVERLAY_INPUT_SHAPE=0`. |
| KDE overlay flashes during open/close | Install and enable the included KWin effect. |

The Windows proxy writes **`capcut-wine-compat.log` beside CapCut.exe**. Its log
still says **v0.1**, because that proxy has not changed; the native companion is
v0.4. Native corrections are logged as `CAPCUT-XCOPY`, `CAPCUT-XRENDER`,
`CAPCUT-XGC`, and `CAPCUT-PRESENT`.

To disable only the new presentation wait, set `CAPCUT_PRESENT_COMPAT=0` in the
bottle and restart CapCut. To remove the shim, close CapCut, remove its
`LD_PRELOAD` value and the `version=n,b` override, restore any backed-up libraries,
and remove the shim's environment variables. Preserve unrelated overrides and
pre-existing settings. The original `VECreator.dll` is never rewritten on disk.

## Status and internals

This is an experimental, version-specific compatibility project, not an official
CapCut Linux port. Local testing covered text editing, immediate updates, undo,
resizing, and isolated GPU presentation. Long-session stability, other hardware,
and all editing/export features have not been established. Transparent letterbox
areas can still expose desktop content.

See [implementation and validation notes](docs/implementation.md). This repository
contains localized patch data from the tested application resource; CapCut itself
and its resources retain their original ownership.
