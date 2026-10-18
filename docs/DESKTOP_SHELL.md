# Suno desktop and supporting tools

The supplied desktop reference is used for its layout: a narrow top bar,
landscape wallpaper, searchable launcher, status panels and a bottom dock.
Suno keeps its own branding, QML icons and original code-drawn landscape.
There are no downloaded wallpapers, Apple assets or additional Qt modules.

This is the application's desktop shell. Turning it into the board's startup
experience still requires the Yocto compositor/session and service integration
described in [BOOT_HANDOFF.md](BOOT_HANDOFF.md). It does not install a Linux
desktop distribution or manage arbitrary external application windows.

## Registered tools

| Tool | Behavior |
| --- | --- |
| Live Electrodogram | Existing live workspace and acquisition controls |
| Acoustic Analytics Studio | Existing DSP and Automatic Mic Calibration workspaces |
| LAN-XI Measurement Studio | Paired Type 3676/3677 control, calibration, stimulus, live analysis, recording and review; approved-kit and hardware validation pending |
| Notepad | Named notes, save/reopen, text export, unsaved-change confirmation |
| Calculator | Arithmetic, parentheses, unary signs, percentages and recent calculations |
| Results | Local CSV/text export list and selectable, read-only previews |
| Desktop Settings | Persistent Dusk/Midnight wallpaper and 12/24-hour clock preferences |

The registry is `src/tools/ToolRegistry.qml`. The Workspace launcher shows
only diagnostic entries: Live Electrodogram, Acoustic Analytics Studio and
LAN-XI Measurement Studio.
Its search matches diagnostic titles and descriptions, and its count reflects
the matching diagnostics. The dock includes all seven registered tools, keeping
Notepad, Calculator, Results and Desktop Settings available there. The welcome
column, promotional text and category tabs have been removed. The Workspace
panel begins at the left margin, with the clock and status widgets on its right.
No browser, app store, media or terminal shortcuts are included. All registered
tools open inside this application.

## Windows and navigation

- Boot opens the desktop once. Returning from a workspace does not replay boot.
- Diagnostic workspaces load on first selection and retain their state in the
  desktop work area after that.
  **Full workspace** or **F10** expands them to the full application viewport.
  **Exit full screen** in the tool header or F10 restores the dock and top bar
  while keeping the tool open. In normal workspace view, this button becomes
  **Desktop** and returns to the launcher when acquisition is idle.
- Supporting tools share a floating window with a draggable title bar, Hide,
  Expand/Restore and Close. One supporting tool is shown at a time; switching
  or hiding it retains its current content in memory. Dock dots show opened tools.
- **Ctrl+Space** opens diagnostic search when acquisition is idle. Use Tab/arrows
  and Enter/Space in the launcher, or click/tap an icon.
- **Escape** closes the foreground utility (with an unsaved-note prompt when
  needed), cancels that prompt, or returns an idle diagnostic view to desktop.
- During acquisition, switching diagnostic tools and returning to desktop stay
  locked until stop acknowledgement. Supporting tools can be used without
  changing the acquisition mode. Input dialogs do not terminate acquisition.
- F11 keeps its existing Windows preview behavior. `--windowed` remains supported.

The desktop uses a 1920 x 1080 design reference and Qt-managed DPI. Its scaled
coordinate area fills the viewport and starts at the top-left corner; wider or
taller windows extend the available area. The top bar spans the top edge, and
the dock follows the bottom edge. The Workspace panel grows with that area.
The static Canvas wallpaper allocates pixels at the display's device pixel
ratio and repaints on size/theme changes. Text, icons and controls stay QML
items; the background does not use continuous animation or live blur.

Diagnostic workspaces fill their available area instead of centering a fixed
1920 x 1080 panel. Their tool header has no Suno branding and uses 60 design
pixels of height with 20-pixel side margins. The restore action sits in this
header, so no floating navigation button covers the tool. Live Electrodogram
anchors its electrode and history controls above the status line; the plot
expands between those controls and the capture toolbar. Acoustic workspace
tabs use smaller gaps. DSP monitor plots split the available height, while
microphone plots split the available width and extend to the bottom of the view.
Expanding/restoring retains tool settings, captured data and acquisition state.

## Note persistence and file access

Notepad saves UTF-8 text and a title inside versioned JSON note documents under
the platform's `Documents/SunoDiagnostics/Notes`. Internal UUID filenames keep
titles out of filesystem paths. Saving uses `QSaveFile`; a per-note `QLockFile`
serializes application instances, and a content hash detects a saved note that
changed since it was opened. Failed saves keep the draft and display the error.
This does not replace backups or guarantee recovery of unsaved edits after a crash.

The note list shows the 100 most recent valid notes. Titles allow 120 characters,
and note text is limited to 1 MiB in UTF-8. New/Open/Close/application-exit actions
offer Save, Discard or Cancel before replacing a dirty note. Hiding or switching
utilities retains the draft without saving it. Ctrl+S saves; Ctrl+N starts a new
note after resolving pending changes. The editor supports ordinary selection,
clipboard and undo/redo keyboard operations through Qt's plain-text editor.

**Export TXT** creates a separate uniquely named text file under
`Documents/SunoDiagnostics`. The Results tool lists the latest 1000 `.csv` and
`.txt` files in that folder. It reads at most 128 KiB for a preview and labels
truncation. Preview content is always plain text and never executed. Results
does not edit/delete files, browse arbitrary directories or launch external apps.

Preferences use `desktop.json` in Qt's application configuration directory.
Storage capacity comes from the volume containing the results folder (or its
nearest existing parent). Platform information comes from the current host;
Windows development builds are not labelled as running Yocto. Acquisition status
comes from the existing diagnostic session. No CPU, battery, network or weather
values are invented.

## Validation

Run the existing Node-only checks:

```powershell
node tests/analysis.test.cjs
node tests/live.test.cjs
node tests/desktop.test.cjs
node tests/tools.test.cjs
```

The 26 diagnostic checks, 18 live graph/settings checks, nine calculator checks
and 14 registry/lifecycle checks pass in the development workspace. Source references, local imports,
QML delimiter balance and the
transfer ZIP are checked separately. These are not substitutes for a Qt build.
No Qt compiler, linter or application runtime was executed on this laptop.

In the approved Qt environment, run CMake, build, run `all_qmllint`, and verify:

1. Boot, three diagnostic launcher entries, all seven tool dock entries, diagnostic
   search (including no matches for "Notepad"), empty results and keyboard
   navigation. Check 100/125/150/200% display scaling, 1920 x 1080, and taller
   and wider windows: the top bar must touch the top edge and the dock must
   stay inside the bottom edge. Confirm no welcome column or category tabs.
2. All three diagnostic routes, Desktop return, F10 expansion and acquisition locks.
   Open each diagnostic for the first time, edit settings/tab selection, return
   to Desktop and reopen it. Confirm the tool retains its state and still scales
   when expanded/restored. With an invalid entry in a test build, confirm the
   desktop still opens and the tool shows Retry and Show details. The details
   should include the component's error and source URL; retry must clear the
   prior details and must not create a second tool instance for diagnosis.
   In full workspace view, confirm the compact tool header has no Suno branding,
   the plot grows, and E1-E16, All/None, Earlier/Later/Follow latest and session
   feedback remain inside the view. Resize in both modes; check DSP Channel
   monitor and Automatic Mic Calibration graphs, tabs and profile controls.
   Open a supporting tool during a real capture and confirm capture continues.
   In LAN-XI, switch microphone/acoustic and amplifier profiles, return to the
   desktop and reopen it; the selection must remain. Opening a page must not
   contact a device or start output. Save/reload a profile and verify output is
   disarmed. Exercise the paired lifecycle and recovery checks in
   [LANXI_INTERFACE.md](LANXI_INTERFACE.md#validation).
3. Utility dragging, Hide/restore, Expand/Restore and focus after closing.
4. Notepad save/reopen including Unicode, Ctrl+S/Ctrl+N, text export and each
   Save/Discard/Cancel choice for New, Open, Close and application exit.
5. Minimize a dirty note, use Calculator, then return to the same draft.
   Attempt application close while that dirty note is hidden.
6. Unwritable/full results storage, an externally changed note, and two instances
   editing the same note. Failed saves must leave the editor content intact.
7. Results refresh, empty folders, long names, CSV/text selection and truncated
   previews; malformed note documents must not be overwritten automatically.
8. Preference persistence across restart and real capacity/status values.

Qt C++ compilation, native file operations and actual rendering/input behavior
remain unverified until those checks run. Profile the wallpaper texture and
text/plot workload on the board. A touch-only product also needs its approved
input-method/keyboard integration; no Qt Virtual Keyboard module was added.

Relevant Qt APIs: [TextEdit](https://doc.qt.io/qt-6/qml-qtquick-textedit.html),
[Canvas](https://doc.qt.io/qt-6/qml-qtquick-canvas.html),
[QSaveFile](https://doc.qt.io/qt-6/qsavefile.html), and
[QStorageInfo](https://doc.qt.io/qt-6/qstorageinfo.html).
