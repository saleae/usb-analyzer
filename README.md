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

**Warning**
This is still WIP, and not fully complete,  So far my main attention has been to using the USB Decode level "packets"
mode, I have now started to do some stuff in the "Control Transfers" 

In earlier versions of this I created lots of different property names, in later pass I converted a lot of them
to generic: "name" and "name2" as it makes the output table more compact
  
### Frame Type: `"addrendp"`

Address and End Point

| Property | Type | Description |
| :--- | :--- | :--- |
| `value` | bytes | usb addr |
| `value2` | bytes | endpoint |

### Frame Type: `"crc16"`
| Property | Type | Description |
| :--- | :--- | :--- |
| `crc` | bytes | crc |
| `ccrc` | bytes | calculated crd |

### Frame Type: `"crc5"`
| Property | Type | Description |
| :--- | :--- | :--- |
| `value` | bytes | crc |
| `value2` | bytes | calculated crd |

### Frame Type: `"data"`

one data byte

| Property | Type | Description |
| :--- | :--- | :--- |
| `out` | byte | one byte of data |
### Frame Type: `"EOP"`

End of Packet

### Frame Type: `"frame"`

Start of frame

| Property | Type | Description |
| :--- | :--- | :--- |
| `value` | bytes | 16 bit frame number |

### Frame Type: `"pid"`

Packet type id - mapped to string names

| Property | Type | Description |
| :--- | :--- | :--- |
| `value` | string | packet id  | 
|       |       |   IN |
|       |       |   OUT |
|       |       |   SOF |
|       |       |   SETUP |
|       |       |   DATA0 |
|       |       |   DATA1 |

### Frame Type: `"protocol"`

(Control Transfer mode ) - WIP

Secondary parts from PID of SETUP

one data byte

| Property | Type | Description |
| :--- | :--- | :--- |
| `bmRequestType` | byte | request type |
| `bRequest` | byte | The request |
| `desc` | string | descriptive name of request |
| `wValue` | bytes | word value|
| `wIndex` | bytes | word index |
| `wLength` | bytes | how many bytes of data |

### Frame Type: `"presult"`

(Control Transfer mode ) - WIP

The data returned from a setup request

one data byte

| Property | Type | Description |
| :--- | :--- | :--- |
| `name` | string | Logical field name |
| `size` | byte | how many bytes  |
| `type` | byte | type (not sure yet) |
| `value` | bytes | value|
### Frame Type: `"sync"`

End of Packet
