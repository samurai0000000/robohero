# Compile teleop on Windows

First-time build of `robohero_teleop` from source. Later configures reuse
downloaded Qt/OpenCV binaries and should take seconds.

Do **not** add Qt, OpenCV, or ONNX Runtime as vcpkg ports. Those are
official prebuilt downloads (see `cmake/FetchPrebuiltWindows.cmake`).
vcpkg is only for small libraries: `mosquitto`, `libconfig`, `pugixml`.

## Prerequisites

- **Visual Studio 2026** (Community or Build Tools) with the
  *Desktop development with C++* workload. The `windows-msvc` preset
  uses the `Visual Studio 18 2026` generator.
- **CMake 3.20+** on `PATH` (`cmake --version`).
- **Python 3** on `PATH`. The first configure uses it to install
  `aqtinstall` and download official Qt binaries.
- **Git**, and network access for the first configure (Qt, OpenCV,
  ONNX Runtime DirectML, vcpkg packages).

## Steps

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

## Run

```
app\teleop\build\windows-msvc\Release\robohero_teleop.exe
```

Start it from that `Release` folder so Qt, OpenCV, and ONNX DLLs next
to the exe are found.
