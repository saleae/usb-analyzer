#ifndef USB_CONTROL_TRANSFERS_H
#define USB_CONTROL_TRANSFERS_H

#include <map>

#include <LogicPublicTypes.h>
#include <AnalyzerResults.h>

#include "USBEnums.h"

struct USBRequest
{
    U8 bmRequestType;
    U8 bRequest;
    U16 wValue;
    U16 wIndex;
    U16 wLength;

    USBRequest()
    {
        Clear();
    }

    void Clear()
    {
        bmRequestType = 0;
        bRequest = 0;
        wValue = 0;
        wIndex = 0;
        wLength = 0;
    }

    void SetFromPacket( USBPacket& p );

    U8 GetRequestedDescriptor() const
    {
        return wValue >> 8;
    } // only valid when bRequest == GET_DESCRIPTOR
    U8 GetRequestedDescriptorIndex() const
    {
        return wValue & 0xff;
    } // only valid when bRequest == GET_DESCRIPTOR  &&  GetRequestedDescriptor() == DT_STRING

    bool IsStandardRequest() const
    {
        return ( bmRequestType & 0x60 ) == 0x00;
    }
    bool IsClassRequest() const
    {
        return ( bmRequestType & 0x60 ) == 0x20;
    }
    bool IsVendorRequest() const
    {
        return ( bmRequestType & 0x60 ) == 0x40;
    }

    bool IsRecipientDevice() const
    {
        return ( bmRequestType & 0x1f ) == 0x00;
    }
    bool IsRecipientInterface() const
    {
        return ( bmRequestType & 0x1f ) == 0x01;
    }
    bool IsRecipientEndpoint() const
    {
        return ( bmRequestType & 0x1f ) == 0x02;
    }
    bool IsRecipientOther() const
    {
        return ( bmRequestType & 0x1f ) == 0x03;
    }

    U8 GetInterfaceNum() const
    {
        return wValue & 0xff;
    }

    bool IsRequestedStandardDescriptor() const
    {
        return IsStandardRequest() && bRequest == GET_DESCRIPTOR && GetRequestedDescriptor() >= DT_DEVICE &&
               GetRequestedDescriptor() <= DT_INTERFACE_POWER;
    }

    bool IsRequestedHIDReportDescriptor() const
    {
        return IsStandardRequest() && bRequest == GET_DESCRIPTOR && GetRequestedDescriptor() == DT_HID_REPORT;
    }
};

/*

// Control transfer descriptor structures - these are here only for reference

struct USBDeviceDescriptor
{
    U8		bLength;
    U8		bDescriptorType;
    U16		bcdUSB;
    U8		bDeviceClass;
    U8		bDeviceSubClass;
    U8		bDeviceProtocol;
    U8		bMaxPacketSize0;
    U16		idVendor;
    U16		idProduct;
    U16		bcdDevice;
    U8		iManufacturer;
    U8		iProduct;
    U8		iSerialNumber;
    U8		bNumConfigurations;
};

struct USBConfigurationDescriptor
{
    U8		bLength;
    U8		bDescriptorType;
    U16		wTotalLength;
    U8		bNumInterfaces;
    U8		bConfigurationValue;
    U8		iConfiguration;
    U8		bmAttributes;
    U8		bMaxPower;
};

struct USBInterfaceDescriptor
{
    U8		bLength;
    U8		bDescriptorType;
    U8		bInterfaceNumber;
    U8		bAlternateSetting;
    U8		bNumEndpoints;
    U8		bInterfaceClass;
    U8		bInterfaceSubClass;
    U8		bInterfaceProtocol;
    U8		iInterface;
};

struct USBEndpointDescriptor
{
    U8		bLength;
    U8		bDescriptorType;
    U8		bEndpointAddress;
    U8		bmAttributes;
    U8		wMaxPacketSize;
    U8		bInterval;
};
*/

class USBCtrlTransFieldFrame : public Frame
{
  public:
    USBCtrlTransFieldFrame()
    {
        mType = FT_ControlTransferField;
        mFlags = FF_None;
    }

    void PackFrame( U32 data, U8 numBytes, U8 address, USBCtrlTransFieldType formatter, const char* name );

    U32 GetData() const
    {
        return mData1 & 0xffffffff;
    }

    U8 GetNumBytes() const
    {
        return ( mData1 >> 32 ) & 0xff;
    }

    USBCtrlTransFieldType GetFormatter() const
    {
        return USBCtrlTransFieldType( ( mData1 >> 40 ) & 0xff );
    }

    U8 GetAddress() const
    {
        return ( mData1 >> 48 ) & 0xff;
    }

    const char* GetFieldName() const
    {
        return ( const char* )mData2;
    }
};

class USBHidRepDescItemFrame : public Frame
{
  public:
    USBHidRepDescItemFrame()
    {
        mType = FT_HIDReportDescriptorItem;
        mFlags = FF_None;
        mData1 = mData2 = 0;
    }

    void PackFrame( const U8* pItem, U16 indentLevel, U16 usagePage );

    const U8* GetItem() const
    {
        return ( const U8* )&mData1;
    }

    U16 GetIndentLevel() const
    {
        return mData2 & 0xffff;
    }

    U16 GetUsagePage() const
    {
        return ( U16 )( mData2 >> 16 );
    }
};

// describes data structures
struct USBStructField
{
    const char* name;
    int numBytes;
    USBCtrlTransFieldType formatter;
};

class USBControlTransferParser
{
  private:
    int mDescBytes; // size of this descriptor in bytes; equal to Descriptor.bLength
    USBDescriptorType mDescType;
    USBCDCDescriptorSubtype mDescSubtype;
    int mParsedOffset; // number of bytes we have already parsed from the current descriptor
    U32 mLeftover;     // the LS byte of the previous packet in case a field spans two packets

    USBAnalyzerResults* pResults;
    U8 mAddress;

    USBRequest mRequest;

    std::string stringDescriptor;

    // these are only valid during a ParsePacket call, and invalid before and after
    USBPacket* pPacket;
    int mPacketOffset;    // index of the data byte of the current packet
    int mPacketDataBytes; // number of bytes in the current packet

    // we store the class IDs for the device and the interfaces
    U8 mInterfaceNumber; // the last parsed interface number
    typedef std::map<U8, USBClassCodes> USBInterfaceClassesContainer;

    USBInterfaceClassesContainer mInterfaceClasses;

    // used for the HID report descriptor parser
    U8 mHidRepDescItem[ 5 ];
    U8 mHidRepDescBytes;
    int mHidIndentLevel;
    int mHidItemCnt;
    std::vector<U16> mHidUsagePageStack;

    USBClassCodes GetClassCodeForInterface( U8 iface ) const;

    bool IsCDCClassRequest() const;

    void ParseStringDescriptor();
    void ParseStructure( USBStructField* descFields );
    void ParseStandardDescriptor();

    void SetHIDUsagePage( U16 usagePage )
    {
        if( mHidUsagePageStack.empty() )
            mHidUsagePageStack.push_back( 0 );

        mHidUsagePageStack.back() = usagePage;
    }

    U16 GetHIDUsagePage()
    {
        if( mHidUsagePageStack.empty() )
            return 0;

        return mHidUsagePageStack.back();
    }

    void PushHIDUsagePage()
    {
        mHidUsagePageStack.push_back( GetHIDUsagePage() );
    }

    void PopHIDUsagePage()
    {
        if( !mHidUsagePageStack.empty() )
            mHidUsagePageStack.pop_back();
    }

    void ParseCDCDataStage();
    void ParseHIDReportDescriptor();
    void ParseUnknownResponse();

  public:
    USBControlTransferParser()
    {
        ResetParser();
    }

    void SetAnalyzerResults( USBAnalyzerResults* pres )
    {
        pResults = pres;
    }

    void ResetParser();

    void SetAddress( U8 address )
    {
        mAddress = address;
    }

    void SetRequest( USBPacket& packet )
    {
        mRequest.SetFromPacket( packet );
    }

    void ParseDataPacket( USBPacket& packet );

    U8 GetClassForInterface( U8 iface ) const
    {
        // get the interface class
        USBInterfaceClassesContainer::const_iterator srch( mInterfaceClasses.find( iface ) );
        if( srch != mInterfaceClasses.end() )
            return srch->second;

        return 0;
    }
};


class USBControlTransferPacketHandler
{
  private:
    enum ControlTransferLastPacketReceived
    {
        // setup stage
        CTS_SetupToken,
        CTS_SetupData,
        CTS_SetupAck,

        // data stage
        CTS_DataInToken,
        CTS_DataOutToken,
        CTS_DataInData,
        CTS_DataOutData,
        CTS_DataEnd,

        // status stage
        CTS_StatusInToken,
        CTS_StatusOutToken,
        CTS_StatusInDataEmpty,
        CTS_StatusOutDataEmpty,
        CTS_StatusInNAKed,
        CTS_StatusOutDataNAKed,
        CTS_StatusEnd,
    };

    // control transfer state
    ControlTransferLastPacketReceived mCtrlTransLastReceived;
    USBRequestCode mCtrlTransRequest;
    bool mCtrlTransDeviceToHost;
    USBControlTransferParser mCtrlTransParser;
    int mAddress;

    USBAnalyzerResults* mResults;

  public:
    void Init( USBAnalyzerResults* pResults, int addr );

    U64 ResetControlTransferParser( USBPacket& pckt, USBFrameFlags flag = FF_UnexpectedPacket );
    U64 HandleControlTransfer( USBPacket& pckt );
};

inline int GetNumHIDItemDataBytes( U8 firstByte )
{
    U8 retVal = firstByte & 0x03;
    return retVal == 3 ? 4 : retVal;
}

inline bool IsStandardDescriptor( U8 descType )
{
    return descType >= DT_DEVICE && descType <= DT_INTERFACE_POWER;
}

inline bool IsHIDItemCollection( U8 firstByte )
{
    return ( firstByte & 0xfc ) == 0xa0;
}
inline bool IsHIDItemEndCollection( U8 firstByte )
{
    return ( firstByte & 0xfc ) == 0xc0;
}
inline bool IsHIDItemPush( U8 firstByte )
{
    return ( firstByte & 0xfc ) == 0xa4;
}
inline bool IsHIDItemPop( U8 firstByte )
{
    return ( firstByte & 0xfc ) == 0xb4;
}
inline bool IsHIDItemUsagePage( U8 firstByte )
{
    return ( firstByte & 0xfc ) == 0x04;
}

#endif // USB_CONTROL_TRANSFERS_H
