#ifndef USB_ENUMS_H
#define USB_ENUMS_H

enum USBFrameTypes	// analyzer frames, NOT USB 1ms frame
{
	FT_Signal,		// low-level signals: J, K, SE0 or SE1

	FT_SYNC,		// USB Sync field
	FT_PID,			// USB PID
	FT_FrameNum,	// SOF's frame number field
	FT_AddrEndp,	// address & endpoint
	FT_EOP,			// USB End Of Packet
	FT_Reset,		// USB reset
	FT_CRC5,
	FT_CRC16,
	FT_Idle,
	FT_KeepAlive,	// Low speed keep alive signals

	FT_Byte,		// used in Bytes decode mode and for data payload of packets

	FT_Error,		// invalid packet

	FT_ControlTransferField,
	FT_HIDReportDescriptorItem,
};

// these are used by the exporter to help with formatting
// since we only have 6 bits available in the mFlags member, and we need more
// we don't actually use these ase bit flags but as distinct values/codes
enum USBFrameFlags
{
	FF_None,

	FF_FieldIncomplete,		// descriptor field or HID report item which spans two packets

	FF_SetupBegin,

	FF_DataBegin,
	FF_DataDescriptor,
	FF_DataInNAKed,
	FF_DataOutNAKed,
	FF_DataEnd,

	FF_StatusBegin,
	FF_StatusOutNAKed,
	FF_StatusInNAKed,
	FF_StatusEnd,

	FF_UnexpectedPacket,
};

// valid USB low and full speed PIDs
// PIDs used on high speed connection are commented out
enum USB_PID
{
	PID_Unknown	= 0x00,

	PID_IN		= 0x69,
	PID_OUT		= 0xE1,
	PID_SOF		= 0xA5,
	PID_SETUP	= 0x2D,

	PID_DATA0	= 0xC3,
	PID_DATA1	= 0x4B,
	//PID_DATA2	= 0x87,
	//PID_MDATA	= 0x0F,

	PID_ACK		= 0xD2,
	PID_NAK		= 0x5A,
	PID_STALL	= 0x1E,
	//PID_NYET	= 0x96,

	PID_PRE		= 0x3C,
	//PID_ERR	= 0x3C,
	//PID_SPLIT	= 0x78,
	//PID_PING	= 0xB4,
};

enum USBRequestCode
{
	GET_STATUS			= 0x00,
	CLEAR_FEATURE		= 0x01,
	SET_FEATURE			= 0x03,
	SET_ADDRESS			= 0x05,
	GET_DESCRIPTOR		= 0x06,
	SET_DESCRIPTOR		= 0x07,
	GET_CONFIGURATION	= 0x08,
	SET_CONFIGURATION	= 0x09,
	GET_INTERFACE		= 0x0A,
	SET_INTERFACE		= 0x11,
	SYNCH_FRAME			= 0x12,
};

enum USBHIDRequestCode
{
	GET_REPORT		= 0x01,
	GET_IDLE		= 0x02,
	GET_PROTOCOL	= 0x03,
	SET_REPORT		= 0x09,
	SET_IDLE		= 0x0A,
	SET_PROTOCOL	= 0x0B,
};

enum USBCDCRequestCode
{
	SEND_ENCAPSULATED_COMMAND		= 0x00,
	GET_ENCAPSULATED_RESPONSE		= 0x01,
	SET_COMM_FEATURE				= 0x02,
	GET_COMM_FEATURE				= 0x03,
	CLEAR_COMM_FEATURE				= 0x04,
	SET_AUX_LINE_STATE				= 0x10,
	SET_HOOK_STATE					= 0x11,
	PULSE_SETUP						= 0x12,
	SEND_PULSE						= 0x13,
	SET_PULSE_TIME					= 0x14,
	RING_AUX_JACK					= 0x15,
	SET_LINE_CODING					= 0x20,
	GET_LINE_CODING					= 0x21,
	SET_CONTROL_LINE_STATE			= 0x22,
	SEND_BREAK						= 0x23,
	SET_RINGER_PARMS				= 0x30,
	GET_RINGER_PARMS				= 0x31,
	SET_OPERATION_PARMS				= 0x32,
	GET_OPERATION_PARMS				= 0x33,
	SET_LINE_PARMS					= 0x34,
	GET_LINE_PARMS					= 0x35,
	DIAL_DIGITS						= 0x36,
	SET_UNIT_PARAMETER				= 0x37,
	GET_UNIT_PARAMETER				= 0x38,
	CLEAR_UNIT_PARAMETER			= 0x39,
	GET_PROFILE						= 0x3A,
	SET_ETHERNET_MULTICAST_FILTERS	= 0x40,
	SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER	= 0x41,
	GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER	= 0x42,
	SET_ETHERNET_PACKET_FILTER		= 0x43,
	GET_ETHERNET_STATISTIC			= 0x44,
	SET_ATM_DATA_FORMAT				= 0x50,
	GET_ATM_DEVICE_STATISTICS		= 0x51,
	SET_ATM_DEFAULT_VC				= 0x52,
	GET_ATM_VC_STATISTICS			= 0x53,
};

enum USBDescriptorType
{
	DT_Undefined					= 0x00,

	// standard
	DT_DEVICE						= 0x01,
	DT_CONFIGURATION				= 0x02,
	DT_STRING						= 0x03,
	DT_INTERFACE					= 0x04,
	DT_ENDPOINT						= 0x05,
	DT_DEVICE_QUALIFIER				= 0x06,
	DT_OTHER_SPEED_CONFIGURATION	= 0x07,
	DT_INTERFACE_POWER				= 0x08,

	// HID
	DT_HID				= 0x21,
	DT_HID_REPORT		= 0x22,
	DT_HID_PHYS			= 0x23,

	// CDC
	DT_CDC_CS_INTERFACE	= 0x24,
	DT_CDC_CS_ENDPOINT	= 0x25,
};

enum USBCDCDescriptorSubtype
{
	DST_HEADER				= 0x00,
	DST_CALL_MANAGEMENT,
	DST_ABSTRACT_CONTROL_MANAGEMENT,
	DST_DIRECT_LINE_MANAGEMENT,
	DST_TELEPHONE_RINGER,
	DST_TELEPHONE_CALL_AND_LINE_STATE,
	DST_UNION,
	DST_COUNTRY_SELECTION,
	DST_TELEPHONE_OPERATIONAL_MODES,
	DST_USB_TERMINAL,
	DST_NETWORK_CHANNEL_TERMINAL,
	DST_PROTOCOL_UNIT,
	DST_EXTENSION_UNIT,
	DST_MULTI_CHANNEL_MANAGEMENT,
	DST_CAPI_CONTROL_MANAGEMENT,
	DST_ETHERNET_NETWORKING,
	DST_ATM_NETWORKING,

	DST_Undefined					= 0xff,
};

enum USBCtrlTransFieldType
{
	Fld_None,

	Fld_bmRequestType,
	Fld_bmRequestType_NoData,
	Fld_bRequest_Standard,
	Fld_bRequest_HID,
	Fld_bRequest_CDC,
	Fld_bRequest_Class,
	Fld_bRequest_Vendor,
	Fld_wValue_Descriptor,
	Fld_wValue_Address,
	Fld_wValue_HIDGetIdle,
	Fld_wValue_HIDSetIdle,
	Fld_wValue_HIDSetProtocol,
	Fld_wValue_HIDGetSetReport,
	Fld_HID_bCountryCode,
	Fld_bDescriptorType,
	Fld_bDescriptorType_Other,
	Fld_bMaxPower,
	Fld_wLANGID,
	Fld_wIndex_InterfaceNum,
	Fld_wIndex_Endpoint,
	Fld_wVendorId,
	Fld_bmAttributes_Endpoint,
	Fld_bmAttributes_Config,
	Fld_bEndpointAddress,
	Fld_BCD,
	Fld_ClassCode,
	Fld_Wchar,
	Fld_String,
	Fld_HIDSubClass,
	Fld_HIDProtocol,

	Fld_CDC_DescriptorSubtype,
	Fld_CDC_bmCapabilities_Call,
	Fld_CDC_bmCapabilities_AbstractCtrl,
	Fld_CDC_bmCapabilities_DataLine,
	Fld_CDC_bRingerVolSteps,
	Fld_CDC_bmCapabilities_TelOpModes,
	Fld_CDC_bmCapabilities_TelCallStateRep,
	Fld_CDC_bmOptions,
	Fld_CDC_bPhysicalInterface,
	Fld_CDC_bProtocol,
	Fld_CDC_bmCapabilities_MultiChannel,
	Fld_CDC_bmCapabilities_CAPIControl,
	Fld_CDC_bmEthernetStatistics,
	Fld_CDC_wNumberMCFilters,
	Fld_CDC_bmDataCapabilities,
	Fld_CDC_bmATMDeviceStatistics,

	Fld_CDC_Data_AbstractState,
	Fld_CDC_Data_CountrySetting,

	Fld_CDC_dwDTERate,
	Fld_CDC_bCharFormat,
	Fld_CDC_bParityType,
	Fld_CDC_bDataBits,

	Fld_CDC_dwRingerBitmap,
	Fld_CDC_OperationMode,
	Fld_CDC_dwLineState,
	Fld_CDC_dwCallState,

	Fld_CDC_wValue_CommFeatureSelector,
	Fld_CDC_wValue_DisconnectConnect,
	Fld_CDC_wValue_RelayConfig,
	Fld_CDC_wValue_EnableDisable,
	Fld_CDC_wValue_Cycles,
	Fld_CDC_wValue_Timing,
	Fld_CDC_wValue_NumberOfRings,
	Fld_CDC_wValue_ControlSignalBitmap,
	Fld_CDC_wValue_DurationOfBreak,
	Fld_CDC_wValue_OperationParms,
	Fld_CDC_wValue_LineStateChange,
	Fld_CDC_wValue_UnitParameterStructure,
	Fld_CDC_wValue_NumberOfFilters,
	Fld_CDC_wValue_FilterNumber,
	Fld_CDC_wValue_PacketFilterBitmap,
	Fld_CDC_wValue_EthFeatureSelector,
	Fld_CDC_wValue_ATMDataFormat,
	Fld_CDC_wValue_ATMFeatureSelector,
	Fld_CDC_wValue_ATMVCFeatureSelector,
};

enum USBState
{
	S_K,
	S_J,
	S_SE0,
	S_SE1,
};

enum USBSpeed
{
	LOW_SPEED,		// 1.5 mbit/s
	FULL_SPEED,		// 12 mbit/s
};

enum USBDecodeLevel
{
	OUT_PACKETS,
	OUT_BYTES,
	OUT_SIGNALS,
	OUT_CONTROL_TRANSFERS,
};

enum USBClassCodes
{
	CC_DeferredToInterface			= 0x00,
	CC_Audio						= 0x01,
	CC_CommunicationsAndCDCControl	= 0x02,
	CC_HID							= 0x03,
	CC_Physical						= 0x05,
	CC_Image						= 0x06,
	CC_Printer						= 0x07,
	CC_MassStorage					= 0x08,
	CC_Hub							= 0x09,
	CC_CDCData						= 0x0A,
	CC_SmartCard					= 0x0B,
	CC_ContentSecurity				= 0x0D,
	CC_Video						= 0x0E,
	CC_PersonalHealthcare			= 0x0F,
	CC_AudioVideo					= 0x10,
	CC_Diagnostic					= 0xDC,
	CC_WirelessController			= 0xE0,
	CC_Miscellaneous				= 0xEF,
	CC_ApplicationSpecific			= 0xFE,
	CC_VendorSpecific				= 0xFF,
};

#endif	// USB_ENUMS_H
