#include <algorithm>
#include <cassert>
#include <iostream>

#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>

#include "USBAnalyzer.h"
#include "USBAnalyzerResults.h"
#include "USBTypes.h"
#include "USBLookupTables.h"

void USBCtrlTransFieldFrame::PackFrame( U32 data, U8 numBytes, U8 address, USBCtrlTransFieldType formatter, const char* name )
{
    // mData1 format:
    //
    // 66665555 55555544 44444444 33333333 33222222 22221111 11111100 00000000
    // 32109876 54321098 76543210 98765432 10987654 32109876 54321098 76543210
    // -------- -address -format- numBytes ---------------data----------------

    mData1 = data;
    mData1 |= U64( numBytes ) << 32;
    mData1 |= U64( formatter ) << 40;
    mData1 |= U64( address ) << 48;

    mData2 = ( U64 )name;
}

void USBHidRepDescItemFrame::PackFrame( const U8* pItem, U16 indentLevel, U16 usagePage )
{
    // mData1 contains the 1 to 5 bytes of the HID item
    U8 itemSize = GetNumHIDItemDataBytes( pItem[ 0 ] ) + 1;
    memcpy( &mData1, pItem, itemSize );

    // mData2 format:
    //
    // 66665555 55555544 44444444 33333333 33222222 22221111 11111100 00000000
    // 32109876 54321098 76543210 98765432 10987654 32109876 54321098 76543210
    // ----------------- ----------------- ----usagePage---- ---indentLevel---

    // contains indentLevel and usagePage
    mData2 = usagePage;
    mData2 <<= 16;
    mData2 |= indentLevel;
}

U32 USBPacket::GetDataPayload( int offset, int bcnt ) const
{
    U32 ret_val = 0;

    if( bcnt == 1 )
        ret_val = mData[ offset + 2 ];
    else if( bcnt == 2 )
        ret_val = ( mData[ offset + 3 ] << 8 ) | mData[ offset + 2 ];
    else if( bcnt == 3 )
        ret_val = ( mData[ offset + 4 ] << 16 ) | ( mData[ offset + 3 ] << 8 ) | mData[ offset + 2 ];
    else if( bcnt == 4 )
        ret_val = ( mData[ offset + 5 ] << 24 ) | ( mData[ offset + 4 ] << 16 ) | ( mData[ offset + 3 ] << 8 ) | mData[ offset + 2 ];

    return ret_val;
}

Frame USBPacket::GetDataPayloadField( int offset, int bcnt, U8 address, const char* name, USBCtrlTransFieldType formatter, U8 flags ) const
{
    USBCtrlTransFieldFrame f;
    f.mFlags = flags;
    f.mStartingSampleInclusive = mBitBeginSamples[ 16 + offset * 8 ];          // first data bit
    f.mEndingSampleInclusive = mBitBeginSamples[ 16 + ( offset + bcnt ) * 8 ]; // first bit after

    f.PackFrame( GetDataPayload( offset, bcnt ), bcnt, address, formatter, name );

    return f;
}

Frame USBPacket::GetHIDItem( int offset, int bcnt, U8* pItem, U16 indentLevel, U16 usagePage, U8 flags ) const
{
    USBHidRepDescItemFrame f;
    f.mFlags = flags;
    f.mStartingSampleInclusive = mBitBeginSamples[ 16 + offset * 8 ];          // first data bit
    f.mEndingSampleInclusive = mBitBeginSamples[ 16 + ( offset + bcnt ) * 8 ]; // first bit after

    if( ( flags & 0x3F ) != FF_FieldIncomplete )
        f.PackFrame( pItem, indentLevel, usagePage );

    return f;
}

void USBPacket::AddStandardSetupPacketFrame( USBAnalyzerResults* pResults, USBControlTransferParser& parser, U8 address )
{
    U8 bRequestType = GetDataPayload( 0, 1 );
    U8 bRequest = GetDataPayload( 1, 1 );
    U16 wIndex = GetDataPayload( 4, 2 );
    U16 wLength = GetDataPayload( 6, 2 );

    bool isRecipientInterface = ( bRequestType & 0x1f ) == 1;
    bool isRecipientEndpoint = ( bRequestType & 0x1f ) == 2;

    if( wLength )
        pResults->AddFrame( GetDataPayloadField( 0, 1, address, "bmRequestType", Fld_bmRequestType, FF_SetupBegin ) );
    else
        pResults->AddFrame( GetDataPayloadField( 0, 1, address, "bmRequestType", Fld_bmRequestType_NoData, FF_SetupBegin ) );

    pResults->AddFrame( GetDataPayloadField( 1, 1, address, "bRequest", Fld_bRequest_Standard ) );

    if( bRequest == SET_DESCRIPTOR || bRequest == GET_DESCRIPTOR )
        pResults->AddFrame( GetDataPayloadField( 2, 2, address, "wValue", Fld_wValue_Descriptor ) );
    else if( bRequest == SET_ADDRESS )
        pResults->AddFrame( GetDataPayloadField( 2, 2, address, "wValue", Fld_wValue_Address ) );
    else
        pResults->AddFrame( GetDataPayloadField( 2, 2, address, "wValue" ) );

    U8 hiByte = GetDataPayload( 3, 1 );
    U8 loByte = GetDataPayload( 2, 1 );
    if( isRecipientInterface )
    {
        pResults->AddFrame( GetDataPayloadField( 4, 2, address, "wIndex", Fld_wIndex_InterfaceNum ) );
    }
    else if( isRecipientEndpoint )
    {
        pResults->AddFrame( GetDataPayloadField( 4, 2, address, "wIndex", Fld_wIndex_Endpoint ) );
    }
    else if( bRequest == GET_DESCRIPTOR )
    {
        if( hiByte == DT_STRING && loByte != 0 )
            pResults->AddFrame( GetDataPayloadField( 4, 2, address, "wIndex", Fld_wLANGID ) );
        else
            pResults->AddFrame( GetDataPayloadField( 4, 2, address, "wIndex" ) );
    }
    else
    {
        pResults->AddFrame( GetDataPayloadField( 4, 2, address, "wIndex" ) );
    }

    pResults->AddFrame( GetDataPayloadField( 6, 2, address, "wLength" ) );
}

void USBPacket::AddClassSetupPacketFrame( USBAnalyzerResults* pResults, USBControlTransferParser& parser, U8 address )
{
    U8 bRequestType = GetDataPayload( 0, 1 );
    U8 bRequest = GetDataPayload( 1, 1 );
    U16 wIndex = GetDataPayload( 4, 2 );
    U16 wLength = GetDataPayload( 6, 2 );

    bool isRecipientInterface = ( bRequestType & 0x1f ) == 1;
    bool isRecipientEndpoint = ( bRequestType & 0x1f ) == 2;

    U8 interfaceClassCode = 0;

    if( isRecipientInterface )
        interfaceClassCode = parser.GetClassForInterface( ( U8 )wIndex );

    bool isCDC = interfaceClassCode == CC_CDCData || interfaceClassCode == CC_CommunicationsAndCDCControl;

    if( wLength )
        pResults->AddFrame( GetDataPayloadField( 0, 1, address, "bmRequestType", Fld_bmRequestType, FF_SetupBegin ) );
    else
        pResults->AddFrame( GetDataPayloadField( 0, 1, address, "bmRequestType", Fld_bmRequestType_NoData, FF_SetupBegin ) );

    USBCtrlTransFieldType fldType = Fld_bRequest_Class;
    if( interfaceClassCode == CC_HID )
        fldType = Fld_bRequest_HID;
    else if( interfaceClassCode == CC_CDCData || interfaceClassCode == CC_CommunicationsAndCDCControl )
        fldType = Fld_bRequest_CDC;

    pResults->AddFrame( GetDataPayloadField( 1, 1, address, "bRequest", fldType ) );

    if( interfaceClassCode == CC_HID )
    {
        if( bRequest == SET_IDLE )
            pResults->AddFrame( GetDataPayloadField( 2, 2, address, "wValue", Fld_wValue_HIDSetIdle ) );
        else if( bRequest == GET_IDLE )
            pResults->AddFrame( GetDataPayloadField( 2, 2, address, "wValue", Fld_wValue_HIDGetIdle ) );
        else if( bRequest == SET_PROTOCOL )
            pResults->AddFrame( GetDataPayloadField( 2, 2, address, "wValue", Fld_wValue_HIDSetProtocol ) );
        else if( bRequest == SET_REPORT || bRequest == GET_REPORT )
            pResults->AddFrame( GetDataPayloadField( 2, 2, address, "wValue", Fld_wValue_HIDGetSetReport ) );
        else
            pResults->AddFrame( GetDataPayloadField( 2, 2, address, "wValue" ) );
    }
    else if( isCDC )
    {
        USBCtrlTransFieldType fldType = Fld_None;

        if( bRequest == SET_COMM_FEATURE || bRequest == GET_COMM_FEATURE || bRequest == CLEAR_COMM_FEATURE )
            fldType = Fld_CDC_wValue_CommFeatureSelector;
        else if( bRequest == SET_AUX_LINE_STATE )
            fldType = Fld_CDC_wValue_DisconnectConnect;
        else if( bRequest == SET_HOOK_STATE )
            fldType = Fld_CDC_wValue_RelayConfig;
        else if( bRequest == PULSE_SETUP )
            fldType = Fld_CDC_wValue_EnableDisable;
        else if( bRequest == SEND_PULSE )
            fldType = Fld_CDC_wValue_Cycles;
        else if( bRequest == SET_PULSE_TIME )
            fldType = Fld_CDC_wValue_Timing;
        else if( bRequest == RING_AUX_JACK )
            fldType = Fld_CDC_wValue_NumberOfRings;
        else if( bRequest == SET_CONTROL_LINE_STATE )
            fldType = Fld_CDC_wValue_ControlSignalBitmap;
        else if( bRequest == SEND_BREAK )
            fldType = Fld_CDC_wValue_DurationOfBreak;
        else if( bRequest == SET_OPERATION_PARMS )
            fldType = Fld_CDC_wValue_OperationParms;
        else if( bRequest == SET_LINE_PARMS )
            fldType = Fld_CDC_wValue_LineStateChange;
        else if( bRequest == SET_UNIT_PARAMETER || bRequest == GET_UNIT_PARAMETER || bRequest == CLEAR_UNIT_PARAMETER )
            fldType = Fld_CDC_wValue_UnitParameterStructure;
        else if( bRequest == SET_ETHERNET_MULTICAST_FILTERS )
            fldType = Fld_CDC_wValue_NumberOfFilters;
        else if( bRequest == SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER || bRequest == GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER )
            fldType = Fld_CDC_wValue_FilterNumber;
        else if( bRequest == SET_ETHERNET_PACKET_FILTER )
            fldType = Fld_CDC_wValue_PacketFilterBitmap;
        else if( bRequest == GET_ETHERNET_STATISTIC )
            fldType = Fld_CDC_wValue_EthFeatureSelector;
        else if( bRequest == SET_ATM_DATA_FORMAT )
            fldType = Fld_CDC_wValue_ATMDataFormat;
        else if( bRequest == GET_ATM_DEVICE_STATISTICS )
            fldType = Fld_CDC_wValue_ATMFeatureSelector;
        else if( bRequest == GET_ATM_VC_STATISTICS )
            fldType = Fld_CDC_wValue_ATMVCFeatureSelector;

        pResults->AddFrame( GetDataPayloadField( 2, 2, address, "wValue", fldType ) );
    }
    else
    {
        pResults->AddFrame( GetDataPayloadField( 2, 2, address, "wValue" ) );
    }

    U8 hiByte = GetDataPayload( 3, 1 );
    U8 loByte = GetDataPayload( 2, 1 );
    if( isRecipientInterface )
        pResults->AddFrame( GetDataPayloadField( 4, 2, address, "wIndex", Fld_wIndex_InterfaceNum ) );
    else if( isRecipientEndpoint )
        pResults->AddFrame( GetDataPayloadField( 4, 2, address, "wIndex", Fld_wIndex_Endpoint ) );
    else
        pResults->AddFrame( GetDataPayloadField( 4, 2, address, "wIndex" ) );

    pResults->AddFrame( GetDataPayloadField( 6, 2, address, "wLength" ) );
}

void USBPacket::AddVendorSetupPacketFrame( USBAnalyzerResults* pResults, USBControlTransferParser& parser, U8 address )
{
    U8 bRequestType = GetDataPayload( 0, 1 );
    U8 bRequest = GetDataPayload( 1, 1 );
    U16 wIndex = GetDataPayload( 4, 2 );
    U16 wLength = GetDataPayload( 6, 2 );

    bool isRecipientInterface = ( bRequestType & 0x1f ) == 1;
    bool isRecipientEndpoint = ( bRequestType & 0x1f ) == 2;

    if( wLength )
        pResults->AddFrame( GetDataPayloadField( 0, 1, address, "bmRequestType", Fld_bmRequestType, FF_SetupBegin ) );
    else
        pResults->AddFrame( GetDataPayloadField( 0, 1, address, "bmRequestType", Fld_bmRequestType_NoData, FF_SetupBegin ) );

    pResults->AddFrame( GetDataPayloadField( 1, 1, address, "bRequest", Fld_bRequest_Vendor ) );

    pResults->AddFrame( GetDataPayloadField( 2, 2, address, "wValue" ) );

    U8 hiByte = GetDataPayload( 3, 1 );
    U8 loByte = GetDataPayload( 2, 1 );
    if( isRecipientInterface )
        pResults->AddFrame( GetDataPayloadField( 4, 2, address, "wIndex", Fld_wIndex_InterfaceNum ) );
    else if( isRecipientEndpoint )
        pResults->AddFrame( GetDataPayloadField( 4, 2, address, "wIndex", Fld_wIndex_Endpoint ) );
    else
        pResults->AddFrame( GetDataPayloadField( 4, 2, address, "wIndex" ) );

    pResults->AddFrame( GetDataPayloadField( 6, 2, address, "wLength" ) );
}

U64 USBPacket::AddSetupPacketFrame( USBAnalyzerResults* pResults, USBControlTransferParser& parser, U8 address )
{
    AddSyncAndPidFrames( pResults );

    U8 bRequestType = GetDataPayload( 0, 1 );

    if( ( ( bRequestType >> 5 ) & 0x03 ) == 0 )
        AddStandardSetupPacketFrame( pResults, parser, address );
    else if( ( ( bRequestType >> 5 ) & 0x03 ) == 1 )
        AddClassSetupPacketFrame( pResults, parser, address );
    else if( ( ( bRequestType >> 5 ) & 0x03 ) == 2 )
        AddVendorSetupPacketFrame( pResults, parser, address );

    AddCRC16Frame( pResults );
    AddEOPFrame( pResults );

    pResults->CommitResults();

    return mSampleEnd;
}

U64 USBPacket::AddDataStageFrames( USBAnalyzerResults* pResults, USBControlTransferParser& parser, U8 address )
{
    AddSyncAndPidFrames( pResults );

    parser.ParseDataPacket( *this );

    AddCRC16Frame( pResults );
    AddEOPFrame( pResults );

    pResults->CommitResults();

    return mSampleEnd;
}

void USBRequest::SetFromPacket( USBPacket& p )
{
    bmRequestType = p.GetDataPayload( 0, 1 );
    bRequest = p.GetDataPayload( 1, 1 );
    wValue = p.GetDataPayload( 2, 2 );
    wIndex = p.GetDataPayload( 4, 2 );
    wLength = p.GetDataPayload( 5, 2 );
}

void USBControlTransferParser::ResetParser()
{
    mParsedOffset = mDescBytes = 0;
    mDescType = DT_Undefined;
    mDescSubtype = DST_Undefined;
    mLeftover = 0;

    mRequest.Clear();

    mAddress = 0;

    pPacket = NULL;
    mPacketDataBytes = mPacketOffset = 0;

    mHidRepDescBytes = 0;
    memset( mHidRepDescItem, 0, sizeof mHidRepDescItem );
    mHidIndentLevel = mHidItemCnt = 0;
    mHidUsagePageStack.clear();
}

USBClassCodes USBControlTransferParser::GetClassCodeForInterface( U8 iface ) const
{
    USBInterfaceClassesContainer::const_iterator srch( mInterfaceClasses.find( iface ) );

    if( srch == mInterfaceClasses.end() )
        return CC_DeferredToInterface;

    return srch->second;
}

bool USBControlTransferParser::IsCDCClassRequest() const
{
    if( mRequest.IsClassRequest() && mRequest.IsRecipientInterface() )
    {
        USBClassCodes classCode = GetClassCodeForInterface( mRequest.GetInterfaceNum() );

        return classCode == CC_CDCData || classCode == CC_CommunicationsAndCDCControl;
    }

    return false;
}

bool USBControlTransferParser::ParseStringDescriptor()
{
    // if this is a supported language table desriptor or actual string descriptor
    if( mRequest.GetRequestedDescriptorIndex() == 0 )
    {
        while( mDescBytes > mParsedOffset && mPacketDataBytes > mPacketOffset )
        {
            pResults->AddFrame( pPacket->GetDataPayloadField( mPacketOffset, 2, mAddress, "wLANGID", Fld_wLANGID ) );
            mPacketOffset += 2;
            mParsedOffset += 2;
        }
    }
    else
    {
        if( mParsedOffset == 2 )
            stringDescriptor.clear();

        // the actual UNICODE string
        while( mDescBytes > mParsedOffset && mPacketDataBytes > mPacketOffset )
        {
            stringDescriptor += ( char )pPacket->GetDataPayload( mPacketOffset, 1 );

            pResults->AddFrame( pPacket->GetDataPayloadField( mPacketOffset, 2, mAddress, "wchar", Fld_Wchar ) );
            mPacketOffset += 2;
            mParsedOffset += 2;
        }

        // do we have the entire string?
        if( mDescBytes == mParsedOffset )
        {
            pResults->AddStringDescriptor( mAddress, mRequest.GetRequestedDescriptorIndex(), stringDescriptor );
        }
    }
    // return true if we've finished parsing the string descriptor, according to the parsed bLength
    return mDescBytes == mParsedOffset;
}

void USBControlTransferParser::ParseUnknownResponse()
{
    USBCtrlTransFieldFrame f;
    f.mFlags = FF_None;
    f.mType = FT_ControlTransferField;

    while( mPacketDataBytes > mPacketOffset ) // more data in packet?
    {
        f.mStartingSampleInclusive = *( pPacket->mBitBeginSamples.begin() + ( mPacketOffset + 2 ) * 8 );
        f.mEndingSampleInclusive = *( pPacket->mBitBeginSamples.begin() + ( mPacketOffset + 3 ) * 8 );

        f.PackFrame( pPacket->mData[ mPacketOffset + 2 ], 1, mAddress, Fld_None, "byte" );

        pResults->AddFrame( f );

        ++mPacketOffset;
        ++mParsedOffset;
    }
}

USBStructField DeviceDescriptorFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bcdUSB", 2, Fld_BCD },
    { "bDeviceClass", 1, Fld_ClassCode },
    { "bDeviceSubClass", 1, Fld_None },
    { "bDeviceProtocol", 1, Fld_None },
    { "bMaxPacketSize0", 1, Fld_None },
    { "idVendor", 2, Fld_wVendorId },
    { "idProduct", 2, Fld_None },
    { "bcdDevice", 2, Fld_BCD },
    { "iManufacturer", 1, Fld_String },
    { "iProduct", 1, Fld_String },
    { "iSerialNumber", 1, Fld_String },
    { "bNumConfigurations", 1, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField DeviceQualifierDescriptorFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bcdUSB", 2, Fld_BCD },
    { "bDeviceClass", 1, Fld_ClassCode },
    { "bDeviceSubClass", 1, Fld_None },
    { "bDeviceProtocol", 1, Fld_None },
    { "bMaxPacketSize0", 1, Fld_None },
    { "bNumConfigurations", 1, Fld_None },
    { "bReserved", 1, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField ConfigurationDescriptorFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "wTotalLength", 2, Fld_None },
    { "bNumInterfaces", 1, Fld_None },
    { "bConfigurationValue", 1, Fld_None },
    { "iConfiguration", 1, Fld_String },
    { "bmAttributes", 1, Fld_bmAttributes_Config },
    { "bMaxPower", 1, Fld_bMaxPower },

    { NULL, 0, Fld_None },
};

USBStructField InterfaceDescriptorFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bInterfaceNumber", 1, Fld_None },
    { "bAlternateSetting", 1, Fld_None },
    { "bNumEndpoints", 1, Fld_None },
    { "bInterfaceClass", 1, Fld_ClassCode },
    { "bInterfaceSubClass", 1, Fld_None },
    { "bInterfaceProtocol", 1, Fld_None },
    { "iInterface", 1, Fld_String },

    { NULL, 0, Fld_None },
};

USBStructField EndpointDescriptorFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bEndpointAddress", 1, Fld_bEndpointAddress },
    { "bmAttributes", 1, Fld_bmAttributes_Endpoint },
    { "wMaxPacketSize", 2, Fld_None },
    { "bInterval", 1, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField HIDDescriptorFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bcdHID", 2, Fld_BCD },
    { "bCountryCode", 1, Fld_HID_bCountryCode },
    { "bNumDescriptors", 1, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },

    // these are optional and depend of the number of HID class descriptors for this interface
    // we have enough for 16
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },
    { "bDescriptorType", 1, Fld_bDescriptorType },
    { "wDescriptorLength", 2, Fld_None },

    { NULL, 0, Fld_None },
};

//
// CDC descriptors
//

USBStructField CDCHeaderFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bcdCDC", 2, Fld_BCD },

    { NULL, 0, Fld_None },
};

USBStructField CDCCallManagementFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bmCapabilities", 1, Fld_CDC_bmCapabilities_Call },
    { "bDataInterface", 1, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField CDCAbstractControlManagementFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bmCapabilities", 1, Fld_CDC_bmCapabilities_AbstractCtrl },

    { NULL, 0, Fld_None },
};

USBStructField CDCDirectLineManagementFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bmCapabilities", 1, Fld_CDC_bmCapabilities_DataLine },

    { NULL, 0, Fld_None },
};

USBStructField CDCTelephoneRingerFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bRingerVolSteps", 1, Fld_CDC_bRingerVolSteps },
    { "bNumRingerPatterns", 1, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField CDCTelephoneOperationalModesFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bmCapabilities", 1, Fld_CDC_bmCapabilities_TelOpModes },

    { NULL, 0, Fld_None },
};

USBStructField CDCTelephoneCallStateReportingFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bmCapabilities", 1, Fld_CDC_bmCapabilities_TelCallStateRep },

    { NULL, 0, Fld_None },
};

USBStructField CDCUnionFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bMasterInterface", 1, Fld_None },
    { "bSlaveInterface0", 1, Fld_None },
    { "bSlaveInterface1", 1, Fld_None },
    { "bSlaveInterface2", 1, Fld_None },
    { "bSlaveInterface3", 1, Fld_None },
    { "bSlaveInterface4", 1, Fld_None },
    { "bSlaveInterface5", 1, Fld_None },
    { "bSlaveInterface6", 1, Fld_None },
    { "bSlaveInterface7", 1, Fld_None },
    { "bSlaveInterface8", 1, Fld_None },
    { "bSlaveInterface9", 1, Fld_None },
    { "bSlaveInterface10", 1, Fld_None },
    { "bSlaveInterface11", 1, Fld_None },
    { "bSlaveInterface12", 1, Fld_None },
    { "bSlaveInterface13", 1, Fld_None },
    { "bSlaveInterface14", 1, Fld_None },
    { "bSlaveInterface15", 1, Fld_None },
    { "bSlaveInterface16", 1, Fld_None },
    { "bSlaveInterface17", 1, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField CDCCountrySelectionFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "iCountryCodeRelDate", 1, Fld_String },
    { "wCountryCode0", 2, Fld_None },
    { "wCountryCode1", 2, Fld_None },
    { "wCountryCode2", 2, Fld_None },
    { "wCountryCode3", 2, Fld_None },
    { "wCountryCode4", 2, Fld_None },
    { "wCountryCode5", 2, Fld_None },
    { "wCountryCode6", 2, Fld_None },
    { "wCountryCode7", 2, Fld_None },
    { "wCountryCode8", 2, Fld_None },
    { "wCountryCode9", 2, Fld_None },
    { "wCountryCode10", 2, Fld_None },
    { "wCountryCode11", 2, Fld_None },
    { "wCountryCode12", 2, Fld_None },
    { "wCountryCode13", 2, Fld_None },
    { "wCountryCode14", 2, Fld_None },
    { "wCountryCode15", 2, Fld_None },
    { "wCountryCode16", 2, Fld_None },
    { "wCountryCode17", 2, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField CDCUSBTerminalFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bEntityId", 1, Fld_None },
    { "bInInterfaceNo", 1, Fld_None },
    { "bOutInterfaceNo", 1, Fld_None },
    { "bmOptions", 1, Fld_CDC_bmOptions },
    { "bChildId0", 1, Fld_None },
    { "bChildId1", 1, Fld_None },
    { "bChildId2", 1, Fld_None },
    { "bChildId3", 1, Fld_None },
    { "bChildId4", 1, Fld_None },
    { "bChildId5", 1, Fld_None },
    { "bChildId6", 1, Fld_None },
    { "bChildId7", 1, Fld_None },
    { "bChildId8", 1, Fld_None },
    { "bChildId9", 1, Fld_None },
    { "bChildId10", 1, Fld_None },
    { "bChildId11", 1, Fld_None },
    { "bChildId12", 1, Fld_None },
    { "bChildId13", 1, Fld_None },
    { "bChildId14", 1, Fld_None },
    { "bChildId15", 1, Fld_None },
    { "bChildId16", 1, Fld_None },
    { "bChildId17", 1, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField CDCNetworkChannelTerminalFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bEntityId", 1, Fld_None },
    { "iName", 1, Fld_String },
    { "bChannelIndex", 1, Fld_None },
    { "bPhysicalInterface", 1, Fld_CDC_bPhysicalInterface },

    { NULL, 0, Fld_None },
};

USBStructField CDCProtocolUnitFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bEntityId", 1, Fld_None },
    { "bProtocol", 1, Fld_CDC_bProtocol },
    { "bOutInterfaceNo", 1, Fld_None },
    { "bmOptions", 1, Fld_CDC_bmOptions },
    { "bChildId0", 1, Fld_None },
    { "bChildId1", 1, Fld_None },
    { "bChildId2", 1, Fld_None },
    { "bChildId3", 1, Fld_None },
    { "bChildId4", 1, Fld_None },
    { "bChildId5", 1, Fld_None },
    { "bChildId6", 1, Fld_None },
    { "bChildId7", 1, Fld_None },
    { "bChildId8", 1, Fld_None },
    { "bChildId9", 1, Fld_None },
    { "bChildId10", 1, Fld_None },
    { "bChildId11", 1, Fld_None },
    { "bChildId12", 1, Fld_None },
    { "bChildId13", 1, Fld_None },
    { "bChildId14", 1, Fld_None },
    { "bChildId15", 1, Fld_None },
    { "bChildId16", 1, Fld_None },
    { "bChildId17", 1, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField CDCExtensionUnitFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bEntityId", 1, Fld_None },
    { "bExtensionCode", 1, Fld_None },
    { "iName", 1, Fld_String },
    { "bChildId0", 1, Fld_None },
    { "bChildId1", 1, Fld_None },
    { "bChildId2", 1, Fld_None },
    { "bChildId3", 1, Fld_None },
    { "bChildId4", 1, Fld_None },
    { "bChildId5", 1, Fld_None },
    { "bChildId6", 1, Fld_None },
    { "bChildId7", 1, Fld_None },
    { "bChildId8", 1, Fld_None },
    { "bChildId9", 1, Fld_None },
    { "bChildId10", 1, Fld_None },
    { "bChildId11", 1, Fld_None },
    { "bChildId12", 1, Fld_None },
    { "bChildId13", 1, Fld_None },
    { "bChildId14", 1, Fld_None },
    { "bChildId15", 1, Fld_None },
    { "bChildId16", 1, Fld_None },
    { "bChildId17", 1, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField CDCMultiChannelFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bmCapabilities", 1, Fld_CDC_bmCapabilities_MultiChannel },

    { NULL, 0, Fld_None },
};

USBStructField CDCCAPIControlFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "bmCapabilities", 1, Fld_CDC_bmCapabilities_CAPIControl },

    { NULL, 0, Fld_None },
};

USBStructField CDCEthernetNetworkingFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "iMACAddress", 1, Fld_String },
    { "bmEthernetStatistics", 4, Fld_CDC_bmEthernetStatistics },
    { "wMaxSegmentSize", 2, Fld_None },
    { "wNumberMCFilters", 2, Fld_CDC_wNumberMCFilters },
    { "bNumberPowerFilters", 2, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField CDCATMNetworkingFields[] = {
    //{"bLength",				1, Fld_None},
    //{"bDescriptorType",		1, Fld_None},
    { "bDescriptorSubtype", 1, Fld_CDC_DescriptorSubtype },

    { "iEndSystemIdentifier", 1, Fld_String },
    { "bmDataCapabilities", 1, Fld_CDC_bmDataCapabilities },
    { "bmATMDeviceStatistics", 1, Fld_CDC_bmATMDeviceStatistics },
    { "wType2MaxSegmentSize", 2, Fld_None },
    { "wType3MaxSegmentSize", 2, Fld_None },
    { "wMaxVC", 2, Fld_None },

    { NULL, 0, Fld_None },
};

//
// CDC data payloads
//

USBStructField CDCLineCodingFields[] = {
    { "dwDTERate", 4, Fld_CDC_dwDTERate },
    { "bCharFormat", 1, Fld_CDC_bCharFormat },
    { "bParityType", 1, Fld_CDC_bParityType },
    { "bDataBits", 1, Fld_CDC_bDataBits },

    { NULL, 0, Fld_None },
};

USBStructField CDCAbstractStateFields[] = {
    { "ABSTRACT_STATE", 2, Fld_CDC_Data_AbstractState },

    { NULL, 0, Fld_None },
};

USBStructField CDCCountrySettingFields[] = {
    { "COUNTRY_SETTING", 2, Fld_CDC_Data_CountrySetting },

    { NULL, 0, Fld_None },
};

USBStructField CDCRingerConfigFields[] = {
    { "dwRingerBitmap", 4, Fld_CDC_dwRingerBitmap },

    { NULL, 0, Fld_None },
};

USBStructField CDCOperationModeFields[] = {
    { "Operation mode", 4, Fld_CDC_OperationMode },

    { NULL, 0, Fld_None },
};

USBStructField CDCLineParmsFields[] = {
    { "wLength", 2, Fld_None },
    { "dwRingerBitmap", 4, Fld_CDC_dwRingerBitmap },
    { "dwLineState", 4, Fld_CDC_dwLineState },
    { "dwCallState0", 4, Fld_CDC_dwCallState },
    { "dwCallState1", 4, Fld_CDC_dwCallState },
    { "dwCallState2", 4, Fld_CDC_dwCallState },
    { "dwCallState3", 4, Fld_CDC_dwCallState },
    { "dwCallState4", 4, Fld_CDC_dwCallState },
    { "dwCallState5", 4, Fld_CDC_dwCallState },
    { "dwCallState6", 4, Fld_CDC_dwCallState },
    { "dwCallState7", 4, Fld_CDC_dwCallState },
    { "dwCallState8", 4, Fld_CDC_dwCallState },
    { "dwCallState9", 4, Fld_CDC_dwCallState },
    { "dwCallState10", 4, Fld_CDC_dwCallState },
    { "dwCallState11", 4, Fld_CDC_dwCallState },
    { "dwCallState12", 4, Fld_CDC_dwCallState },
    { "dwCallState13", 4, Fld_CDC_dwCallState },
    { "dwCallState14", 4, Fld_CDC_dwCallState },
    { "dwCallState15", 4, Fld_CDC_dwCallState },
    { "dwCallState16", 4, Fld_CDC_dwCallState },
    { "dwCallState17", 4, Fld_CDC_dwCallState },
    { "dwCallState18", 4, Fld_CDC_dwCallState },
    { "dwCallState19", 4, Fld_CDC_dwCallState },

    { NULL, 0, Fld_None },
};

USBStructField CDCUnitParameterFields[] = {
    { "bEntityId", 1, Fld_None },
    { "bParameterIndex", 1, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField CDCUnsignedIntFields[] = {
    { "uint32", 4, Fld_None },

    { NULL, 0, Fld_None },
};

USBStructField CDCATMDefaultVCFields[] = {
    { "VPI", 1, Fld_None },
    { "VCI", 2, Fld_None },

    { NULL, 0, Fld_None },
};

void USBControlTransferParser::ParseStructure( USBStructField* descFields )
{
    int descFldCnt = 0;
    int descFldOffset = 2; // we already handled bLength and bDescriptorType in the caller

    // find the first non-parsed or partially parsed descriptor field in this packet
    while( descFldOffset < mParsedOffset && descFields[ descFldCnt ].numBytes != 0 ) // haven't reached the last field?
    {
        int fldBytes = descFields[ descFldCnt ].numBytes;

        // did we get the LS byte in the previous packet?
        if( fldBytes > 1 && descFldOffset + fldBytes > mParsedOffset )
        {
            U8 bytesRemaining = mParsedOffset - descFldOffset;

            USBCtrlTransFieldFrame f;
            f.mFlags = FF_None;
            f.mStartingSampleInclusive = pPacket->mBitBeginSamples[ 16 ];
            f.mEndingSampleInclusive = pPacket->mBitBeginSamples[ 16 + bytesRemaining * 8 ];

            U32 newVal = pPacket->GetDataPayload( 0, bytesRemaining ) << ( fldBytes - bytesRemaining ) * 8;
            newVal = newVal | mLeftover;
            f.PackFrame( newVal, 2, mAddress, descFields[ descFldCnt ].formatter, descFields[ descFldCnt ].name );

            pResults->AddFrame( f );

            ++mPacketOffset;
            ++mParsedOffset;
        }

        descFldOffset += fldBytes;
        ++descFldCnt;
    }

    while( descFields[ descFldCnt ].numBytes != 0 // haven't reached the last field?
           && mDescBytes > mParsedOffset          // more bytes in descriptor?
           && mPacketDataBytes > mPacketOffset    // more data in packet?
           && mDescBytes >=
                  mParsedOffset +
                      descFields[ descFldCnt ].numBytes ) // the next field won't consume more than we have in the received descriptor
    {
        int fldBytes = descFields[ descFldCnt ].numBytes;

        USBCtrlTransFieldType formatter = descFields[ descFldCnt ].formatter;

        // get the interface number and interface class/subclass
        if( descFields == InterfaceDescriptorFields )
        {
            if( descFldCnt == 0 ) // bInterfaceNumber
                mInterfaceNumber = pPacket->GetDataPayload( mPacketOffset, 1 );
            else if( descFldCnt == 3 ) // bInterfaceClass
                mInterfaceClasses[ mInterfaceNumber ] = ( USBClassCodes )pPacket->GetDataPayload( mPacketOffset, 1 );
            else if( descFldCnt == 4 && mInterfaceClasses[ mInterfaceNumber ] == CC_HID ) // bInterfaceSubClass
                formatter = Fld_HIDSubClass;
            else if( descFldCnt == 5 && mInterfaceClasses[ mInterfaceNumber ] == CC_HID ) // bInterfaceProtocol
                formatter = Fld_HIDProtocol;
        }

        // does this field spill over into the next packet?
        U8 flags = 0;
        if( fldBytes > mPacketDataBytes - mPacketOffset )
        {
            fldBytes = mPacketDataBytes - mPacketOffset;
            mLeftover = pPacket->GetDataPayload( mPacketOffset, fldBytes );

            flags = FF_FieldIncomplete;
        }

        pResults->AddFrame(
            pPacket->GetDataPayloadField( mPacketOffset, fldBytes, mAddress, descFields[ descFldCnt ].name, formatter, flags ) );

        mPacketOffset += fldBytes;
        mParsedOffset += fldBytes;
        descFldOffset += fldBytes;

        ++descFldCnt;
    }
}

void USBControlTransferParser::ParseStandardDescriptor()
{
    USBStructField* descFields = NULL;
    if( mDescType == DT_DEVICE )
        descFields = DeviceDescriptorFields;
    else if( mDescType == DT_DEVICE_QUALIFIER )
        descFields = DeviceQualifierDescriptorFields;
    else if( mDescType == DT_CONFIGURATION || mDescType == DT_OTHER_SPEED_CONFIGURATION )
        descFields = ConfigurationDescriptorFields;
    else if( mDescType == DT_INTERFACE )
        descFields = InterfaceDescriptorFields;
    else if( mDescType == DT_ENDPOINT )
        descFields = EndpointDescriptorFields;
    else if( mDescType == DT_HID && mInterfaceClasses[ mInterfaceNumber ] == CC_HID )
        descFields = HIDDescriptorFields;
    else if( mDescType == DT_CDC_CS_INTERFACE ||
             mDescType == DT_CDC_CS_ENDPOINT && ( mInterfaceClasses[ mInterfaceNumber ] == CC_CommunicationsAndCDCControl ||
                                                  mInterfaceClasses[ mInterfaceNumber ] == CC_CDCData ) )
    {
        if( mDescSubtype == DST_HEADER )
            descFields = CDCHeaderFields;
        else if( mDescSubtype == DST_CALL_MANAGEMENT )
            descFields = CDCCallManagementFields;
        else if( mDescSubtype == DST_ABSTRACT_CONTROL_MANAGEMENT )
            descFields = CDCAbstractControlManagementFields;
        else if( mDescSubtype == DST_DIRECT_LINE_MANAGEMENT )
            descFields = CDCDirectLineManagementFields;
        else if( mDescSubtype == DST_TELEPHONE_RINGER )
            descFields = CDCTelephoneRingerFields;
        else if( mDescSubtype == DST_TELEPHONE_CALL_AND_LINE_STATE )
            descFields = CDCTelephoneCallStateReportingFields;
        else if( mDescSubtype == DST_UNION )
            descFields = CDCUnionFields;
        else if( mDescSubtype == DST_COUNTRY_SELECTION )
            descFields = CDCCountrySelectionFields;
        else if( mDescSubtype == DST_TELEPHONE_OPERATIONAL_MODES )
            descFields = CDCTelephoneOperationalModesFields;
        else if( mDescSubtype == DST_USB_TERMINAL )
            descFields = CDCUSBTerminalFields;
        else if( mDescSubtype == DST_NETWORK_CHANNEL_TERMINAL )
            descFields = CDCNetworkChannelTerminalFields;
        else if( mDescSubtype == DST_PROTOCOL_UNIT )
            descFields = CDCProtocolUnitFields;
        else if( mDescSubtype == DST_EXTENSION_UNIT )
            descFields = CDCExtensionUnitFields;
        else if( mDescSubtype == DST_MULTI_CHANNEL_MANAGEMENT )
            descFields = CDCMultiChannelFields;
        else if( mDescSubtype == DST_CAPI_CONTROL_MANAGEMENT )
            descFields = CDCCAPIControlFields;
        else if( mDescSubtype == DST_ETHERNET_NETWORKING )
            descFields = CDCEthernetNetworkingFields;
        else if( mDescSubtype == DST_ATM_NETWORKING )
            descFields = CDCATMNetworkingFields;
    }

    if( descFields )
        ParseStructure( descFields );

    // parse the rest of the descriptor or packet as raw bytes

    while( mDescBytes > mParsedOffset            // more bytes in descriptor?
           && mPacketDataBytes > mPacketOffset ) // more data in packet?
    {
        USBCtrlTransFieldFrame f;
        f.mFlags = FF_None;
        f.mStartingSampleInclusive = *( pPacket->mBitBeginSamples.begin() + ( mPacketOffset + 2 ) * 8 );
        f.mEndingSampleInclusive = *( pPacket->mBitBeginSamples.begin() + ( mPacketOffset + 3 ) * 8 );

        f.PackFrame( pPacket->mData[ mPacketOffset + 2 ], 1, mAddress, Fld_None, "byte" );

        pResults->AddFrame( f );

        ++mPacketOffset;
        ++mParsedOffset;
    }

    // reset the parser if we have completeted parsing the descriptor
    if( mDescBytes == mParsedOffset )
    {
        mParsedOffset = 0;
        mDescType = DT_Undefined;
    }
}

void USBControlTransferParser::ParseHIDReportDescriptor()
{
    // more bytes in the packet?
    while( mPacketDataBytes > mPacketOffset )
    {
        int itemStartOffset = mPacketOffset;

        // we need to have at least the first byte of the item to get the item size
        if( mHidRepDescBytes == 0 )
            mHidRepDescItem[ mHidRepDescBytes++ ] = pPacket->GetDataPayload( mPacketOffset++, 1 );

        U8 itemSize = GetNumHIDItemDataBytes( mHidRepDescItem[ 0 ] ) + 1;

        // do we have the entire item in this packet?
        if( itemSize - mHidRepDescBytes <= mPacketDataBytes - mPacketOffset )
        {
            // we have the entire item - copy it to the item buffer
            while( mHidRepDescBytes < itemSize )
                mHidRepDescItem[ mHidRepDescBytes++ ] = pPacket->GetDataPayload( mPacketOffset++, 1 );

            // end collection decreases the indent level
            if( IsHIDItemEndCollection( mHidRepDescItem[ 0 ] ) && mHidIndentLevel > 0 )
                --mHidIndentLevel;
            else if( IsHIDItemUsagePage( mHidRepDescItem[ 0 ] ) )
                SetHIDUsagePage( *( U16* )( mHidRepDescItem + 1 ) );
            else if( IsHIDItemPush( mHidRepDescItem[ 0 ] ) )
                PushHIDUsagePage();
            else if( IsHIDItemPop( mHidRepDescItem[ 0 ] ) )
                PopHIDUsagePage();

            // make the frame with all the data
            pResults->AddFrame( pPacket->GetHIDItem( itemStartOffset, mPacketOffset - itemStartOffset, mHidRepDescItem, mHidIndentLevel,
                                                     GetHIDUsagePage(), mHidItemCnt == 0 ? FF_DataDescriptor : FF_None ) );

            // collection increases the indent level
            if( IsHIDItemCollection( mHidRepDescItem[ 0 ] ) )
                ++mHidIndentLevel;

            // start with a new packet
            mHidRepDescBytes = 0;
            memset( mHidRepDescItem, 0, sizeof mHidRepDescItem );

            mHidItemCnt++;
        }
        else
        {
            // we don't have the entire item, so just make a frame and store what data we do have
            pResults->AddFrame( pPacket->GetHIDItem( itemStartOffset, mPacketOffset - itemStartOffset, mHidRepDescItem, mHidIndentLevel,
                                                     GetHIDUsagePage(), FF_FieldIncomplete ) );
            while( mPacketDataBytes > mPacketOffset )
                mHidRepDescItem[ mHidRepDescBytes++ ] = pPacket->GetDataPayload( mPacketOffset++, 1 );
        }
    }
}

void USBControlTransferParser::ParseCDCDataStage()
{
    USBCDCRequestCode reqCode = ( USBCDCRequestCode )mRequest.bRequest;

    if( reqCode == GET_COMM_FEATURE || reqCode == SET_COMM_FEATURE )
    {
        mDescBytes = mRequest.wLength;
        mParsedOffset = 0;

        if( mRequest.wValue == 1 ) // ABSTRACT_STATE
            ParseStructure( CDCAbstractStateFields );
        else if( mRequest.wValue == 2 ) // COUNTRY_SETTING
            ParseStructure( CDCCountrySettingFields );
        else
            ParseUnknownResponse();
    }
    else if( reqCode == SET_LINE_CODING || reqCode == GET_LINE_CODING )
    {
        mDescBytes = mRequest.wLength;
        mParsedOffset = 0;
        ParseStructure( CDCLineCodingFields );
    }
    else if( reqCode == SET_RINGER_PARMS || reqCode == GET_RINGER_PARMS )
    {
        mDescBytes = mRequest.wLength;
        mParsedOffset = 0;
        ParseStructure( CDCRingerConfigFields );
    }
    else if( reqCode == GET_OPERATION_PARMS )
    {
        mDescBytes = mRequest.wLength;
        mParsedOffset = 0;
        ParseStructure( CDCOperationModeFields );
    }
    else if( reqCode == SET_LINE_PARMS || reqCode == GET_LINE_PARMS )
    {
        mDescBytes = mRequest.wLength;
        mParsedOffset = 0;
        ParseStructure( CDCLineParmsFields );
    }
    else if( reqCode == SET_UNIT_PARAMETER || reqCode == GET_UNIT_PARAMETER )
    {
        mDescBytes = mRequest.wLength;
        mParsedOffset = 0;
        ParseStructure( CDCUnitParameterFields );
    }
    else if( reqCode == GET_ETHERNET_STATISTIC || reqCode == GET_ATM_DEVICE_STATISTICS || reqCode == GET_ATM_VC_STATISTICS )
    {
        mDescBytes = mRequest.wLength;
        mParsedOffset = 0;
        ParseStructure( CDCUnsignedIntFields );
    }
    else if( reqCode == SET_ATM_DEFAULT_VC )
    {
        mDescBytes = mRequest.wLength;
        mParsedOffset = 0;
        ParseStructure( CDCATMDefaultVCFields );
    }
    else
    {
        ParseUnknownResponse();
    }
}

void USBControlTransferParser::ParseDataPacket( USBPacket& packet )
{
    pPacket = &packet;
    mPacketOffset = 0;
    mPacketDataBytes = packet.mData.size() - 4;

    if( mRequest.IsRequestedStandardDescriptor() )
    {
        while( mPacketDataBytes > mPacketOffset )
        {
            U8 interfaceClass = mInterfaceClasses[ mInterfaceNumber ];

            if( mParsedOffset == 0 ) // are we just starting with this descriptor?
            {
                // handle the bLength field
                pResults->AddFrame( packet.GetDataPayloadField( mPacketOffset, 1, mAddress, "bLength", Fld_None, FF_DataDescriptor ) );
                mDescBytes = packet.GetDataPayload( mPacketOffset, 1 );
                ++mPacketOffset;
                ++mParsedOffset;
            }
            else if( mParsedOffset == 1 )
            {
                mDescType = USBDescriptorType( packet.GetDataPayload( mPacketOffset, 1 ) );

                if( IsStandardDescriptor( mDescType ) || interfaceClass == CC_HID || interfaceClass == CC_CommunicationsAndCDCControl ||
                    interfaceClass == CC_CDCData )
                    pResults->AddFrame( packet.GetDataPayloadField( mPacketOffset, 1, mAddress, "bDescriptorType", Fld_bDescriptorType ) );
                else
                    pResults->AddFrame(
                        packet.GetDataPayloadField( mPacketOffset, 1, mAddress, "bDescriptorType", Fld_bDescriptorType_Other ) );

                ++mPacketOffset;
                ++mParsedOffset;
            }
            else if( mParsedOffset == 2 && ( interfaceClass == CC_CommunicationsAndCDCControl || interfaceClass == CC_CDCData ) &&
                     ( mDescType == DT_CDC_CS_INTERFACE || mDescType == DT_CDC_CS_ENDPOINT ) )
            {
                mDescSubtype = USBCDCDescriptorSubtype( packet.GetDataPayload( mPacketOffset, 1 ) );
                pResults->AddFrame(
                    packet.GetDataPayloadField( mPacketOffset, 1, mAddress, "bDescriptorSubtype", Fld_CDC_DescriptorSubtype ) );

                ++mPacketOffset;
                ++mParsedOffset;
            }
            else
            {
                if( mDescType == DT_STRING )
                {
                    bool detected_complete_descriptor = ParseStringDescriptor();
                    if( detected_complete_descriptor && mPacketDataBytes > mPacketOffset )
                    {
                        // we finished parsing the complete string descriptor, according to bLength, however there are still bytes left in the
                        // packet.
                        std::cerr << "String descriptor length shorter than packet length. Packet length: " << mPacketDataBytes
                                  << " processed bytes: " << mPacketOffset << " reported descriptor length: " << mDescBytes << "\n";
                        break;
                    }
                }
                else
                {
                    ParseStandardDescriptor();
                }
            }
        }
    }
    else if( mRequest.IsRequestedHIDReportDescriptor() )
    {
        ParseHIDReportDescriptor();
    }
    else if( IsCDCClassRequest() )
    {
        ParseCDCDataStage();
    }
    else
    {
        ParseUnknownResponse();
    }
}

void USBControlTransferPacketHandler::Init( USBAnalyzerResults* pResults, int address )
{
    mResults = pResults;
    mAddress = address;

    mCtrlTransLastReceived = CTS_StatusEnd;
    mCtrlTransParser.SetAnalyzerResults( mResults );
}

U64 USBControlTransferPacketHandler::ResetControlTransferParser( USBPacket& pckt, USBFrameFlags flag )
{
    mCtrlTransLastReceived = CTS_StatusEnd;
    mCtrlTransParser.ResetParser();
    return pckt.AddPacketFrames( mResults, flag );
}

U64 USBControlTransferPacketHandler::HandleControlTransfer( USBPacket& pckt )
{
    //
    // setup stage
    //

    if( pckt.mPID == PID_SETUP )
    {
        USBFrameFlags flag = mCtrlTransLastReceived == CTS_StatusEnd ? FF_None : FF_UnexpectedPacket;
        mCtrlTransLastReceived = CTS_SetupToken;
        return pckt.AddPacketFrames( mResults, flag );
    }

    if( mCtrlTransLastReceived == CTS_SetupToken )
    {
        // we must get a DATA0 after the SETUP pid
        if( pckt.mPID != PID_DATA0 )
            return ResetControlTransferParser( pckt );

        // remember the request code and the response direction
        mCtrlTransRequest = USBRequestCode( pckt.GetDataPayload( 1, 1 ) );
        mCtrlTransDeviceToHost = ( pckt.GetDataPayload( 0, 1 ) & 0x80 ) != 0;

        mCtrlTransParser.ResetParser();
        mCtrlTransParser.SetRequest( pckt );
        mCtrlTransParser.SetAddress( mAddress );

        mCtrlTransLastReceived = CTS_SetupData;
        return pckt.AddSetupPacketFrame( mResults, mCtrlTransParser, mAddress );
    }
    else if( mCtrlTransLastReceived == CTS_SetupData )
    {
        // the DATA0 setup request must be ACKed by the device
        if( pckt.mPID != PID_ACK )
            return ResetControlTransferParser( pckt );

        mCtrlTransLastReceived = CTS_SetupAck;
        return pckt.AddPacketFrames( mResults );
    }

    //
    // data stage
    //

    if( mCtrlTransLastReceived == CTS_SetupAck || mCtrlTransLastReceived == CTS_DataEnd )
    {
        if( pckt.mPID != PID_IN && pckt.mPID != PID_OUT )
            return ResetControlTransferParser( pckt );

        USBFrameFlags flag = mCtrlTransLastReceived == CTS_SetupAck ? FF_DataBegin : FF_None;

        if( mCtrlTransDeviceToHost )
            mCtrlTransLastReceived = pckt.mPID == PID_IN ? CTS_DataInToken : CTS_StatusOutToken;
        else
            mCtrlTransLastReceived = pckt.mPID == PID_OUT ? CTS_DataOutToken : CTS_StatusInToken;

        if( mCtrlTransLastReceived == CTS_StatusOutToken || mCtrlTransLastReceived == CTS_StatusInToken )
            flag = FF_StatusBegin;

        return pckt.AddPacketFrames( mResults, flag );
    }
    else if( mCtrlTransLastReceived == CTS_DataInToken )
    {
        if( pckt.mPID == PID_NAK )
        {
            mCtrlTransLastReceived = CTS_DataEnd;
            return pckt.AddPacketFrames( mResults, FF_DataInNAKed );
        }

        if( pckt.mPID == PID_STALL )
            return ResetControlTransferParser( pckt, FF_StatusEnd );

        if( pckt.mPID != PID_DATA0 && pckt.mPID != PID_DATA1 )
            return ResetControlTransferParser( pckt );

        mCtrlTransLastReceived = CTS_DataInData;

        return pckt.AddDataStageFrames( mResults, mCtrlTransParser, mAddress );
    }
    else if( mCtrlTransLastReceived == CTS_DataOutToken )
    {
        if( pckt.mPID != PID_DATA0 && pckt.mPID != PID_DATA1 )
            return ResetControlTransferParser( pckt );

        mCtrlTransLastReceived = CTS_DataOutData;

        return pckt.AddDataStageFrames( mResults, mCtrlTransParser, mAddress );
    }
    else if( mCtrlTransLastReceived == CTS_DataInData )
    {
        // a host can only ACK the data in packet
        if( pckt.mPID != PID_ACK )
            return ResetControlTransferParser( pckt );

        mCtrlTransLastReceived = CTS_DataEnd;

        return pckt.AddPacketFrames( mResults );
    }
    else if( mCtrlTransLastReceived == CTS_DataOutData )
    {
        if( pckt.mPID == PID_STALL )
            return ResetControlTransferParser( pckt, FF_DataEnd );

        if( pckt.mPID != PID_NAK && pckt.mPID != PID_ACK )
            return ResetControlTransferParser( pckt );

        mCtrlTransLastReceived = CTS_DataEnd;

        return pckt.AddPacketFrames( mResults, pckt.mPID == PID_NAK ? FF_DataOutNAKed : FF_None );
    }

    //
    // status stage - IN/OUT tokens of the status stage are handled in the first block of the data stage
    //

    if( mCtrlTransLastReceived == CTS_StatusInToken )
    {
        if( pckt.mPID != PID_DATA0 && pckt.mPID != PID_DATA1 && pckt.mPID != PID_STALL && pckt.mPID != PID_NAK )
            return ResetControlTransferParser( pckt );

        USBFrameFlags flag;
        if( pckt.mPID == PID_STALL )
        {
            mCtrlTransLastReceived = CTS_StatusEnd;
            flag = FF_StatusEnd;
        }
        else if( pckt.mPID == PID_NAK )
        {
            mCtrlTransLastReceived = CTS_StatusInNAKed;
            flag = FF_StatusInNAKed;
        }
        else
        { // DATA0/1
            mCtrlTransLastReceived = CTS_StatusInDataEmpty;
            flag = FF_None;
        }

        return pckt.AddPacketFrames( mResults, flag );
    }
    else if( mCtrlTransLastReceived == CTS_StatusOutToken )
    {
        if( pckt.mPID != PID_DATA0 && pckt.mPID != PID_DATA1 )
            return ResetControlTransferParser( pckt );

        mCtrlTransLastReceived = CTS_StatusOutDataEmpty;

        return pckt.AddPacketFrames( mResults );
    }
    else if( mCtrlTransLastReceived == CTS_StatusInDataEmpty )
    {
        if( pckt.mPID != PID_ACK )
            return ResetControlTransferParser( pckt );

        mCtrlTransLastReceived = CTS_StatusEnd;

        return pckt.AddPacketFrames( mResults, FF_StatusEnd );
    }
    else if( mCtrlTransLastReceived == CTS_StatusOutDataEmpty )
    {
        if( pckt.mPID != PID_ACK && pckt.mPID != PID_STALL && pckt.mPID != PID_NAK )
            return ResetControlTransferParser( pckt );

        USBFrameFlags flag;
        if( pckt.mPID == PID_ACK || pckt.mPID == PID_STALL )
        {
            mCtrlTransLastReceived = CTS_StatusEnd;
            flag = FF_StatusEnd;
        }
        else
        {
            mCtrlTransLastReceived = CTS_StatusOutDataNAKed;
            flag = FF_StatusOutNAKed;
        }

        return pckt.AddPacketFrames( mResults, flag );
    }
    else if( mCtrlTransLastReceived == CTS_StatusOutDataNAKed )
    {
        if( pckt.mPID != PID_OUT )
            return ResetControlTransferParser( pckt );

        mCtrlTransLastReceived = CTS_StatusOutToken;

        return pckt.AddPacketFrames( mResults );
    }
    else if( mCtrlTransLastReceived == CTS_StatusInNAKed )
    {
        if( pckt.mPID != PID_IN )
            return ResetControlTransferParser( pckt );

        mCtrlTransLastReceived = CTS_StatusInToken;

        return pckt.AddPacketFrames( mResults );
    }

    assert( mCtrlTransLastReceived == CTS_StatusEnd );

    return ResetControlTransferParser( pckt, FF_None );
}
