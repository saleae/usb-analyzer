#include <iostream>
#include <fstream>
#include <algorithm>

#include <stdio.h>

#include <AnalyzerHelpers.h>

#include "USBAnalyzerResults.h"
#include "USBAnalyzer.h"
#include "USBAnalyzerSettings.h"
#include "USBLookupTables.h"

std::string GetCollectionData(U8 data)
{
	switch (data)
	{
	case 0x00:		return "Physical";
	case 0x01:		return "Application";
	case 0x02:		return "Logical";
	case 0x03:		return "Report";
	case 0x04:		return "Named Array";
	case 0x05:		return "Usage Switch";
	case 0x06:		return "Usage Modifier";
	}

	if (data >= 0x07  &&  data <= 0x7F)
		return "Reserved";

	return "Vendor-defined";
}

std::string GetInputData(U8 data1, U8 data2)
{
	std::string retVal;

	retVal += data1 & 0x01 ? "Constant" : "Data";					retVal += ',';
	retVal += data1 & 0x02 ? "Variable" : "Array";					retVal += ',';
	retVal += data1 & 0x04 ? "Relative" : "Absolute";				retVal += ',';
	retVal += data1 & 0x08 ? "Wrap" : "No wrap";					retVal += ',';
	retVal += data1 & 0x10 ? "Non Linear" : "Linear";				retVal += ',';
	retVal += data1 & 0x20 ? "No Preferred" : "Preferred State";	retVal += ',';
	retVal += data1 & 0x40 ? "Null State" : "No Null position";		retVal += ',';

	retVal += data2 & 0x01 ? "Buffered Bytes" : "Bit Field";

	return retVal;
}

std::string GetOutputAndFeatureData(U8 data1, U8 data2)
{
	std::string retVal;

	retVal += data1 & 0x01 ? "Constant" : "Data";					retVal += ',';
	retVal += data1 & 0x02 ? "Variable" : "Array";					retVal += ',';
	retVal += data1 & 0x04 ? "Relative" : "Absolute";				retVal += ',';
	retVal += data1 & 0x08 ? "Wrap" : "No wrap";					retVal += ',';
	retVal += data1 & 0x10 ? "Non Linear" : "Linear";				retVal += ',';
	retVal += data1 & 0x20 ? "No Preferred" : "Preferred State";	retVal += ',';
	retVal += data1 & 0x40 ? "Null State" : "No Null position";		retVal += ',';
	retVal += data1 & 0x80 ? "Volatile" : "Non Volatile";			retVal += ',';

	retVal += data2 & 0x01 ? "Buffered Bytes" : "Bit Field";

	return retVal;
}

std::string GetHIDItemUsage(U16 usagePage, const U8* pItem)
{
	// do we have an extended usage?
	if (GetNumHIDItemDataBytes(*pItem) == 4)
	{
		U16 usagePage = *(U16*) (pItem + 3);
		U16 usageID = *(U16*) (pItem + 1);
		return GetHIDUsageName(usagePage, usageID);
	}

	return GetHIDUsageName(usagePage, *(U16*)(pItem+1));
}

std::string GetSignedDataValue(const U8* pItem, DisplayBase display_base)
{
	int numBytes = GetNumHIDItemDataBytes(pItem[0]);		// number of data bytes

	if (numBytes == 0)
		return "0";

	int sVal;
	U64 uVal;

	if (numBytes == 1)
	{
		sVal = S8(pItem[1]);
		uVal = pItem[1];
	} else if (numBytes == 2) {
		sVal = *(S16*) (pItem + 1);
		uVal = *(U16*) (pItem + 1);
	} else {
		sVal = *(S32*) (pItem + 1);
		uVal = *(U32*) (pItem + 1);
	}

	std::string retVal;

	if (display_base == Decimal)
	{
		// we can't use AnalyzerHelpers::GetNumberString because we need signed values
		char buff[32];
		sprintf(buff, "%i", sVal);
		retVal = buff;
	} else {
		retVal = int2str_sal(uVal, display_base, numBytes*8);
	}

	return retVal;
}

std::string GetUnitExponent(U8 data)
{
	if (data > 0xF0)
		return "undefined";

	const char* results[] = {"0", "1", "2", "3", "4", "5", "6", "7", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1"};

	return results[data];
}

std::string GetUnit(const U8* pItem)
{
	U32 val = *(U32*) (pItem + 1);

	U8 firstNibble = val & 0x0f;

	// branch by quality measures
	if (val & 0x000000f0)				// Length
	{
		switch (firstNibble)
		{
		case 1:		return "Centimeter";
		case 2:		return "Radian";
		case 3:		return "Inch";
		case 4:		return "Degree";
		}

	} else if (val & 0x00000f00) {		// Mass

		if (firstNibble == 1  ||  firstNibble == 2)
			return "Gram";
		else if (firstNibble == 3  ||  firstNibble == 4)
			return "Slug";

	} else if (val & 0x0000f000) {		// Time

		if (firstNibble >= 1  ||  firstNibble <= 4)
			return "Second";

	} else if (val & 0x000f0000) {		// Temperature

		if (firstNibble == 1  ||  firstNibble == 2)
			return "Kelvin";
		else if (firstNibble == 3  ||  firstNibble == 4)
			return "Fahrenheit";

	} else if (val & 0x00f00000) {		// Current

		if (firstNibble >= 1  ||  firstNibble <= 4)
			return "Ampere";

	} else if (val & 0x0f000000) {		// Luminosity

		if (firstNibble >= 1  ||  firstNibble <= 4)
			return "Candela";
	}

	return "Undefined Unit";
}


USBAnalyzerResults::USBAnalyzerResults(USBAnalyzer* analyzer, USBAnalyzerSettings* settings)
:	mSettings(settings),
	mAnalyzer(analyzer)
{}

USBAnalyzerResults::~USBAnalyzerResults()
{}

double USBAnalyzerResults::GetSampleTime(S64 sample) const
{
	return (sample - mAnalyzer->GetTriggerSample()) / double(mAnalyzer->GetSampleRate());
}

std::string USBAnalyzerResults::GetSampleTimeStr(S64 sample) const
{
	char time_str[128];
	AnalyzerHelpers::GetTimeString(sample, mAnalyzer->GetTriggerSample(), mAnalyzer->GetSampleRate(), time_str, sizeof(time_str));

	// remove trailing zeros
	char* pEnd = strchr(time_str, 0) - 1;
	while (pEnd > time_str  &&  *pEnd == '0')
	{
		*pEnd = '\0';
		--pEnd;
	}

	return time_str;
}

void GetHIDReportDescriptorItemFrameDesc(const Frame& frm, DisplayBase display_base, std::vector<std::string>& results)
{
	if ((frm.mFlags & 0x3F) == FF_FieldIncomplete)
	{
		results.push_back("<item incomplete - see next packet>");
		results.push_back("<item incomplete>");
		results.push_back("<incomplete>");
		return;
	}

	const USBHidRepDescItemFrame& f(static_cast<const USBHidRepDescItemFrame&>(frm));

	std::string indent(f.GetIndentLevel() * 4, ' ');

	const U8* pItem = f.GetItem();
	U8 itemSize = GetNumHIDItemDataBytes(pItem[0]) + 1;

	// make the raw values (always HEX)
	std::string rawVal;
	for (int cnt = 0; cnt < itemSize; cnt++)
	{
		if (cnt)
			rawVal += ' ';

		rawVal += int2str_sal(pItem[cnt], Hexadecimal, 8);
	}

	size_t padding_chars;
	if (rawVal.size() >= 15)
		padding_chars = 1;
	else
		padding_chars = 16 - rawVal.size();

	std::string padding(padding_chars, ' ');
	std::string desc;

	const U8 firstByte = *pItem;
	const U8 tagType = (firstByte & 0xfc);
	const U8 bType = (firstByte >> 2) & 0x03;
	const U8 bTag = firstByte >> 4;

	if (bType == 0)		// Main items
	{
		if (bTag == 0x0A)
			desc = "Collection (" + GetCollectionData(pItem[1]) + ")";
		else if (bTag == 0x0C)
			desc = "End Collection";
		else if (bTag == 0x08)
			desc = "Input (" + GetInputData(pItem[1], pItem[2]) + ")";
		else if (bTag == 0x09)
			desc = "Output (" + GetOutputAndFeatureData(pItem[1], pItem[2]) + ")";
		else if (bTag == 0x0B)
			desc = "Feature (" + GetOutputAndFeatureData(pItem[1], pItem[2]) + ")";
		else
			desc = "Unknown Main item. bTag=" + int2str_sal(bTag, display_base, 4);

	} else if (bType == 1) {		// Global

		if (bTag == 0x00)
			desc = "Usage Page (" + GetHIDUsagePageName((pItem[2] << 8) | pItem[1]) + ")";
		else if (bTag == 0x01)
			desc = "Logical Minimum (" + GetSignedDataValue(pItem, display_base) + ")";
		else if (bTag == 0x02)
			desc = "Logical Maximum (" + GetSignedDataValue(pItem, display_base) + ")";
		else if (bTag == 0x03)
			desc = "Physical Minimum (" + GetSignedDataValue(pItem, display_base) + ")";
		else if (bTag == 0x04)
			desc = "Physical Maximum (" + GetSignedDataValue(pItem, display_base) + ")";
		else if (bTag == 0x05)
			desc = "Unit Exponent (" + GetUnitExponent(pItem[1]) + ")";
		else if (bTag == 0x06)
			desc = "Unit (" + GetUnit(pItem) + ")";
		else if (bTag == 0x07)
			desc = "Report Size (" + int2str_sal(pItem[1], display_base, 8) + ")";
		else if (bTag == 0x08)
			desc = "Report ID (" + int2str_sal(pItem[1], display_base, 8) + ")";
		else if (bTag == 0x09)
			desc = "Report Count (" + int2str_sal(pItem[1], display_base, 8) + ")";
		else if (bTag == 0x0A)
			desc = "Push";
		else if (bTag == 0x0B)
			desc = "Pop";
		else
			desc = "Unknown Global item bTag=" + int2str_sal(bTag, display_base, 4);

	} else if (bType == 2) {		// Local

		if (bTag == 0)
			desc = "Usage (" + GetHIDItemUsage(f.GetUsagePage(), pItem) + ")";
		else if (bTag == 1)
			desc = "Usage Minimum (" + GetHIDItemUsage(f.GetUsagePage(), pItem) + ")";
		else if (bTag == 2)
			desc = "Usage Maximum (" + GetHIDItemUsage(f.GetUsagePage(), pItem) + ")";
		else if (bTag == 3)
			desc = "Designator Index (" + int2str_sal(pItem[1], display_base, 8) + ")";
		else if (bTag == 4)
			desc = "Designator Minimum (" + int2str_sal(pItem[1], display_base, 8) + ")";
		else if (bTag == 5)
			desc = "Designator Maximum (" + int2str_sal(pItem[1], display_base, 8) + ")";
		else if (bTag == 7)
			desc = "String Index (" + int2str_sal(pItem[1], display_base, 8) + ")";
		else if (bTag == 8)
			desc = "String Minimum (" + int2str_sal(pItem[1], display_base, 8) + ")";
		else if (bTag == 9)
			desc = "String Maximum (" + int2str_sal(pItem[1], display_base, 8) + ")";
		else if (bTag == 10)
			desc = "Delimiter (" + int2str_sal(pItem[1], display_base, 8) + ")";
		else
			desc = "Unknown Local item bTag=" + int2str_sal(bTag, display_base, 4);

	} else {
		desc = "Unknown item type (" + int2str_sal(tagType, Hexadecimal, 8) + ")";
	}

	results.push_back(rawVal + padding + indent + desc);
	results.push_back(desc);
	results.push_back(rawVal + desc);
	results.push_back(rawVal);
}

void GetCtrlTransFrameDesc(const Frame& frm, DisplayBase display_base, std::vector<std::string>& results, const USBAnalyzerResults::USBStringContainer& stringDescriptors)
{
	const USBCtrlTransFieldFrame& f(static_cast<const USBCtrlTransFieldFrame&>(frm));

	// create value string for the general case
	U16 bcnt = f.GetNumBytes();
	U32 val = f.GetData();
	std::string val_str = int2str_sal(val, display_base, bcnt*8);

	// handle details for specific fields
	std::string desc;
	USBCtrlTransFieldType formatter = f.GetFormatter();
	if (formatter == Fld_bRequest_Standard)
	{
		desc = " ";
		desc += GetRequestName(val);

	} else if (formatter == Fld_bRequest_Class) {

		desc = " (Class request)";

	} else if (formatter == Fld_bRequest_HID) {

		desc = " ";
		desc += GetHIDRequestName(val);
		desc += " (HID class)";

	} else if (formatter == Fld_bRequest_CDC) {

		desc = " ";
		desc += GetCDCRequestName(val);
		desc += " (CDC class)";

	} else if (formatter == Fld_bRequest_Vendor) {

		desc = " (Vendor request)";

	} else if (formatter == Fld_bmRequestType  ||  formatter == Fld_bmRequestType_NoData) {

		desc = " Data direction=";
		if (formatter == Fld_bmRequestType_NoData)
			desc += "No data";
		else
			desc += (val & 0x80) ? "Device to host" : "Host to device";

		desc += ", Type=";
		switch ((val & 0x60) >> 5)
		{
		case 0:	desc += "Standard"; break;
		case 1:	desc += "Class"; break;
		case 2:	desc += "Vendor"; break;
		case 3:	desc += "Reserved"; break;
		}

		desc += ", Recipient=";
		switch (val & 0x1f)
		{
		case 0:	desc += "Device"; break;
		case 1:	desc += "Interface"; break;
		case 2:	desc += "Endpoint"; break;
		case 3:	desc += "Other"; break;
		default:	desc += "Reserved"; break;
		}

	} else if (formatter == Fld_wValue_Address) {

		desc += " Address=" + int2str_sal(val, display_base, 8);

	} else if (formatter == Fld_wValue_Descriptor) {

		U8 descriptor = (val >> 8) & 0xff;
		U8 index = val & 0xff;

		desc = " Descriptor=";
		desc += GetDescriptorName(descriptor);
		desc += ", Index=" + int2str_sal(index, display_base, 8);

	} else if (formatter == Fld_wValue_HIDSetIdle) {
		
		U8 duration = (val >> 8) & 0xff;
		U8 reportID = val & 0xff;

		desc = " Duration=";
		if (duration == 0)
			desc += "Indefinite";
		else
			desc += int2str(duration) + "ms";

		desc += ", Report ID=" + int2str_sal(reportID, display_base, 8);

	} else if (formatter == Fld_wValue_HIDGetIdle) {
		
		desc = " Report ID=" + int2str_sal(val & 0xff, display_base, 8);

	} else if (formatter == Fld_wValue_HIDSetProtocol) {

		desc = " Protocol=";
		if (val == 0)
			desc += "Boot protocol";
		else if (val == 1)
			desc += "Report protocol";
		else
			desc += "<unknown>";

	} else if (formatter == Fld_wValue_HIDGetSetReport) {

		U8 reportType = (val >> 8) & 0xff;
		U8 reportID = val & 0xff;

		desc = " Report type=";
		if (reportType == 1)
			desc += "Input";
		else if (reportType == 2)
			desc += "Output";
		else if (reportType == 3)
			desc += "Feature";
		else
			desc += "Reserved";

		desc += ", Report ID=" + int2str_sal(reportID, display_base, 8);

	} else if (formatter == Fld_bDescriptorType) {

		desc = " ";
		desc += GetDescriptorName(val & 0xff);

	} else if (formatter == Fld_bDescriptorType_Other) {

		desc = " <unknown>";

	} else if (formatter == Fld_Wchar) {

		desc = std::string(" char='") + char(val & 0xff) + '\'';

	} else if (formatter == Fld_wLANGID) {

		desc = " Language=" + std::string(GetLangName(val));

	} else if (formatter == Fld_wVendorId) {
			
		desc = " Vendor=" + std::string(GetVendorName(val));

	} else if (formatter == Fld_bMaxPower) {

		desc += " " + int2str(val * 2) + "mA";

	} else if (formatter == Fld_bmAttributes_Endpoint) {

		desc = " ";
		switch (val & 0x03)
		{
		case 0:		desc += "Control"; break;
		case 1:		desc += "Isochronous"; break;
		case 2:		desc += "Bulk"; break;
		case 3:		desc += "Interrupt"; break;
		}

		if ((val & 0x03) == 1)		// if isochronous
		{
			desc += ", ";
			switch ((val >> 2) & 0x03)
			{
			case 0:		desc += "No Synchronization"; break;
			case 1:		desc += "Asynchronous"; break;
			case 2:		desc += "Adaptive"; break;
			case 3:		desc += "Synchronous"; break;
			}

			desc += ", ";
			switch ((val >> 4) & 0x03)
			{
			case 0:		desc += "Data endpoint"; break;
			case 1:		desc += "Feedback endpoint"; break;
			case 2:		desc += "Implicit feedback Data endpoint"; break;
			case 3:		desc += "Reserved"; break;
			}
		}

	} else if (formatter == Fld_bmAttributes_Config) {

		desc  = (val & 0x40) ? " Self powered" : " Bus powered";
		desc += ", Remote wakeup ";
		desc += (val & 0x20) ? "supported" : "unsupported";

	} else if (formatter == Fld_bEndpointAddress  ||  formatter == Fld_wIndex_Endpoint) {

		desc = " Endpoint=" + int2str(val & 0x0F);
		desc += ", Direction=";
		desc += (val & 0x80) == 0 ? "OUT" : "IN";

	} else if (formatter == Fld_BCD) {

		desc = " ";
		if (val & 0xf000)
			desc += int2str(val >> 12);

		desc += int2str((val >> 8) & 0x0f);
		desc += ".";
		desc += int2str((val >> 4) & 0x0f);
		desc += int2str(val & 0x0f);

	} else if (formatter == Fld_ClassCode) {

		desc = " ";
		desc += GetUSBClassName((U8)val);

	} else if (formatter == Fld_HIDSubClass) {

		if (val == 0)
			desc = " None";
		else if (val == 1)
			desc = " Boot Interface";
		else
			desc = " Reserved";

	} else if (formatter == Fld_HIDProtocol) {

		if (val == 0)
			desc = " None";
		else if (val == 1)
			desc = " Keyboard";
		else if (val == 2)
			desc = " Mouse";
		else
			desc = " Reserved";
	} else if (formatter == Fld_wIndex_InterfaceNum) {
		desc = " Interface=" + int2str_sal(val & 0xff, display_base, 8);
	} else if (formatter == Fld_HID_bCountryCode) {

		desc = " Country=";
		desc += GetHIDCountryName((U8) val);

	} else if (formatter == Fld_String) {

		if (val != 0)
		{
			USBAnalyzerResults::USBStringContainer::const_iterator srch(stringDescriptors.find(std::make_pair(f.GetAddress(), (int)val)));
			if (srch != stringDescriptors.end())
				desc = " " + srch->second;
		}

	} else if (formatter == Fld_CDC_wValue_CommFeatureSelector) {

		if (val == 0)
			desc = " RESERVED";
		else if (val == 1)
			desc = " ABSTRACT_STATE";
		else if (val == 2)
			desc = " COUNTRY_SETTING";

	} else if (formatter == Fld_CDC_wValue_DisconnectConnect) {

		if (val == 0)
			desc = " Disconnect";
		else if (val == 1)
			desc = " Connect";

	} else if (formatter == Fld_CDC_wValue_RelayConfig) {

		if (val == 0)
			desc = " ON_HOOK";
		else if (val == 1)
			desc = " OFF_HOOK";
		else if (val == 2)
			desc = " SNOOPING";

	} else if (formatter == Fld_CDC_wValue_EnableDisable) {

		if (val == 0xffff)
			desc = " Disengage the holding circuit";
		else
			desc = " Prepare for a pulse-dialing cycle";

	} else if (formatter == Fld_CDC_wValue_Cycles) {
		desc = " Number of cycles";
	} else if (formatter == Fld_CDC_wValue_Timing) {

		U8 hi = (val >> 8) & 0xff;
		U8 lo = val & 0xff;

		desc = " Break time=" + int2str(hi) + "ms, make time=" + int2str(lo);

	} else if (formatter == Fld_CDC_wValue_NumberOfRings) {
		desc = " Number of rings";
	} else if (formatter == Fld_CDC_wValue_ControlSignalBitmap) {
		desc  = (val & 2) ? " Activate carrier" : " Deactivate carrier";
		desc += (val & 1) ? ", DTE Present" : ", DTE Not Present";
	} else if (formatter == Fld_CDC_wValue_DurationOfBreak) {

		if (val == 0xffff)
			desc = " Break until receive SEND_BREAK with wValue of 0";
		else
			desc = " Duration of break " + int2str(val) + "ms";

	} else if (formatter == Fld_CDC_wValue_OperationParms  ||  formatter == Fld_CDC_OperationMode) {

		if (val == 0)
			desc = " Simple Mode";
		else if (val == 1)
			desc = " Standalone Mode";
		else if (val == 2)
			desc = " Host Centric Mode";

	} else if (formatter == Fld_CDC_wValue_LineStateChange) {

		if (val == 0)
			desc = " Drop the active call on the line.";
		else if (val == 1)
			desc = " Start a new call on the line.";
		else if (val == 2)
			desc = " Apply ringing to the line.";
		else if (val == 3)
			desc = " Remove ringing from the line.";
		else if (val == 4)
			desc = " Switch to a specific call on the line.";

	} else if (formatter == Fld_CDC_wValue_UnitParameterStructure) {

		desc  = " bEntityId=" + int2str_sal(val & 0xff, display_base, 8);
		desc += ", bParameterIndex=" + int2str_sal(val >> 8, display_base, 8);

	} else if (formatter == Fld_CDC_wValue_NumberOfFilters) {
		desc = " Number of filters";
	} else if (formatter == Fld_CDC_wValue_FilterNumber) {
		desc = " Filter number";
	} else if (formatter == Fld_CDC_wValue_PacketFilterBitmap) {
		desc  = " PACKET_TYPE_MULTICAST=";		desc += ((val & 0x10) ? "1" : "0");
		desc += ", PACKET_TYPE_BROADCAST=";		desc += ((val & 0x08) ? "1" : "0");
		desc += ", PACKET_TYPE_DIRECTED=";		desc += ((val & 0x04) ? "1" : "0");
		desc += ", PACKET_TYPE_ALL_MULTICAST=";	desc += ((val & 0x02) ? "1" : "0");
		desc += ", PACKET_TYPE_PROMISCUOUS=";	desc += ((val & 0x01) ? "1" : "0");
	} else if (formatter == Fld_CDC_wValue_EthFeatureSelector) {
		desc = " ";
		desc += GetCDCEthFeatureSelectorName(val);
	} else if (formatter == Fld_CDC_wValue_ATMDataFormat) {
		if (val == 1)
			desc = " Concatenated ATM cells";
		else if (val == 2)
			desc = " ATM header template + concatenated ATM cell payloads";
		else if (val == 3)
			desc = " AAL 5 SDU";
	} else if (formatter == Fld_CDC_wValue_ATMFeatureSelector) {
		desc = " ";
		desc += GetCDCATMFeatureSelectorName(val);		
	} else if (formatter == Fld_CDC_wValue_ATMVCFeatureSelector) {
		if (val == 1)
			desc = " VC_US_CELLS_SENT";
		else if (val == 2)
			desc = " VC_DS_CELLS_RECEIVED";
	} else if (formatter == Fld_CDC_DescriptorSubtype) {
		desc = " ";
		desc += GetCDCDescriptorSubtypeName(val);
	} else if (formatter == Fld_CDC_bmCapabilities_Call) {
		desc = (val & 0x02) ? " Call management over a Data Class interface" : " Call management only over the Comm Class interface";
		desc += ", ";
		desc += (val & 0x01) ? "Device handles call management itself" : "Device doesn't handle call management itself";
	} else if (formatter == Fld_CDC_bmCapabilities_AbstractCtrl) {
		desc = " Network_Connection notification ";
		desc += (val & 0x08) ? "supported" : "not supported";
		desc += "; Send_Break request ";
		desc += (val & 0x04) ? "supported" : "not supported";
		desc += "; Set_Line_Coding, Set_Control_Line_State, Get_Line_Coding and the notification Serial_State ";
		desc += (val & 0x02) ? "supported" : "not supported";
		desc += "; Set_Comm_Feature, Clear_Comm_Feature and Get_Comm_Feature ";
		desc += (val & 0x01) ? "supported" : "not supported";
	} else if (formatter == Fld_CDC_bmCapabilities_DataLine) {
		if (val & 0x04)	desc  = "Device requires extra Pulse_Setup request during pulse dialing sequence to disengage holding circuit";
		if (val & 0x02)	desc += (desc.empty() ? "" : "; ") + std::string("Device supports the request combination of Set_Aux_Line_State, Ring_Aux_Jack, and notification Aux_Jack_Hook_State");
		if (val & 0x01)	desc += (desc.empty() ? "" : "; ") + std::string("Device supports the request combination of Pulse_Setup, Send_Pulse, and Set_Pulse_Time");
		if (!desc.empty())
			desc = " " + desc;
	} else if (formatter == Fld_CDC_bRingerVolSteps) {
		desc = " ";
		if (val == 0)
			desc += "256 discrete volume steps";
		else
			desc = int2str(val) + " discrete volume steps";
	} else if (formatter == Fld_CDC_bmCapabilities_TelOpModes) {
		desc  = (val & 0x04) ? " Supports Computer Centric mode" : " Does not support Computer Centric mode";
		desc += "; ";
		desc += (val & 0x02) ? "Supports Standalone mode" : "Does not support Standalone mode";
		desc += "; ";
		desc += (val & 0x01) ? "Supports Simple mode" : "Does not support Simple mode";
	} else if (formatter == Fld_CDC_bmCapabilities_TelCallStateRep) {
		desc  = (val & 0x20) ? " Supports line state change notification." : " Does not support line state change notification";
		desc += "; ";
		desc  = (val & 0x10) ? "Can report DTMF digits input remotely over the telephone line" : "Cannot report DTMF digits input remotely over the telephone line";
		desc += "; ";
		desc  = (val & 0x08) ? "Reports only incoming ringing" : "Reports incoming distinctive ringing patterns";
		desc += "; ";
		desc += (val & 0x04) ? "Reports caller ID information" : "Does not report caller ID";
		desc += "; ";
		desc += (val & 0x02) ? "Reports ringback, busy, and fast busy states." : "Reports only dialing state";
		desc += "; ";
		desc += (val & 0x01) ? "Reports interrupted dialtone in addition to normal dialtone" : "Reports only dialtone";
	} else if (formatter == Fld_CDC_bmOptions) {
		desc = (val & 0x01) ? " Wrapper used" : " No wrapper used";
	} else if (formatter == Fld_CDC_bPhysicalInterface) {
		if (val == 0)
			desc = " None";
		else if (val == 1)
			desc = " ISDN";
		else if (val >= 2  &&  val <= 200)
			desc = " RESERVED";
		else
			desc = " Vendor specific";

	} else if (formatter == Fld_CDC_bProtocol) {

		if (val == 0x00)
			desc = " No class specific protocol required";
		else if (val == 0x30)
			desc = " Physical interface protocol for ISDN BRI";
		else if (val == 0x31)
			desc = " HDLC";
		else if (val == 0x32)
			desc = " Transparent";
		else if (val == 0x50)
			desc = " Management protocol for Q.921 data link protocol";
		else if (val == 0x51)
			desc = " Data link protocol for Q.931";
		else if (val == 0x52)
			desc = " TEI-multiplexor for Q.921 data link protocol";
		else if (val == 0x90)
			desc = " Data compression procedures";
		else if (val == 0x91)
			desc = " Euro-ISDN protocol control";
		else if (val == 0x92)
			desc = " V.24 rate adaptation to ISDN";
		else if (val == 0x93)
			desc = " CAPI Commands";
		else if (val == 0xfd)
			desc = " Host based driver";
		else if (val == 0xfe)
			desc = " The protocol(s) are described using a Protocol Unit Functional Descriptors on Communication Class Interface.";
		else if (val == 0xff)
			desc = " Vendor-specific";
		else
			desc = " RESERVED";

	} else if (formatter == Fld_CDC_bmCapabilities_MultiChannel) {
		if (val & 0x04)	desc  = "Device supports the request Set_Unit_Parameter";
		if (val & 0x02)	desc += (desc.empty() ? "" : "; ") + std::string("Device supports the request Clear_Unit_Parameter");
		if (val & 0x01)	desc += (desc.empty() ? "" : "; ") + std::string("Device stores Unit parameters in non-volatile memory");
		if (!desc.empty())
			desc = " " + desc;
	} else if (formatter == Fld_CDC_bmCapabilities_CAPIControl) {
		if (val & 0x01)
			desc = " Device is an Intelligent CAPI device";
		else
			desc = " Device is an Simple CAPI device";
	} else if (formatter == Fld_CDC_bmEthernetStatistics) {

		if (val & 0x00000001)	desc  = "XMIT_OK";
		if (val & 0x00000002)	desc += (desc.empty() ? "" : ", ") + std::string("RVC_OK");
		if (val & 0x00000004)	desc += (desc.empty() ? "" : ", ") + std::string("XMIT_ERROR");
		if (val & 0x00000008)	desc += (desc.empty() ? "" : ", ") + std::string("RCV_ERROR");
		if (val & 0x00000010)	desc += (desc.empty() ? "" : ", ") + std::string("RCV_NO_BUFFER");
		if (val & 0x00000020)	desc += (desc.empty() ? "" : ", ") + std::string("DIRECTED_BYTES_XMIT");
		if (val & 0x00000040)	desc += (desc.empty() ? "" : ", ") + std::string("DIRECTED_FRAMES_XMIT");
		if (val & 0x00000080)	desc += (desc.empty() ? "" : ", ") + std::string("MULTICAST_BYTES_XMIT");
		if (val & 0x00000100)	desc += (desc.empty() ? "" : ", ") + std::string("MULTICAST_FRAMES_XMIT");
		if (val & 0x00000200)	desc += (desc.empty() ? "" : ", ") + std::string("BROADCAST_BYTES_XMIT");
		if (val & 0x00000400)	desc += (desc.empty() ? "" : ", ") + std::string("BROADCAST_FRAMES_XMIT");
		if (val & 0x00000800)	desc += (desc.empty() ? "" : ", ") + std::string("DIRECTED_BYTES_RCV");
		if (val & 0x00001000)	desc += (desc.empty() ? "" : ", ") + std::string("DIRECTED_FRAMES_RCV");
		if (val & 0x00002000)	desc += (desc.empty() ? "" : ", ") + std::string("MULTICAST_BYTES_RCV");
		if (val & 0x00004000)	desc += (desc.empty() ? "" : ", ") + std::string("MULTICAST_FRAMES_RCV");
		if (val & 0x00008000)	desc += (desc.empty() ? "" : ", ") + std::string("BROADCAST_BYTES_RCV");
		if (val & 0x00010000)	desc += (desc.empty() ? "" : ", ") + std::string("BROADCAST_FRAMES_RCV");
		if (val & 0x00020000)	desc += (desc.empty() ? "" : ", ") + std::string("RCV_CRC_ERROR");
		if (val & 0x00040000)	desc += (desc.empty() ? "" : ", ") + std::string("TRANSMIT_QUEUE_LENGTH");
		if (val & 0x00080000)	desc += (desc.empty() ? "" : ", ") + std::string("RCV_ERROR_ALIGNMENT");
		if (val & 0x00100000)	desc += (desc.empty() ? "" : ", ") + std::string("XMIT_ONE_COLLISION");
		if (val & 0x00200000)	desc += (desc.empty() ? "" : ", ") + std::string("XMIT_MORE_COLLISIONS");
		if (val & 0x00400000)	desc += (desc.empty() ? "" : ", ") + std::string("XMIT_DEFERRED");
		if (val & 0x00800000)	desc += (desc.empty() ? "" : ", ") + std::string("XMIT_MAX_COLLISIONS");
		if (val & 0x01000000)	desc += (desc.empty() ? "" : ", ") + std::string("RCV_OVERRUN");
		if (val & 0x02000000)	desc += (desc.empty() ? "" : ", ") + std::string("XMIT_UNDERRUN");
		if (val & 0x04000000)	desc += (desc.empty() ? "" : ", ") + std::string("XMIT_HEARTBEAT_FAILURE");
		if (val & 0x08000000)	desc += (desc.empty() ? "" : ", ") + std::string("XMIT_TIMES_CRS_LOST");
		if (val & 0x10000000)	desc += (desc.empty() ? "" : ", ") + std::string("XMIT_LATE_COLLISIONS");

		if (!desc.empty())
			desc = " " + desc;

	} else if (formatter == Fld_CDC_wNumberMCFilters) {

		desc = " Number of multicase filters=" + int2str(val & 0x7fff) + "; ";
		if (val & 0x8000)
			desc += "The device uses imperfect multicast address filtering (hashing)";
		else
			desc += "The device performs perfect multicast address filtering (no hashing)";

	} else if (formatter == Fld_CDC_bmDataCapabilities) {

		if (val & 0x08)		desc  = "Type 3 - AAL5 SDU";
		if (val & 0x04)		desc += (desc.empty() ? "" : ", ") + std::string("Type 2 - ATM header template + concatenated ATM cell payloads");
		if (val & 0x02)		desc += (desc.empty() ? "" : ", ") + std::string("Type 1 - Concatenated ATM cells");

		if (!desc.empty())
			desc = " " + desc;

	} else if (formatter == Fld_CDC_bmATMDeviceStatistics) {

		if (val & 0x10)		desc  = "Device counts upstream cells sent on a per VC basis (VC_US_CELLS_SENT)";
		if (val & 0x08)		desc += (desc.empty() ? "" : ", ") + std::string("Device counts downstream cells received on a per VC basis (VC_DS_CELLS_RECEIVED)");
		if (val & 0x04)		desc += (desc.empty() ? "" : ", ") + std::string("Device counts cells with HEC error detected and corrected (DS_CELLS_HEC_ERROR_CORRECTED)");
		if (val & 0x02)		desc += (desc.empty() ? "" : ", ") + std::string("Device counts upstream cells sent (US_CELLS_SENT)");
		if (val & 0x01)		desc += (desc.empty() ? "" : ", ") + std::string("Device counts downstream cells received (DS_CELLS_RECEIVED)");

		if (!desc.empty())
			desc = " " + desc;

	} else if (formatter == Fld_CDC_Data_AbstractState) {
		desc  = " ";
		desc += (val & 0x02) ? "Enables multiplexing" : "Disables multiplexing";
		desc += "; ";
		desc += (val & 0x01) ? "All of the endpoints in this interface will not accept data from the host or offer data to the host" : "The endpoints in this interface will continue to accept/offer data";
	} else if (formatter == Fld_CDC_Data_CountrySetting) {
		desc = " Country code";
	} else if (formatter == Fld_CDC_dwDTERate) {
		desc = " " + int2str(val) + " bps";
	} else if (formatter == Fld_CDC_bCharFormat) {
		if (val == 0)
			desc = " 1 Stop Bit";
		else if (val == 1)
			desc = " 1.5 Stop Bits";
		else if (val == 2)
			desc = " 2 Stop Bits";
	} else if (formatter == Fld_CDC_bParityType) {
		if (val == 0)
			desc = " None";
		else if (val == 1)
			desc = " Odd";
		else if (val == 2)
			desc = " Even";
		else if (val == 3)
			desc = " Mark";
		else if (val == 4)
			desc = " Space";
	} else if (formatter == Fld_CDC_bDataBits) {
		desc = " " + int2str(val) + " bits";
	} else if (formatter == Fld_CDC_dwRingerBitmap) {
		desc = " ";
		if (val & 0x80000000UL)
		{
			desc += int2str_sal((val >> 8) & 0xff, display_base, 8) + " ringer volume; ";
			desc += int2str_sal(val & 0xff, display_base, 8) + " ringer pattern";
		} else {
			desc += "A ringer does not exist";
		}

	} else if (formatter == Fld_CDC_dwLineState) {
		desc = " ";
		if (val & 0x80000000UL)
			desc += "Line is active";
		else
			desc += "No activity on the line";

		if ((val & 0xff) == 0xff)
			desc += "; No call exists on the line";
		else
			desc += "; Active call is " + int2str_sal(val & 0xff, display_base, 8);
	} else if (formatter == Fld_CDC_dwCallState) {

		desc = " ";
		if (val & 0x80000000UL)
			desc += "No active call";
		else
			desc += "Call is active";

		desc += "; ";

		U8 callStateValue = val & 0xff;
		if (callStateValue == 0)
			desc += "Call is idle";
		else if (callStateValue == 1)
			desc += "Typical dial tone";
		else if (callStateValue == 2)
			desc += "Interrupted dial tone";
		else if (callStateValue == 3)
			desc += "Dialing is in progress";
		else if (callStateValue == 4)
			desc += "Ringback";
		else if (callStateValue == 5)
			desc += "Connected";
		else if (callStateValue == 6)
			desc += "Incoming call";

		desc += "; ";

		U8 callStateChange = (val >> 8) & 0xff;
		if (callStateChange == 1)
			desc += "Call has become idle";
		else if (callStateChange == 2)
			desc += "Dialing";
		else if (callStateChange == 3)
			desc += "Ringback";
		else if (callStateChange == 4)
			desc += "Connected";
		else if (callStateChange == 5)
			desc += "Incomming call";
	}

	const char* fieldName = f.GetFieldName();
	const char* isIncomplete = (f.mFlags & 0x3F) == FF_FieldIncomplete ? " (incomplete)" : "";

	results.push_back(fieldName + std::string("=") + val_str + desc + isIncomplete);
	results.push_back(fieldName + std::string("=") + val_str);
	results.push_back(fieldName + desc);
	results.push_back(fieldName);
	results.push_back(val_str);
	results.push_back(desc);
}

void GetFrameDesc(const Frame& f, DisplayBase display_base, std::vector<std::string>& results, const USBAnalyzerResults::USBStringContainer& stringDescriptors)
{
	results.clear();

	if (f.mType == FT_Signal)
	{
		std::string result;
		if (f.mData1 == S_J)
			result = "J";
		else if (f.mData1 == S_K)
			result = "K";
		else if (f.mData1 == S_SE0)
			result = "SE0";
		else if (f.mData1 == S_SE1)
			result = "SE1";

		results.push_back(result);

	} else if (f.mType == FT_EOP) {
		results.push_back("EOP");
	} else if (f.mType == FT_Reset) {
		results.push_back("Reset");
	} else if (f.mType == FT_Idle) {
		results.push_back("Idle");
	} else if (f.mType == FT_SYNC) {
		results.push_back("SYNC");
	} else if (f.mType == FT_PID) {
		results.push_back("PID " + GetPIDName(USB_PID(f.mData1)));
		results.push_back(GetPIDName(USB_PID(f.mData1)));
	} else if (f.mType == FT_FrameNum) {
		results.push_back("Frame # " + int2str_sal(f.mData1, display_base, 11));
		results.push_back("F # " + int2str_sal(f.mData1, display_base, 11));
		results.push_back("Frame #");
		results.push_back(int2str_sal(f.mData1, display_base, 11));
	} else if (f.mType == FT_AddrEndp) {
		results.push_back("Address=" + int2str_sal(f.mData1, display_base, 7) + " Endpoint=" + int2str_sal(f.mData2, display_base, 5));
		results.push_back("Addr=" + int2str_sal(f.mData1, display_base, 7) + " Endp=" + int2str_sal(f.mData2, display_base, 5));
		results.push_back("A:" + int2str_sal(f.mData1, display_base, 7) + " E:" + int2str_sal(f.mData2, display_base, 5));
		results.push_back(int2str_sal(f.mData1, display_base, 7) + " " + int2str_sal(f.mData2, display_base, 5));
	} else if (f.mType == FT_Byte) {
		results.push_back("Byte " + int2str_sal(f.mData1, display_base, 8));
		results.push_back(int2str_sal(f.mData1, display_base, 8));
	} else if (f.mType == FT_KeepAlive) {
		results.push_back("Keep alive");
		results.push_back("KA");
	} else if (f.mType == FT_CRC5  ||  f.mType == FT_CRC16) {
		const int num_bits = f.mType == FT_CRC5 ? 5 : 16;
		results.push_back("CRC");
		if (f.mData1 == f.mData2)
		{
			results.push_back("CRC OK " + int2str_sal(f.mData1, display_base, num_bits));
			results.push_back("CRC OK");
		} else {
			results.push_back("CRC Bad! Rcvd: " + int2str_sal(f.mData1, display_base, num_bits) + " Calc: " + int2str_sal(f.mData2, display_base, num_bits));
			results.push_back("CRC Bad! Rcvd: " + int2str_sal(f.mData1, display_base, num_bits));
			results.push_back("CRC Bad");
		}

		results.push_back(int2str_sal(f.mData1, display_base, num_bits));

	} else if (f.mType == FT_Error) {
		results.push_back("Error packet");
		results.push_back("Error");
		results.push_back("Err");
		results.push_back("E");
	} else if (f.mType == FT_ControlTransferField) {
		GetCtrlTransFrameDesc(f, display_base, results, stringDescriptors);
	} else if (f.mType == FT_HIDReportDescriptorItem) {
		GetHIDReportDescriptorItemFrameDesc(f, display_base, results);
	}
}

void USBAnalyzerResults::GenerateBubbleText(U64 frame_index, Channel& channel, DisplayBase display_base)
{
	ClearResultStrings();
	Frame f = GetFrame(frame_index);
	std::vector<std::string> results;

	GetFrameDesc(f, display_base, results, mAllStringDescriptors);

	for (std::vector<std::string>::iterator ri(results.begin()); ri != results.end(); ++ri)
		AddResultString(ri->c_str());
}

void USBAnalyzerResults::GenerateExportFileControlTransfers(const char* file, DisplayBase display_base)
{
	std::ofstream file_stream(file, std::ios::out);

	U64 trigger_sample = mAnalyzer->GetTriggerSample();
	U32 sample_rate = mAnalyzer->GetSampleRate();

	Frame f;
	const U64 num_frames = GetNumFrames();
	std::vector<std::string> results;
	U8 address = 0;
	for (U64 fcnt = 0; fcnt < num_frames; fcnt++)
	{
		// get the frame
		f = GetFrame(fcnt);

		if (UpdateExportProgressAndCheckForCancel(fcnt, num_frames))
			return;

		if (f.mType == FT_AddrEndp)
			address = U8(f.mData1);

		if (f.mFlags == FF_StatusBegin)
			file_stream << "STATUS time: " << GetSampleTimeStr(f.mStartingSampleInclusive) << std::endl;
		else if (f.mFlags == FF_DataBegin)
			file_stream << "DATA time: " << GetSampleTimeStr(f.mStartingSampleInclusive) << std::endl;
		else if (f.mFlags == FF_DataDescriptor)
			file_stream << "Descriptor time: " << GetSampleTimeStr(f.mStartingSampleInclusive) << std::endl;
		else if (f.mFlags == FF_SetupBegin)
			file_stream << std::endl << "SETUP address: " + int2str_sal(address, display_base, 7) + " time: " << GetSampleTimeStr(f.mStartingSampleInclusive) << std::endl;

		if ((f.mType == FT_ControlTransferField  ||  f.mType == FT_HIDReportDescriptorItem)  &&  f.mFlags != FF_FieldIncomplete)
		{
			GetFrameDesc(f, display_base, results, mAllStringDescriptors);

			// output the packet
			file_stream << "\t" << results.front() << std::endl;
		} else if (f.mType == FT_Reset) {
			file_stream << std::endl << "USB RESET Time: " << GetSampleTimeStr(f.mStartingSampleInclusive) << std::endl;
		}

		if (f.mFlags == FF_StatusEnd)
			file_stream << "\t" << GetPIDName(USB_PID(f.mData1)) << std::endl;
		else if (f.mFlags == FF_DataInNAKed)
			file_stream << "\t<data IN packet NAKed by device. Time: " << GetSampleTimeStr(f.mStartingSampleInclusive) << '>' << std::endl;
		else if (f.mFlags == FF_DataOutNAKed)
			file_stream << "\t<data OUT packet NAKed by device. Time: " << GetSampleTimeStr(f.mStartingSampleInclusive) << '>' << std::endl;
		else if (f.mFlags == FF_StatusInNAKed)
			file_stream << "\t<status IN packet NAKed by device. Time: " << GetSampleTimeStr(f.mStartingSampleInclusive) << '>' << std::endl;
		else if (f.mFlags == FF_StatusOutNAKed)
			file_stream << "\t<status OUT data packet NAKed by device. Time: " << GetSampleTimeStr(f.mStartingSampleInclusive) << '>' << std::endl;
		else if (f.mFlags == FF_UnexpectedPacket)
			file_stream << "Unexpected packet " << GetPIDName(USB_PID(f.mData1)) << ". Time: " << GetSampleTimeStr(f.mStartingSampleInclusive) << std::endl;
	}

	// end
	UpdateExportProgressAndCheckForCancel(num_frames, num_frames);
}

void USBAnalyzerResults::GenerateExportFilePackets(const char* file, DisplayBase display_base)
{
	std::ofstream file_stream(file, std::ios::out);

	U64 trigger_sample = mAnalyzer->GetTriggerSample();
	U32 sample_rate = mAnalyzer->GetSampleRate();

	// header
	file_stream << "Time [s],PID,Address,Endpoint,Frame #,Data,CRC" << std::endl;

	Frame f;
	char time_str[128];
	time_str[0] = '\0';
	const U64 num_frames = GetNumFrames();
	std::string PID, Address, Endpoint, FrameNum, Data, CRC;
	for (U64 fcnt = 0; fcnt < num_frames; fcnt++)
	{
		// get the frame
		f = GetFrame(fcnt);

		if (UpdateExportProgressAndCheckForCancel(fcnt, num_frames))
			return;

		// start of a new packet?
		if (f.mType == FT_SYNC)
		{
			// make the time string
			AnalyzerHelpers::GetTimeString(f.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, sizeof(time_str));

			// reset packet fields
			PID.clear(), Address.clear(), Endpoint.clear(), FrameNum.clear(), Data.clear(), CRC.clear();
		} else if (f.mType == FT_PID) {
			PID = GetPIDName(USB_PID(f.mData1));

			// output the PRE packet because it does not have an EOP
			if (f.mData1 == PID_PRE)
				file_stream << time_str << "," << PID << ",,,,," << std::endl;

		} else if (f.mType == FT_AddrEndp) {
			Address = int2str_sal(f.mData1, display_base, 7);
			Endpoint = int2str_sal(f.mData2, display_base, 5);
		} else if (f.mType == FT_FrameNum) {
			FrameNum = int2str_sal(f.mData1, display_base, 11);
		} else if (f.mType == FT_Byte) {
			Data += (Data.empty() ? "" : " ") + int2str_sal(f.mData1, display_base, 8);
		} else if (f.mType == FT_CRC5  ||  f.mType == FT_CRC16) {
			CRC = int2str_sal(f.mData1, display_base, f.mType == FT_CRC5 ? 5 : 16);
		} else if (f.mType == FT_EOP) {

			// output the packet
			file_stream << time_str << "," << PID << "," << Address << "," << Endpoint << "," << FrameNum << "," << Data << "," << CRC << std::endl;

		} else if (f.mType == FT_Error) {

			// make the time string
			AnalyzerHelpers::GetTimeString(f.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, sizeof(time_str));

			file_stream << time_str << ",Parsing error,,,,," << std::endl;
		}
	}

	// end
	UpdateExportProgressAndCheckForCancel(num_frames, num_frames);
}

void USBAnalyzerResults::GenerateExportFileBytes(const char* file, DisplayBase display_base)
{
	std::ofstream file_stream(file, std::ios::out);

	U64 trigger_sample = mAnalyzer->GetTriggerSample();
	U32 sample_rate = mAnalyzer->GetSampleRate();

	// header
	file_stream << "Time [s],Byte" << std::endl;

	Frame f;
	char time_str[128];
	time_str[0] = '\0';
	const U64 num_frames = GetNumFrames();
	for (U64 fcnt = 0; fcnt < num_frames; fcnt++)
	{
		// get the frame
		f = GetFrame(fcnt);

		if (UpdateExportProgressAndCheckForCancel(fcnt, num_frames))
			return;

		// start of a new packet?
		if (f.mType == FT_Byte)
		{
			// make the time string
			AnalyzerHelpers::GetTimeString(f.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, sizeof(time_str));

			// output byte and timestamp
			file_stream << time_str << "," << int2str_sal(f.mData1, display_base, 8) << std::endl;
		}
	}

	// end
	UpdateExportProgressAndCheckForCancel(num_frames, num_frames);
}

void USBAnalyzerResults::GenerateExportFileSignals(const char* file, DisplayBase display_base)
{
	std::ofstream file_stream(file, std::ios::out);

	U64 trigger_sample = mAnalyzer->GetTriggerSample();
	U32 sample_rate = mAnalyzer->GetSampleRate();

	// header
	file_stream << "Time [s],Signal,Duration [ns]" << std::endl;

	Frame f;
	char time_str[128];
	time_str[0] = '\0';
	const U64 num_frames = GetNumFrames();
	for (U64 fcnt = 0; fcnt < num_frames; fcnt++)
	{
		// get the frame
		f = GetFrame(fcnt);

		if (UpdateExportProgressAndCheckForCancel(fcnt, num_frames))
			return;

		// make the time string
		AnalyzerHelpers::GetTimeString(f.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, sizeof(time_str));

		// output timestamp
		file_stream << time_str << ",";

		// start of a new packet?
		if (f.mType == FT_Signal)
		{
			if (f.mData1 == S_J)
				file_stream << 'J';
			else if (f.mData1 == S_K)
				file_stream << 'K';
			else if (f.mData1 == S_SE0)
				file_stream << "SE0";
			else if (f.mData1 == S_SE1)
				file_stream << "SE1";
				
			file_stream << ',' << (f.mEndingSampleInclusive - f.mStartingSampleInclusive) / (sample_rate / 1e9) << std::endl;
		}
	}

	// end
	UpdateExportProgressAndCheckForCancel(num_frames, num_frames);
}

void USBAnalyzerResults::GenerateExportFile(const char* file, DisplayBase display_base, U32 export_type_user_id)
{
	if (mSettings->mDecodeLevel == OUT_CONTROL_TRANSFERS)
		GenerateExportFileControlTransfers(file, display_base);
	else if (mSettings->mDecodeLevel == OUT_PACKETS)
		GenerateExportFilePackets(file, display_base);
	else if (mSettings->mDecodeLevel == OUT_BYTES)
		GenerateExportFileBytes(file, display_base);
	else if (mSettings->mDecodeLevel == OUT_SIGNALS)
		GenerateExportFileSignals(file, display_base);
}

void USBAnalyzerResults::GenerateFrameTabularText(U64 frame_index, DisplayBase display_base)
{
	ClearTabularText();
	Frame f = GetFrame( frame_index );
	std::vector<std::string> results;

	//GetFrameDesc(f, display_base, results);

	results.clear();

	if( f.mType == FT_Signal )
	{
		std::string result;
		if( f.mData1 == S_J )
			result = "J";
		else if( f.mData1 == S_K )
			result = "K";
		else if( f.mData1 == S_SE0 )
			result = "SE0";
		else if( f.mData1 == S_SE1 )
			result = "SE1";

		results.push_back( result );

	}
	else if( f.mType == FT_EOP )
	{
		results.push_back( "EOP" );
	}
	else if( f.mType == FT_Reset )
	{
		results.push_back( "Reset" );
	}
	else if( f.mType == FT_Idle )
	{
		results.push_back( "Idle" );
	}
	else if( f.mType == FT_SYNC )
	{
		results.push_back( "SYNC" );
	}
	else if( f.mType == FT_PID )
	{
		results.push_back( "PID " + GetPIDName( USB_PID( f.mData1 ) ) );
	}
	else if( f.mType == FT_FrameNum )
	{
		results.push_back( "Frame # " + int2str_sal( f.mData1, display_base, 11 ) );
	}
	else if( f.mType == FT_AddrEndp )
	{
		results.push_back( "Address=" + int2str_sal( f.mData1, display_base, 7 ) + " Endpoint=" + int2str_sal( f.mData2, display_base, 5 ) );
	}
	else if( f.mType == FT_Byte )
	{
		results.push_back( "Byte " + int2str_sal( f.mData1, display_base, 8 ) );
	}
	else if( f.mType == FT_KeepAlive )
	{
		results.push_back( "Keep alive" );
	}
	else if( f.mType == FT_CRC5 || f.mType == FT_CRC16 )
	{
		const int num_bits = f.mType == FT_CRC5 ? 5 : 16;
		if( f.mData1 == f.mData2 )
		{
			results.push_back( "CRC OK " + int2str_sal( f.mData1, display_base, num_bits ) );
		}
		else
		{
			results.push_back( "CRC Bad! Rcvd: " + int2str_sal( f.mData1, display_base, num_bits ) + " Calc: " + int2str_sal( f.mData2, display_base, num_bits ) );
		}
	}
	else if( f.mType == FT_Error )
	{
		results.push_back( "Error packet" );
	}
	 else if( f.mType == FT_ControlTransferField ) 
	 {
		 //hijack the bubble text generator for this.
		 std::vector<std::string> bubble_results;
		 GetCtrlTransFrameDesc( f, display_base, bubble_results, mAllStringDescriptors );
		 if( bubble_results.size() > 0 )
			results.push_back( *bubble_results.begin() );

	 }
	 else if( f.mType == FT_HIDReportDescriptorItem ) 
	 {
		 //hijack the bubble text generator for this.
		 std::vector<std::string> bubble_results;
		 GetHIDReportDescriptorItemFrameDesc( f, display_base, bubble_results );
		 if( bubble_results.size() > 0 )
			 results.push_back( *bubble_results.begin() );
	 }

	for( std::vector<std::string>::iterator ri( results.begin() ); ri != results.end(); ++ri )
		AddTabularText( ri->c_str() );
}

void USBAnalyzerResults::GeneratePacketTabularText(U64 packet_id, DisplayBase display_base)
{
	ClearResultStrings();
	AddResultString("not supported");
}

void USBAnalyzerResults::GenerateTransactionTabularText(U64 transaction_id, DisplayBase display_base)
{
	ClearResultStrings();
	AddResultString("not supported");
}
