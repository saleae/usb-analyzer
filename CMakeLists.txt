cmake_minimum_required (VERSION 3.11)
project(usb_analyzer)



# enable generation of compile_commands.json, helpful for IDEs to locate include files.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# custom CMake Modules are located in the cmake directory.
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(ExternalAnalyzerSDK)

set(SOURCES 
src/USBAnalyzer.cpp
src/USBAnalyzer.h
src/USBAnalyzerResults.cpp
src/USBAnalyzerResults.h
src/USBAnalyzerSettings.cpp
src/USBAnalyzerSettings.h
src/USBControlTransfers.cpp
src/USBControlTransfers.h
src/USBEnums.h
src/USBLookupTables.cpp
src/USBLookupTables.h
src/USBSimulationDataGenerator.cpp
src/USBSimulationDataGenerator.h
src/USBTypes.cpp
src/USBTypes.h
)

add_analyzer_plugin(usb_analyzer SOURCES ${SOURCES})
