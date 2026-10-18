# Ubuntu transfer and FRDM i.MX95 testing

This is the complete **source project** for the existing Suno application:
boot screen, desktop, Live Electrodogram, Acoustic Analytics Studio, LAN-XI
Measurement Studio and supporting tools. Build it using your approved Linux Qt
installation. The archive includes source, assets, tests and documentation;
Windows executables, build caches, IDE settings and Qt SDK/runtime binaries are
excluded. Packaging does not change the project's Qt licensing arrangement.

## 1. Extract and open the project

Copy the ZIP and its `.sha256` file to Ubuntu. From that directory:

```bash
sha256sum -c FRDM_IMX95_QT_DSW-ubuntu-source.zip.sha256
unzip FRDM_IMX95_QT_DSW-ubuntu-source.zip
cd FRDM_IMX95_QT_DSW
sha256sum -c SOURCE_SHA256SUMS
```

Extract into a fresh directory, not over an older source copy. The ZIP has one
outer `FRDM_IMX95_QT_DSW` directory. All portable text uses LF line endings, and
shell helpers have Unix executable permissions. `bash scripts/build_ubuntu.sh`
also works if your archive manager does not restore executable permissions.

**Qt Creator:** open the root `CMakeLists.txt`, select your **Linux desktop Qt
kit**, and choose a fresh build directory such as `build/ubuntu-debug`.
Build, then Run with `--windowed` in the application's command-line arguments
for the initial desktop test. Remove that argument for fullscreen.
Do not select a Windows/MinGW kit or reuse its build directory.

The project currently requires:

| Component | Requirement |
| --- | --- |
| Qt | 6.10 or newer, built for the machine on which the executable will run |
| Qt modules | Quick/QML and Network, with their development tools and runtime plugins |
| Compiler | Matching Linux GCC or Clang with C++17 support |
| Build | CMake 3.21+ and Make or Ninja (also meet your Qt SDK's requirements) |
| Optional audio | Qt Multimedia only when `SUNO_LANXI_AUDIO=ON` |
| Development checks | Python 3.9+, Node.js for JavaScript tests; not application runtime dependencies |

Use the Qt version already approved for your environment. A BSP containing an
older Qt release does not satisfy this project's current 6.10 minimum; its SDK
and image must be aligned before board compilation. Qt Creator is optional.

## 2. Build and run on Ubuntu from a terminal

Use a normal Ubuntu development terminal with the matching compiler, CMake and
Make/Ninja on PATH. Replace the example with your installed Qt **kit directory**
(the directory containing that kit's `lib`, `bin` and `include`):

```bash
bash scripts/build_ubuntu.sh "$HOME/Qt/6.12.0/gcc_64"
./build/ubuntu-debug/appFRDM_IMX95_QT_DSW --windowed
```

The helper configures Debug, builds with two parallel jobs, and downloads or
installs nothing. Set `SUNO_BUILD_JOBS=4` before the command to change parallelism.
For a different build directory, supply it as the second argument. The helper
is for a native Linux build; use the SDK procedure below for cross-compilation.

Equivalent manual commands, also suitable for choosing Release or enabling audio:

```bash
cmake -S . -B build/ubuntu-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/gcc_64" \
  -DSUNO_LANXI_AUDIO=OFF
cmake --build build/ubuntu-debug --parallel 2
cmake --build build/ubuntu-debug --target all_qmllint
./build/ubuntu-debug/appFRDM_IMX95_QT_DSW --windowed
```

Start with the executable in its build directory while using the installed Qt
environment. The Linux `install` target installs the application executable;
it **does not bundle Qt libraries or plugins**. The destination machine must
provide the matching runtime. See [Qt package selection](https://doc.qt.io/qt-6/cmake-making-qt-available.html)
and [Linux runtime deployment](https://doc.qt.io/qt-6/linux-deployment.html).

## 3. Run the included checks on Ubuntu

These native C++ checks do not need Qt or physical LAN-XI hardware:

```bash
cmake -P cmake/VerifySources.cmake
python3 scripts/check_structure.py
cmake -S tests/lanxi -B build/tests-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests-linux --parallel 2
ctest --test-dir build/tests-linux --output-on-failure
node --test tests/analysis.test.cjs tests/live.test.cjs tests/desktop.test.cjs tests/tools.test.cjs tests/lanxi/session.test.cjs
```

The Qt backend integration suite uses loopback HTTP/TCP fixtures rather than
your instruments. It requires the installed Qt SDK and Python 3:

```bash
cmake -S tests/lanxi -B build/tests-linux-qt \
  -DCMAKE_BUILD_TYPE=Debug -DLANXI_QT_TESTS=ON \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/gcc_64"
cmake --build build/tests-linux-qt --parallel 2
ctest --test-dir build/tests-linux-qt --output-on-failure
```

Use Debug for these checks: several native checks use C++ assertions. Run this
suite natively on Ubuntu; an ARM64 test executable cannot run on an x86 PC
without a separately configured emulator.

## 4. Build for the FRDM i.MX95

An Ubuntu desktop build is for the development machine. For the FRDM board,
produce a Linux **AArch64/ARM64** build matching its image, Qt runtime, libc,
graphics drivers and compiler ABI. Even an Ubuntu ARM64 executable is not
automatically compatible with a different Yocto image.

If Ubuntu and the matching Qt development kit are installed directly on the
board, the native build above can run there. If Ubuntu is your development PC
and the board runs Yocto, use the board image's NXP/Yocto SDK, including target
Qt development files and the matching host Qt tools.

In a **new terminal**, source the SDK's environment setup file. The following
paths are placeholders; replace them with the SDK actually supplied for your
board image. Do not use the Ubuntu desktop Qt kit as the target Qt installation.

```bash
source "/path/to/sdk/environment-setup-<aarch64-target>"
```

Prefer the target SDK's `qt-cmake` wrapper when it is provided and configured by
that SDK. Otherwise use the SDK-supplied CMake toolchain file:

```bash
cmake -S . -B build/frdm-imx95 \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/sdk/board-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DSUNO_LANXI_AUDIO=OFF
cmake --build build/frdm-imx95 --parallel 2
DESTDIR="$PWD/build/frdm-stage" cmake --install build/frdm-imx95
file build/frdm-stage/usr/bin/appFRDM_IMX95_QT_DSW
```

Confirm that `file` reports an AArch64 ELF executable. The SDK must resolve
target Qt libraries and runnable host tools such as `moc`, `rcc` and
`qmlcachegen`. If it requests `QT_HOST_PATH`, use the matching host Qt supplied
or specified by that SDK. Never reuse `build/ubuntu-debug` for this build.
These are SDK integration templates; the BSP release and toolchain paths have
not yet been supplied or tested. [Qt cross-compilation](https://doc.qt.io/qt-6/cmake-build-on-cmdline.html#cross-compiling)
and [Yocto SDK environment setup](https://docs.yoctoproject.org/sdk-manual/working-projects.html)
describe the corresponding environment requirements.

After verifying the architecture, a board transfer archive can be made with:

```bash
tar -C build/frdm-stage -czf build/suno-frdm-imx95-app.tar.gz usr
```

Transfer it using your existing board deployment workflow. Its contents are
the application under `usr/bin`; runtime dependencies must come from the board
image or its approved matching packages. Test manually before enabling any
startup service. This source ZIP does not flash the board, change network
settings, or install an autostart service.

## 5. Display, storage and first run

Configure the board display to **1920 x 1080 landscape** using its existing BSP
display configuration. Fullscreen uses that active mode; the application does
not switch the HDMI/display mode itself.

Run in the board's configured graphical session, using the platform supported
by its Qt installation. For a running Wayland compositor, an example is:

```bash
QT_QPA_PLATFORM=wayland /usr/bin/appFRDM_IMX95_QT_DSW --fullscreen
```

For a BSP configured for direct EGLFS display ownership, use `eglfs` instead.
Wayland needs the compositor's session environment and permissions. EGLFS needs
the device's graphics/input access and control of the display. Do not force
either platform merely because it appears in this guide. [Qt embedded Linux platforms](https://doc.qt.io/qt-6/embedded-linux.html)
explains these requirements.

The runtime user needs a writable home, application configuration directory
and Documents directory. Notes/results normally live under
`Documents/SunoDiagnostics`; the actual Documents path follows Qt's platform
settings. LAN-XI uses its `LanXi` subdirectory. An absolute
`SUNO_LANXI_DATA_DIR` can redirect LAN-XI storage only; it does not redirect
Notepad, desktop settings or the other diagnostic tools. Existing user data is
not included in this source package.

Check these after the first successful build:

1. Splash reaches 100% and the desktop opens. Progress is currently simulated;
   early Linux boot splash integration is a separate task.
2. Open and reopen all three diagnostic tools. Check plots, settings, tool
   fullscreen/restore and 1920 x 1080 layout.
3. Save and reopen a note and a result using the intended runtime account.
4. Run the loopback backend suite before physical LAN-XI capture/generator tests.
5. Verify the board's LAN connection to the Type 3676 and Type 3677, then follow
   the acceptance procedures in [LANXI_WORKBENCH.md](docs/LANXI_WORKBENCH.md).

For a startup failure, retain the first QML or plugin error and the process exit
code. On Ubuntu, from a terminal in the source directory:

```bash
set -o pipefail
QT_DEBUG_PLUGINS=1 QSG_INFO=1 \
  ./build/ubuntu-debug/appFRDM_IMX95_QT_DSW --windowed 2>&1 | tee build/ubuntu-run.log
```

Qt compilation, the graphical interface, hardware communication and FRDM
deployment still require validation in your Linux environment. The source
archive's integrity/path checks do not certify those results. The earlier
Windows abnormal-termination report has not established a root cause.
M7 acquisition transport and Linux startup/boot handover are not implemented
by this transfer; see [BOOT_HANDOFF.md](docs/BOOT_HANDOFF.md).
