# Building and transferring the application

For Ubuntu development and FRDM i.MX95 SDK builds, use
[the Ubuntu transfer guide](../README_UBUNTU.md). Generate that package with
`python scripts/package_source.py --ubuntu`; it includes Linux line endings,
Unix shell permissions and SHA-256 checksums. Linux compilation and device
validation run in the approved Linux Qt environment.

Open the repository-root `CMakeLists.txt` in the approved Qt environment.
The C++ entry point is `src/app/main.cpp`; the QML entry point is
`src/app/qml/Main.qml`. Source manifests under `src/` are collected by
`cmake/ProjectSources.cmake`. Keep `src/` and `cmake/` together when copying.

After the structure migration, extract the complete source ZIP into a fresh
source folder and configure a fresh build directory using the matching Qt kit.
Do not copy a previous machine's build cache. The ZIP has one outer
`FRDM_IMX95_QT_DSW` folder; open the `CMakeLists.txt` inside that folder.
The executable and QML module remain `appFRDM_IMX95_QT_DSW` and
`FRDM_IMX95_QT_DSW` respectively. Application storage paths remain unchanged.

Generate the source ZIP locally with `python scripts/package_source.py`.
It verifies source references and produces
`build/transfer/FRDM_IMX95_QT_DSW-source.zip`, without build output or IDE state.
Python is only needed for these development scripts, not for building/running Qt.

For missing source errors, restore the file at the path CMake reports from the
complete current source package. `cmake -P cmake/VerifySources.cmake` validates
the source list without loading Qt. `python scripts/check_structure.py` also
checks import paths, Linux filename casing and registered tool entry points.
Cleaning cannot restore missing source files. Never hand-create `.qmltypes`.

### MinGW cannot open a generated `.cpp.obj.d` dependency file

The reported failure in `AutomaticMicCalibration_qml.cpp` occurs while the
compiler writes its dependency file. With the reported build directory, the
absolute `.obj.d` path is **260 characters**. That strongly indicates the
traditional Windows path limit, which includes the terminating null character.
The source folder restructure increased the generated filename: Qt includes
the relative QML path in its cache-generated C++ basename.

Use a short, fresh build directory in the approved Qt environment:

1. Open **Projects > Build Settings** for the selected Qt/MinGW kit.
2. Set **Build directory** to `E:\b\suno-debug` (use a new, empty location).
3. Select **Run CMake**, then **Build Project**. Confirm the command in Compile
   Output uses `--build E:/b/suno-debug`.

The source `CMakeLists.txt` stays at
`E:\Linux\Linux_Application\FRDM_IMX95_QT_DSW\CMakeLists.txt`.
The same dependency filename under the short build root is 188 characters,
leaving room for generated object and dependency files. Use a separate short
build directory for another kit or Release configuration, for example
`E:\b\suno-release`. Do not copy `CMakeCache.txt`, generated Makefiles or `.rcc`
files from the previous directory. No deletion of the old build is necessary.

If Qt Creator keeps using the previous build directory, verify the active build
configuration and its Build directory field before running CMake again. Cleaning
the long directory does not shorten its paths. Do not edit the generated C++
file or create an empty `.obj.d` file.

The project now sets a default `CMAKE_OBJECT_PATH_MAX` of 240 for Windows MinGW
and checks the longest generated QML dependency basename before loading Qt.
CMake can hash object directories, but a long build root plus a long basename
can still exceed the limit. The check reports that case with the short-directory
instructions; it does not disable QML cache generation or change resource paths.
Linux/Yocto builds do not use this Windows MinGW check.

Run `cmake -P tests/build_paths.test.cmake` to exercise the reported path and
the short-directory recovery without Qt or a compiler. Actual MinGW compilation
still needs verification in the approved environment. If the short-directory
build also fails, inspect that run's first error and the parent directory of the
reported dependency file; path length is not the only possible file-open failure.

References: [Windows path limits](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation),
[CMake object-path hashing](https://cmake.org/cmake/help/v3.30/variable/CMAKE_OBJECT_PATH_MAX.html),
and [Qt Creator build settings](https://doc.qt.io/qtcreator/creator-build-settings-cmake.html).

### Import reports "Error while reading typeinfo files"

`import FRDM_IMX95_QT_DSW` matches the module URI in `CMakeLists.txt` and
`src/app/main.cpp`. It exposes `DiagnosticPlot`, `DiagnosticFiles` and `DesktopFiles`.
Keep this import. Qt Creator needs the generated `.qmltypes` file to describe
those C++ types; cleaning removes generated files and an interrupted build can
leave that information unavailable. The full diagnostic identifies the unreadable
file and, for a parsing error, its line number.

In the approved Qt environment, open this project's `CMakeLists.txt`, select
the matching Qt/MinGW kit and run CMake. Then generate the type information
using the kit's terminal, from the project source folder. An ordinary PowerShell
window does not automatically inherit the kit's environment. For the compiler
paths shown in the reported build log, initialize that session and check that
the C++ compiler's preprocessing step works before building:

```powershell
$frdmMingwBin = 'E:\Linux\Qt\Tools\mingw1310_64\bin'
$frdmQtBin = 'E:\Linux\Qt\6.12.0\mingw_64\bin'
$env:Path = "$frdmMingwBin;$frdmQtBin;E:\Linux\Qt\Tools\CMake_64\bin;$env:Path"

'#include <string>' | & "$frdmMingwBin\g++.exe" -std=c++17 -x c++ -E - -o NUL
if ($LASTEXITCODE -ne 0) { throw 'MinGW preprocessing failed. Resolve the compiler environment before building.' }

& 'E:\Linux\Qt\Tools\CMake_64\bin\cmake.exe' --build 'E:/b/suno-debug' --target appFRDM_IMX95_QT_DSW_qmltyperegistration
```

This changes PATH only for the current PowerShell session. The compiler's bin
directory also needs to be available to its child programs; an absolute path
to `g++.exe` alone does not initialize that environment. Adjust these directories
to the compiler and Qt installation selected in the active kit.

If AutoMoc fails while generating `moc_predefs.h` and its command is `g++ -dM -E`,
the failure occurs in compiler preprocessing before QML type registration.
An empty error-output section does not identify its cause. If the probe above
fails, repeat it with `-v` and capture the diagnostic and exit code:

```powershell
'#include <string>' | & "$frdmMingwBin\g++.exe" -v -std=c++17 -x c++ -E - -o NUL
$frdmCompilerExit = $LASTEXITCODE
Write-Output "MinGW preprocessing exit code: $frdmCompilerExit"
```

Compare with a build inside Qt Creator using the same kit. If preprocessing
works only there, compare the kit's Build Environment with the terminal PATH.
If it fails there too, inspect the detailed compiler error and the selected
toolchain installation. Keep compiler predefinitions enabled so AutoMoc sees
the compiler's actual definitions.

The reported cache selects MinGW 13.1. Qt's published Qt 6.12 Windows support
table lists MinGW-w64 15.1. Confirm which compiler was supplied for this Qt SDK;
a custom-built SDK may differ. If correcting the selected compiler, configure
a fresh build directory with that kit rather than reusing the old compiler cache.
See [Qt's Windows compiler configuration](https://doc.qt.io/qt-6.12/windows.html)
and [command-line environment setup](https://doc.qt.io/qt-6.12/windows-building.html).

Use your active kit's build directory if its name differs. The expected output
in the default configuration is:

```text
<build-directory>/FRDM_IMX95_QT_DSW/appFRDM_IMX95_QT_DSW.qmltypes
```

After that target succeeds, build the application normally. If the editor still
shows the import error, use **Tools > QML/JS > Reset Code Model**, or reopen the
project. If generation fails, use its first error in Compile Output to diagnose
the failure. If the file exists but cannot be parsed, capture the complete
typeinfo diagnostic, including its filename and line number, and the Qt Creator
version. Do not create a placeholder `.qmltypes` or copy one from another kit.

Qt documents [automatic type-information generation](https://doc.qt.io/qt-6/qt-add-qml-module.html#auto-generating-qmldir-and-typeinfo-files)
and [refreshing Qt Creator's QML code model](https://doc.qt.io/qtcreator/creator-qml-modules-with-plugins.html).

### Live Electrodogram shows "Unable to open"

The supplied Application Output identifies the page-load error:

```text
LiveElectrodogram.qml:214:22: Cannot assign to non-existent property "onEndPressed"
```

Qt Quick `Keys` has no dedicated `homePressed` or `endPressed` signals. The
history slider now handles `Qt.Key_Home` and `Qt.Key_End` in `Keys.onPressed`,
accepting handled events and passing other keys onward. This fixes the reported
component-definition failure. See [Qt Quick Keys](https://doc.qt.io/qt-6/qml-qtquick-keys.html).

The separate wallpaper warning is addressed by renaming the custom string
property to `wallpaperStyle`, avoiding the inherited `Item.palette` property.
The existing saved wallpaper preference remains compatible.

Transfer the complete updated source and rebuild in the approved Qt environment.
The generated QML cache is build output; edit the source QML, not its generated
C++ files. Check that the rebuild succeeds before running the executable.
Opening Live Electrodogram should then show the plot and settings, even while
acquisition is disconnected. The DirectX debugger messages in the supplied log
are separate from the explicit QML property error that prevented this page loading.

The tool host now offers **Retry** and **Show details** on a load failure.
Details include the registered source URL and component error where available.
If Qt fails during instance creation without a component-definition error,
Application Output is still needed. The probe never instantiates a second tool.

## Build and run on Windows

Build in an environment with a company-approved Qt 6.10+ SDK and its matching
compiler. Qt Creator and an AI client are not required for this command-line
workflow. The Qt licensing route and distribution obligations must be settled
through your company; having a Visual Studio license does not settle those.
The splash, desktop and supporting tools use the existing Qt Quick dependency. No additional Qt
modules, fonts, icon packages or third-party dependencies were added.
The diagnostic plots use `QQuickPaintedItem` from that same module; Qt Charts,
Qt Graphs, Qt SerialPort and Java libraries are not required by this update.

Configure a fresh build directory. The existing local cache may refer to a
previous installation on `E:/Linux/Qt`; it is not portable between machines
or kits.

For command-line builds, use the compiler environment matching the Qt kit
(for example, a Visual Studio developer terminal for an MSVC kit, or the
matching MinGW tools on PATH). Replace `<Qt-kit-directory>` with the actual Qt
kit directory:

```powershell
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Qt-kit-directory>"
cmake --build build/desktop --parallel
cmake --build build/desktop --target all_qmllint
cmake --install build/desktop --prefix "$PWD/build/desktop/install"
.\build\desktop\install\bin\appFRDM_IMX95_QT_DSW.exe
```

The Windows install step deploys Qt DLLs, the platform plugin, and imported
QML modules alongside the application. Use an absolute install prefix as in
the example above; Qt's deployment scripts require it. The executable now
starts fullscreen; append `--windowed` for a desktop preview.

Generated `build/` directories and Qt Creator/Visual Studio settings are
excluded from Git.

LAN-XI captured-window audio is an optional dependency, disabled by default.
In the approved Qt environment, add `-DSUNO_LANXI_AUDIO=ON` only when its
approved Qt Multimedia development/runtime package is available. CMake requires
that component when enabled; it does not download it. Measurement acquisition
and analysis do not depend on audio. See [workbench operation](LANXI_WORKBENCH.md).

## Checking pixelation on Windows

1. Run a freshly rebuilt executable on the display's native resolution.
2. Check text and rounded edges at 100%, 125%, 150%, and 200% Windows scaling.
   If multiple monitors are available, move the app between monitors with
   different scale factors.
3. Keep Windows DPI compatibility overrides disabled for the executable.
   System bitmap scaling can blur an otherwise DPI-aware application.
4. Check the application's run environment for old overrides such as
   `QT_ENABLE_HIGHDPI_SCALING=0`, `QT_SCALE_FACTOR`,
   `QT_SCREEN_SCALE_FACTORS`, or `QT_FONT_DPI`. Normal use should follow the
   operating system's scale settings.
5. Set `QSG_INFO=1` in the run environment when diagnosing graphics issues.
   Qt will report its scene-graph backend in application output.

Check resizing, fullscreen entry/exit, and progress bar alignment. Rendering
changes must also be verified on the actual Windows display: an offscreen
render cannot reproduce Windows
compatibility settings, compositor scaling, or a monitor's output mode.

