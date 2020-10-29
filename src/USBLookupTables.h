#ifndef USB_LOOKUP_TABLES_H
#define USB_LOOKUP_TABLES_H

#include <LogicPublicTypes.h>

#include "USBEnums.h"

std::string GetPIDName( USB_PID pid );
const char* GetDescriptorName( U8 descriptor );
const char* GetRequestName( U8 request );
const char* GetHIDRequestName( U8 request );
const char* GetHIDCountryName( U8 countryCode );
const char* GetCDCRequestName( U8 request );
const char* GetCDCEthFeatureSelectorName( U8 feature );
const char* GetCDCATMFeatureSelectorName( U8 feature );
const char* GetCDCDescriptorSubtypeName( U8 subtype );
const char* GetLangName( U16 langID );
const char* GetVendorName( U16 vendorID );
const char* GetUSBClassName( U8 classCode );

std::string GetHIDUsagePageName( U16 usagePage );
std::string GetHIDUsageName( U16 usagePage, U16 usageID );

#endif // USB_LOOKUP_TABLES_H