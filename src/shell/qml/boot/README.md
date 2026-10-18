# Application boot screen

This folder is the application's reusable boot screen source. It belongs in
version control, outside `build/`. The main CMake target includes
`BootScreen.qml` and `assets/BootScreen.png`; the preview is a development tool.

The source was brought into this project from the supplied `QT files/Linux`
design. This folder is now the copy to edit for the application. The external
design folder and the earlier staging files are not runtime dependencies.

## Appearance

The original PNG is 1920 x 1080 and includes the logo and "System booting"
text. The separate progress bar is positioned at (688, 628) and measures
544 x 8 at native size. Its track is translucent white and its fill is dark
purple. The screen and bar share centered 16:9 geometry, including when the
window's aspect ratio changes. The bar remains QML geometry; the supplied
logo/text are raster artwork.

[Static composition at 25%](../../../../docs/boot-screen-preview.png)

## Progress API

```qml
BootScreen {
    anchors.fill: parent
    progress: startupController.progress
}
```

`startupController` is an example of a future backend, not an object supplied
by this folder. Its property must notify QML when progress changes.

| Property | Meaning |
| --- | --- |
| `progress: -1` | Unknown progress; moving indicator (explicit opt-in) |
| `progress: 0` | Empty bar (component default) |
| `progress: 0.25` | 25% complete |
| `progress: 1` | Full bar |
| `animationsEnabled: false` | Disable motion for static review |

Values above 1 are clamped to 1. Negative and non-finite values use the
indeterminate indicator. This component never advances startup work on a
timer, exits the application, or decides when the workflow is ready.

`src/app/qml/Main.qml` supplies a one-time visual startup
sequence: `startupProgress` starts at 0 and fills to 1 over four seconds.
`startupDurationMs` controls the duration. When `startupReady` is also true,
the completed bar holds for 400 ms and fades out to the home launcher over
240 ms. Resizing, fullscreen changes and tool selection do not replay boot.
During the progress sequence the
component's value smoothing is disabled, since the startup animation already
interpolates the fraction every frame.

This is simulated progress until real initialization tasks exist. To connect
the actual startup controller later, set `simulateStartup: false` in `src/app/qml/Main.qml`
and bind both `startupProgress` and `startupReady` to your controller. Real
readiness must come from successful initialization; progress alone is not
enough to enter home when the simulation is disabled. The standalone boot
component remains independent of navigation.

Use the standalone preview below to review specific progress values and the
indeterminate animation.

## Preview in the approved Qt environment

From the repository root, if the installed Qt kit includes these tools:

```text
qml src/shell/qml/boot/Preview.qml
qmllint src/shell/qml/boot/BootScreen.qml src/shell/qml/boot/Preview.qml
```

The preview plays the same four-second fill once. Keys: **R** = replay from 0%,
**P** = 25%, **I** = unknown progress, **0** = empty, **1** = full,
**F11** = fullscreen, **Escape** = leave fullscreen. Selecting a specific value
stops the preview sequence, so it cannot overwrite the selection.
The normal application's startup is independent of these preview controls.

Use the company's approved Qt SDK and licensing route. The standalone QML
preview needs neither Visual Studio nor an AI client; compiling the complete
application still needs the compiler matching its Qt kit. Qt Creator is not
required. No new dependency is needed for this component.

Qt tools were not executed for this integration on the company laptop.
Runtime and board rendering still need validation in the approved environment.

## Later Linux integration

Keep `BootScreen.qml` and `assets/BootScreen.png` together when reusing this
folder; the image URL is relative to the QML file. Include both in the Linux
application's resources using the same relative paths.

The component starts after Qt and its graphics platform start. Showing the
same design earlier during Linux boot needs a BSP-specific splash service
and handover configuration. See [boot handover notes](../../../../docs/BOOT_HANDOFF.md).

References: [Qt image scaling](https://doc.qt.io/qt-6/qml-qtquick-image.html),
[QML runtime tool](https://doc.qt.io/qt-6/qtquick-qml-runtime.html),
[Qt animation loop count](https://doc.qt.io/qt-6/qml-qtquick-animation.html#loops-prop).
