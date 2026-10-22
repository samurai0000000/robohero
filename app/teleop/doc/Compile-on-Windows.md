# Compile teleop for Windows

`robohero_teleop` supports both **Linux-to-Windows MinGW-w64 cross-compilation** (producing the standalone `robohero_teleop-setup-<version>.exe` NSIS installer directly on Linux `x86_64`) and **native Windows MSVC compilation** (`windows-msvc` CMake preset).

---

## 1. Primary Workflow: Cross-Compile Windows NSIS Installer on Linux (`x86_64`)

On a Linux `x86_64` development workstation, the top-level and `app/teleop` Makefiles cross-compile `robohero_teleop.exe` using `x86_64-w64-mingw32-g++-posix`, bundle the MinGW Qt6, OpenCV, ONNX Runtime DirectML, and Mosquitto runtime DLLs, and package everything into a signed-manifest NSIS setup installer (`robohero_teleop-setup-<version>.exe`) with integrated `selfupdate` support.

### Build Commands (from Linux)

```bash
# Build both Linux native binary and Windows NSIS installer from app/teleop:
make -C app/teleop all

# Or build only the Windows binary and NSIS setup installer:
make -C app/teleop windows

# Or build from the repository root:
make windows
```

### Output Artifacts

- **NSIS Setup Installer**: `app/teleop/build/windows/robohero_teleop-setup-<version>.exe`
- **Cross-Compiled Executable**: `app/teleop/build/windows/robohero_teleop.exe`
- **Self-Contained Staging Bundle**: `app/teleop/build/windows/dist/`

---

## 2. Native Windows Compilation (`windows-msvc` Preset)

First-time native build of `robohero_teleop` from source on Windows. Later configures reuse
downloaded Qt/OpenCV binaries and should take seconds.

Do **not** add Qt, OpenCV, or ONNX Runtime as vcpkg ports. Those are
official prebuilt downloads (see `cmake/FetchPrebuiltWindows.cmake`).
vcpkg is only for small libraries: `mosquitto`, `libconfig`, `pugixml`.

### Prerequisites

- **Visual Studio 2026** (Community or Build Tools) with the
  *Desktop development with C++* workload. The `windows-msvc` preset
  uses the `Visual Studio 18 2026` generator.
- **CMake 3.20+** on `PATH` (`cmake --version`).
- **Python 3** on `PATH`. The first configure uses it to install
  `aqtinstall` and download official Qt binaries.
- **Git**, and network access for the first configure (Qt, OpenCV,
  ONNX Runtime DirectML, vcpkg packages).

### Steps

From a Developer PowerShell (or any shell that sees `cmake` and MSVC):

```powershell
cd <repo>\robohero
git submodule update --init --recursive

cd third_party\vcpkg
.\bootstrap-vcpkg.bat
cd ..\..

cd app\teleop
cmake --preset windows-msvc
cmake --build --preset windows-msvc
```

The first `cmake --preset windows-msvc`:

- Builds the small vcpkg ports (mosquitto, libconfig, pugixml)
- Downloads official **Qt 6.8.3** `win64_msvc2022_64` (qtbase)
- Downloads official **OpenCV 4.12.0** Windows pack
- Downloads **ONNX Runtime DirectML** from NuGet

`windeployqt` runs as a post-build step. You do not need to run it by
hand.

### Run

```
app\teleop\build\windows-msvc\Release\robohero_teleop.exe
```

Start it from that `Release` folder so Qt, OpenCV, and ONNX DLLs next
to the exe are found.
