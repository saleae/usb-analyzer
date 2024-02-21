# Saleae USB Analyzer

Saleae USB Analyzer

## Getting Started

The following documentation describes how to build this analyzer locally. For more detailed information about the Analyzer SDK, debugging, CI builds, and more, check out the readme in the Sample Analyzer repository.

https://github.com/saleae/SampleAnalyzer

### MacOS

Dependencies:

- XCode with command line tools
- CMake 3.13+
- git

Install command line tools after XCode is installed:

```
xcode-select --install
```

Then open XCode, open Preferences from the main menu, go to locations, and select the only option under 'Command line tools'.

Install CMake on MacOS:

1. Download the binary distribution for MacOS, `cmake-*-Darwin-x86_64.dmg`
2. Install the usual way by dragging into applications.
3. Open a terminal and run the following:

```
/Applications/CMake.app/Contents/bin/cmake-gui --install
```

_Note: Errors may occur if older versions of CMake are installed._

Build the analyzer:

```
mkdir build
cd build
cmake ..
cmake --build .
```

### Ubuntu 18.04+

Dependencies:

- CMake 3.13+
- gcc 4.8+
- git

Misc dependencies:

```
sudo apt-get install build-essential
```

Build the analyzer:

```
mkdir build
cd build
cmake ..
cmake --build .
```

### Windows

Dependencies:

- Visual Studio 2019
- CMake 3.13+
- git

**Visual Studio 2019**

_Note - newer and older versions of Visual Studio are likely to work._

Setup options:

- Workloads > Desktop & Mobile > "Desktop development with C++"

Note - if CMake has any problems with the MSVC compiler, it's likely a component is missing.

**CMake**

Download and install the latest CMake release here.
https://cmake.org/download/

**git**

Download and install git here.
https://git-scm.com/

Build the analyzer:

```
mkdir build
cd build
cmake .. -A x64
```

Then, open the newly created solution file located here: `build\usb_analyzer.sln`

Optionally, build from the command line without opening Visual Studio:

```
cmake --build .
```

The built analyzer DLLs will be located here:

`build\Analyzers\Debug`

`build\Analyzers\Release`

For debug and release builds, respectively.

