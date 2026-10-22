# Compile motion for Windows

`robohero_motion` supports both **Linux-to-Windows MinGW-w64 cross-compilation** (producing the standalone `robohero_motion-setup-<version>.exe` NSIS installer directly on Linux `x86_64`) and **native Windows MSVC compilation** (`windows-msvc` CMake preset).

---

## 1. Primary Workflow: Cross-Compile Windows NSIS Installer on Linux (`x86_64`)

On a Linux `x86_64` development workstation, the top-level and `app/motion` Makefiles cross-compile `robohero_motion.exe` using `x86_64-w64-mingw32-g++-posix`, bundle the MinGW Qt6, OpenGL, and Mosquitto runtime DLLs, and package everything into an NSIS setup installer (`robohero_motion-setup-<version>.exe`) with integrated `selfupdate` support.

### Build Commands (from Linux)

```bash
# Build both Linux native binary and Windows NSIS installer from app/motion:
make -C app/motion all

# Or build only the Windows binary and NSIS setup installer:
make -C app/motion windows

# Or build from the repository root:
make windows
```

### Output Artifacts

- **NSIS Setup Installer**: `app/motion/build/windows/robohero_motion-setup-<version>.exe`
- **Cross-Compiled Executable**: `app/motion/build/windows/robohero_motion.exe`
- **Self-Contained Staging Bundle**: `app/motion/build/windows/dist/`

---

## 2. Native Windows Compilation (`windows-msvc` Preset)

First-time native build of `robohero_motion` from source on Windows. Later configures reuse
downloaded Qt binaries and take seconds.

Do **not** add Qt as a vcpkg port. Qt is an official prebuilt download
managed via `aqtinstall` (see `cmake/FetchPrebuiltWindows.cmake`).
vcpkg is only used for small libraries: `mosquitto`, `libconfig`, `pugixml`.

### Prerequisites

- **Visual Studio 2026** (Community or Build Tools) with the
  *Desktop development with C++* workload. The `windows-msvc` preset
  uses the `Visual Studio 18 2026` generator.
- **CMake 3.20+** on `PATH` (`cmake --version`).
- **Python 3** on `PATH`. The first configure uses it to install `aqtinstall`
  and download official Qt binaries.
- **Git**, and network access for the first configure (Qt and vcpkg packages).

### Steps

From a Developer PowerShell (or any shell with `cmake` and MSVC toolchain):

```powershell
cd <repo>\robohero
git submodule update --init --recursive

cd third_party\vcpkg
.\bootstrap-vcpkg.bat
cd ..\..

cd app\motion
cmake --preset windows-msvc
cmake --build --preset windows-msvc
```

The first `cmake --preset windows-msvc`:

- Builds the small vcpkg ports (`mosquitto`, `libconfig`, `pugixml`)
- Downloads official **Qt 6.8.3** `win64_msvc2022_64` (`qtbase`) via `aqtinstall`
- Configures CMake with embedded resources for `model/robohero.urdf` and `model/calibration.json`

`windeployqt` runs automatically as a post-build step to deploy necessary Qt DLLs.

### Run

```powershell
app\motion\build\windows-msvc\Release\robohero_motion.exe
```
