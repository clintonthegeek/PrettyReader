# Session Management & Single Instance Design

## Problems

1. **Tab close bug**: Closing a tab doesn't clean up sidebar state. No special handling for closing the last tab.
2. **No session restore**: Open files aren't remembered across launches.
3. **No single instance**: Opening a file when PrettyReader is already running creates a second window/process instead of adding a tab.

## Requirements

- Closing the last tab quits the application (with session save).
- Closing any tab clears stale sidebar content.
- Launching with no arguments restores the previous session (open files, active tab, zoom, scroll).
- Launching with file arguments opens those files fresh (no session restore).
- Second launch sends files to the running instance instead of opening a new window.
- Files sent to a running instance are added as new tabs alongside existing ones.
- Single-instance: KDBusService on Linux, QLocalServer/QLocalSocket on Windows, compile-time CMake selection.

## Design

### 1. Tab Close Fix

Replace the direct `tabCloseRequested → removeTab` connection with `MainWindow::onTabCloseRequested(int index)`:
1. Remove the tab widget at `index`.
2. If tabs remain: trigger sidebar updates for the now-current tab (TOC, etc.).
3. If no tabs remain: call `close()` → `closeEvent()` → save session → quit.

### 2. Session Save/Restore

**Save** (in existing `saveSession()`, called from `closeEvent`):
- `Session/OpenFiles`: ordered `QStringList` of file paths, one per tab
- `Session/ActiveTab`: `int` index of focused tab
- `Session/ZoomLevels`: parallel `QList<int>` of zoom percentage per tab
- `Session/ScrollPositions`: parallel `QList<int>` of vertical scroll value per tab
- Existing sidebar/theme state saving unchanged

**Restore** (in existing `restoreSession()`):
- Only if no command-line files were provided.
- Read file list, reopen each via `openFile()`.
- Set active tab index.
- Deferred restore of zoom/scroll via `QTimer::singleShot(0, ...)` so views are laid out first.

**Startup flow** (main.cpp):
- If command-line files provided → open those files, skip session restore.
- If no files → `restoreSession()` reopens previous session.

### 3. Single Instance

**Compile-time selection via CMake:**
- `find_package(KF6DBusAddons QUIET)` → define `HAVE_KDBUSSERVICE` if found.
- Link `KF6::DBusAddons` conditionally.

**Linux (KDBusService):**
- `KDBusService(KDBusService::Unique)` in main.cpp before MainWindow creation.
- Connect `activateRequested(QStringList, QString)` → `MainWindow::activateWithFiles()`.
- Second launch: args forwarded via DBus, second process exits.

**Windows fallback (QLocalServer/QLocalSocket):**
- `SingleInstanceGuard` class in `src/app/`.
- Named pipe: `"PrettyReader-{username}"`.
- First instance: creates `QLocalServer`, listens for connections.
- Second instance: connects, sends file paths as newline-delimited UTF-8, exits.
- First instance: receives paths → `MainWindow::activateWithFiles()`.

**Shared interface:**
- `MainWindow::activateWithFiles(const QStringList &paths)` — opens each file as a new tab, raises/activates the window via `raise()` + `activateWindow()`.

## Files Changed

- `CMakeLists.txt` — add KF6DBusAddons optional dependency, define HAVE_KDBUSSERVICE
- `src/app/main.cpp` — single-instance setup, conditional session restore
- `src/app/mainwindow.h` — new slots: `onTabCloseRequested`, `activateWithFiles`
- `src/app/mainwindow.cpp` — tab close slot, session save/restore of open files + per-tab state
- `src/app/singleinstanceguard.h/cpp` — new: QLocalServer fallback for Windows
