# Boot splash to application workflow

Target sequence for the FRDM i.MX95:

1. The bootloader and kernel initialize the board and display hardware.
2. An early Linux splash service displays `src/shell/qml/boot/assets/BootScreen.png` with its own
   live progress bar while system services start.
3. The graphics platform and diagnostic application start. The application's
   first screen embeds `src/shell/qml/boot/BootScreen.qml` using the same artwork and position.
4. The application finishes device/service initialization, then replaces its
   splash with the diagnostic home when its startup controller reports ready.
5. The user selects Live Electrodogram, Acoustic Analytics Studio or a future
   diagnostic tool from this home. Tool screens will share the same device UI.

The application currently displays the step 3 screen, fills its bar from 0%
to 100% once over four seconds, holds for 400 ms, then fades into the home
launcher. This does not measure Linux or application initialization.
`src/app/qml/Main.qml` exposes both `startupProgress` and `startupReady` for real tasks;
set `simulateStartup: false` before binding both to the startup controller.
Home is revealed once both are ready and is not reset by selecting tools.
Tool selection now opens the native Live Electrodogram view or Acoustic
Analytics Studio with DSP Calibration and Automatic Mic Calibration.
Device/service initialization, the acquisition adapter and board handover
are not implemented. Capture controls require advertised backend capabilities.
No Linux splash service or board
handover is configured. A splash before Linux userspace starts is a separate
bootloader/kernel change.

The reusable source is kept in `src/shell/qml/boot/` in this repository, outside the
build directory. Its relative asset paths remain portable for later Linux
application development. No files from the separate design export directory
are required at runtime.

## Device home and the custom OS experience

The application now starts fullscreen and owns its boot screen, home, tool
selection and navigation. Its UI uses the same purple/blue visual language,
with a system clock and scalable QML tool icons. Additional tools are registered
in `src/tools/ToolRegistry.qml`; see [home integration](LAUNCHER.md).

During Linux integration, make this process the device's main UI using the
selected BSP's startup service and display stack. Configure the supported
Wayland compositor/session without desktop panels or window decorations, or
use EGLFS if the BSP supports that route. Startup ordering, display ownership,
input, recovery after a process failure and returning home from tools need
board testing. The application alone does not replace or configure a Linux
desktop. No generic service or compositor config has been installed while
the BSP and display stack remain unknown.

The default fullscreen Linux mode does not provide F11/Escape shortcuts to
leave the device UI. `--windowed` explicitly enables desktop preview behavior;
Windows also retains F11 for development. This is a presentation choice,
not an OS security or kiosk-lockdown mechanism.
[Qt embedded Linux display platforms](https://doc.qt.io/qt-6/embedded-linux.html)

## Use the boot service already selected by your BSP

Yocto's `splash` image feature enables a boot splash and defaults to `psplash`.
That default is not proof that it is present in your particular NXP image.
Confirm the actual image recipe, init system and display support first.
[Yocto image features](https://docs.yoctoproject.org/current/ref-manual/features.html#image-features)

If your approved image already uses psplash, customize that recipe in your
own layer. Its image and progress layout need to be adapted: replacing only
the logo does not automatically reproduce the bar placement in the reference.
If your image uses another splash service, use that service's theme mechanism.
Verify its display backend against the i.MX95 BSP before selecting a package.

The OpenEmbedded psplash recipe lists `GPL-2.0-or-later`. Any use or modification
must follow your company's approved licensing process and applicable license
obligations. No psplash, Plymouth, Qt tool, or other package was installed or
added to a Yocto image by this update.
[OpenEmbedded psplash recipe](https://layers.openembedded.org/layerindex/recipe/421476/)

## Progress and handover

- Connect the Linux splash to actual boot/service milestones. Use an activity
  indicator when no meaningful percentage is available.
- Bind the Qt component to application initialization progress. If you want
  one continuous percentage through both stages, define shared milestones
  and pass the current state during handover; the QML component alone cannot
  measure boot completion.
- Check service ordering and ownership of the display. A direct-display
  splash may need to release the display before EGLFS or the compositor can
  acquire it. Under Wayland, the compositor must be available before Qt starts.
- Coordinate splash exit with graphics readiness and the first application
  frame where the selected backend permits it. Measure the transition on the
  board; using the same image does not by itself guarantee a flicker-free swap.
- Keep the Qt event loop responsive during initialization. Enter the main
  workflow only when required services/devices are ready; handle a startup
  failure explicitly rather than displaying 100% indefinitely.

## Information needed to finish board integration

- NXP BSP release, Yocto release and image recipe.
- Existing splash package/configuration and init system (for example systemd).
- Qt runtime version and supported display path (for example Wayland/Weston
  or EGLFS), including the compositor service name if applicable.
- Application executable/service name, startup milestones and the first
  workflow screen to show after initialization.

With these, the boot theme, image recipe changes and service handover can be
implemented against the actual BSP and validated at 1920 x 1080.

[Qt embedded Linux graphics](https://doc.qt.io/qt-6/embedded-linux.html)
