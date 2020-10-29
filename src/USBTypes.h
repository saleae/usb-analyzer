#ifndef USB_TYPES_H
#define USB_TYPES_H

#include <map>
#include <vector>

#include <LogicPublicTypes.h>
#include <AnalyzerResults.h>
#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>

#include "USBEnums.h"

class USBAnalyzerResults;
class USBControlTransferParser;

struct USBPacket
{
    U64 mSampleBegin;
    U64 mSampleEnd;

    // data member contains all the data in the packet starting from the SYNC field all the way to the CRC
    // so:
    // data[0] SYNC
    // data[1] PID
    //   data[2..n-3] payload (for packets > 4 bytes)
    //   data[n-2..n-1] CRC (for packets > 4 bytes)
    //   data[2..3] address, endpoint and CRC (for packets == 4 bytes)
    std::vector<U8> mData;

    // sample number of the individual bits
    // the last element if this vector will hold the ending sample of the last data bit
    // this means the number of elements in the vector will be == num_bytes*8 + 1
    std::vector<U64> mBitBeginSamples;

    USB_PID mPID;
    U16 mCRC; // used for both 16 and 5 bit CRC values

    void Clear();

    U16 GetLastWord() const
    {
        return ( mData.back() << 8 ) | *( mData.end() - 2 );
    }

    U8 GetAddress() const
    {
        return mData[ 2 ] & 0x7F;
    }

    U8 GetEndpoint() const
    {
        return ( GetLastWord() >> 7 ) & 0xf;
    }

    U16 GetFrameNum() const
    {
        return GetLastWord() & 0x7ff;
    }

    bool IsTokenPacket() const
    {
        return mPID == PID_IN || mPID == PID_OUT || mPID == PID_SETUP;
    }

    bool IsSOFPacket() const
    {
        return mPID == PID_SOF;
    }

    bool IsDataPacket() const
    {
        return mPID == PID_DATA0 || mPID == PID_DATA1;
    }

    bool IsHandshakePacket() const
    {
        return mPID == PID_ACK || mPID == PID_NAK || mPID == PID_STALL;
    }

    bool IsPIDValid() const;

    static U8 CalcCRC5( U16 data );
    U16 CalcCRC16() const;

    void AddSyncAndPidFrames( USBAnalyzerResults* pResults, USBFrameFlags flagPID = FF_None );
    void AddEOPFrame( USBAnalyzerResults* pResults );
    void AddCRC16Frame( USBAnalyzerResults* pResults );

    U64 AddPacketFrames( USBAnalyzerResults* pResults, USBFrameFlags flagPID = FF_None );
    U64 AddRawByteFrames( USBAnalyzerResults* pResults );
    U64 AddErrorFrame( USBAnalyzerResults* pResults );

    // control transfer decoders
    // these are defined in USBControlTransfer.cpp
    U64 AddSetupPacketFrame( USBAnalyzerResults* pResults, USBControlTransferParser& parser, U8 address );
    void AddStandardSetupPacketFrame( USBAnalyzerResults* pResults, USBControlTransferParser& parser, U8 address );
    void AddClassSetupPacketFrame( USBAnalyzerResults* pResults, USBControlTransferParser& parser, U8 address );
    void AddVendorSetupPacketFrame( USBAnalyzerResults* pResults, USBControlTransferParser& parser, U8 address );
    U64 AddDataStageFrames( USBAnalyzerResults* pResults, USBControlTransferParser& parser, U8 address );

    Frame GetDataPayloadField( int ndx, int bcnt, U8 address, const char* name, USBCtrlTransFieldType fldHandler = Fld_None,
                               U8 flags = 0 ) const;
    U32 GetDataPayload( int ndx, int bcnt ) const;

    Frame GetHIDItem( int offset, int bcnt, U8* pItem, U16 indentLevel, U16 usagePage, U8 flags ) const;
};

const double FS_BIT_DUR = ( 1000 / 12.0 ); // 83.3 ns
const double LS_BIT_DUR = ( 1000 / 1.5 );  // 666.7 ns

struct USBSignalState
{
    U64 mSampleBegin;
    U64 mSampleEnd;

    USBState mState;
    double mDur; // in nano sec

    bool IsDurationFS() const
    {
        return mDur > FS_BIT_DUR * .3 && mDur < FS_BIT_DUR * 7.5;
    }

    bool IsDurationLS() const
    {
        return mDur > LS_BIT_DUR * .7 && mDur < LS_BIT_DUR * 7.3;
    }

    bool IsData( const USBSpeed speed ) const
    {
        return ( speed == LOW_SPEED ? IsDurationLS() : IsDurationFS() ) && ( mState == S_J || mState == S_K );
    }

    int GetNumBits( USBSpeed speed ) const
    {
        const double BIT_DUR = ( speed == FULL_SPEED ? FS_BIT_DUR : LS_BIT_DUR );
        return int( mDur / BIT_DUR + 0.5 );
    }

    void AddFrame( USBAnalyzerResults* res );
};

class USBAnalyzer;
class USBAnalyzerResults;
class USBAnalyzerSettings;

class USBSignalFilter
{
  private:
    AnalyzerChannelData* mDP;
    AnalyzerChannelData* mDM;

    USBAnalyzer* mAnalyzer;
    USBAnalyzerResults* mResults;
    USBAnalyzerSettings* mSettings;

    USBSpeed mSpeed;         // LS or FS
    bool mExpectLowSpeed;    // this is set to true after a PRE packet
    const double mSampleDur; // in ns
    U64 mStateStartSample;   // used for filtered signal state start between calls

    bool SkipNoise( AnalyzerChannelData* pNearer, AnalyzerChannelData* pFurther );
    U64 DoFilter( AnalyzerChannelData* mDP, AnalyzerChannelData* mDM );

  public:
    USBSignalFilter( USBAnalyzer* pAnalyzer, USBAnalyzerResults* pResults, USBAnalyzerSettings* pSettings, AnalyzerChannelData* pDP,
                     AnalyzerChannelData* pDM, USBSpeed speed );

    bool HasMoreData();
    USBSignalState GetState();
    bool IsDataSignal( const USBSignalState& s );
    bool GetPacket( USBPacket& pckt, USBSignalState& sgnl );
    USBSpeed GetCurrSpeed() const
    {
        return mSpeed;
    }
};

std::string int2str_sal( const U64 i, DisplayBase base, const int max_bits = 8 );

inline std::string int2str( const U64 i )
{
    return int2str_sal( i, Decimal, 64 );
}

/*
#ifdef _WINDOWS
# include <windows.h>
#endif

// debugging helper functions -- Windows only!!!
inline void debug(const std::string& str)
{
#if !defined(NDEBUG)  &&  defined(_WINDOWS)
    ::OutputDebugStringA(("----- " + str + "\n").c_str());
#endif
}

inline void debug(const char* str)
{
#if !defined(NDEBUG)  &&  defined(_WINDOWS)
    debug(std::string(str));
#endif
}
*/

#endif // USB_TYPES_H