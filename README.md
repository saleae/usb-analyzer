# Saleae USB Analyzer

Saleae USB Analyzer

## Getting Started

### MacOS

Dependencies:
- XCode with command line tools
- CMake 3.13+

Installing command line tools after XCode is installed:
```
xcode-select --install
```

Then open XCode, open Preferences from the main menu, go to locations, and select the only option under 'Command line tools'.

Installing CMake on MacOS:

1. Download the binary distribution for MacOS, `cmake-*-Darwin-x86_64.dmg`
2. Install the usual way by dragging into applications.
3. Open a terminal and run the following:
```
/Applications/CMake.app/Contents/bin/cmake-gui --install
```
*Note: Errors may occur if older versions of CMake are installed.*

Building the analyzer:
```
mkdir build
cd build
cmake ..
cmake --build .
```

### Ubuntu 16.04

Dependencies:
- CMake 3.13+
- gcc 4.8+

Misc dependencies:

```
sudo apt-get install build-essential
```

Building the analyzer:
```
mkdir build
cd build
cmake ..
cmake --build .
```

### Windows

Dependencies:
- Visual Studio 2015 Update 3
- CMake 3.13+

**Visual Studio 2015**

*Note - newer versions of Visual Studio should be fine.*

Setup options:
- Programming Languages > Visual C++ > select all sub-components.

Note - if CMake has any problems with the MSVC compiler, it's likely a component is missing.

**CMake**

Download and install the latest CMake release here.
https://cmake.org/download/

Building the analyzer:
```
mkdir build
cd build
cmake ..
```

Then, open the newly created solution file located here: `build\usb_analyzer.sln`


## Output Frame Format
  
### Frame Type: `"addrendp"`

Address and End Point

| Property | Type | Description |
| :--- | :--- | :--- |
| `addr` | bytes | usb addr |
| `endpoint` | bytes | endpoint |

### Frame Type: `"crc16"`
| Property | Type | Description |
| :--- | :--- | :--- |
| `crc` | bytes | crc |
| `ccrc` | bytes | calculated crd |

### Frame Type: `"crc5"`
| Property | Type | Description |
| :--- | :--- | :--- |
| `crc` | bytes | crc |
| `ccrc` | bytes | calculated crd |

### Frame Type: `"EOP"`

End of Packet

### Frame Type: `"frame"`

Start of frame

| Property | Type | Description |
| :--- | :--- | :--- |
| `framenum` | bytes | 16 bit frame number |

### Frame Type: `"pid"`

Packet type id

| Property | Type | Description |
| :--- | :--- | :--- |
| `pid` | bytes | packet id  | 
|       |       |   0x69 = IN |
|       |       |   0xE1 = OUT |
|       |       |   0xA5 = SOF |
|       |       |   0x2D = SETUP |
|       |       |   0xC3 = DATA0 |
|       |       |   0x4B = DATA1 |

### Frame Type: `"result"`

one data byte

| Property | Type | Description |
| :--- | :--- | :--- |
| `out` | byte | one byte of data |

### Frame Type: `"sync"`

End of Packet
