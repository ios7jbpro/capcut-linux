# Prebuilt package

[Download CapCut-Wine-Compat-v0.4.zip](CapCut-Wine-Compat-v0.4.zip?raw=1).
Use **Windows x64 CapCut 1.5.0.230 and Bottles** with the configuration in the
[installation guide](../README.md).

The archive includes `version.dll`, `libcapcut_xcopy.so`, portable instructions,
source, a configurable logging launcher, and the KDE effect. It does not include
CapCut or a Wine runner. The libraries are the same binaries used for the local
v0.4 validation; documentation and packaging have been prepared for other users.

Verify the downloaded archive using the adjacent `.zip.sha256` file, then verify
its contents with `sha256sum -c SHA256SUMS` from inside the extracted directory.

Recreate a package from your own build with `python3 scripts/package.py src/build`
from the repository root. Python 3's standard library is sufficient.
