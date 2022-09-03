#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>

#include "USBAnalyzer.h"
#include "USBAnalyzerResults.h"
#include "USBTypes.h"

std::string GetPIDName( USB_PID pid )
{
    switch( pid )
    {
    case PID_IN:
        return "IN";
    case PID_OUT:
        return "OUT";
    case PID_SOF:
        return "SOF";
    case PID_SETUP:
        return "SETUP";
    case PID_DATA0:
        return "DATA0";
    case PID_DATA1:
        return "DATA1";
    case PID_ACK:
        return "ACK";
    case PID_NAK:
        return "NAK";
    case PID_STALL:
        return "STALL";
    case PID_PRE:
        return "PRE";
    }

    return "<invalid>";
}

void USBPacket::Clear()
{
    mData.clear();
    mBitBeginSamples.clear();
    mSampleBegin = mSampleEnd = 0;
    mPID = PID_Unknown;
    mCRC = 0;
}

bool USBPacket::IsPIDValid() const
{
    return mPID == PID_IN || mPID == PID_OUT || mPID == PID_SOF || mPID == PID_SETUP || mPID == PID_DATA0 || mPID == PID_DATA1 ||
           mPID == PID_ACK || mPID == PID_NAK || mPID == PID_STALL || mPID == PID_PRE;
}

U8 USBPacket::CalcCRC5( U16 data )
{
    U8 crc_register = 0x1f;
    U8 polynom = 0x14; // 0b10100

    U8 data_bit, crc_bit;
    U16 shift_register;

    // only the lower 11 bits of the 16 bit number
    for( shift_register = 1; shift_register <= 0x400; shift_register <<= 1 )
    {
        data_bit = ( data & shift_register ) ? 1 : 0;
        crc_bit = crc_register & 1;

        crc_register >>= 1;

        if( data_bit ^ crc_bit )
            crc_register ^= polynom;
    }

    return ( ~crc_register ) & 0x1f;
}

U16 USBPacket::CalcCRC16() const
{
    size_t counter;
    U16 crc_register = 0xffff;
    U16 polynom = 0xA001;
    U8 shift_register, data_bit, crc_bit;

    for( counter = 2; counter < mData.size() - 2; counter++ )
    {
        for( shift_register = 0x01; shift_register > 0x00; shift_register <<= 1 )
        {
            data_bit = ( mData[ counter ] & shift_register ) ? 1 : 0;
            crc_bit = crc_register & 1;

            crc_register >>= 1;

            if( data_bit ^ crc_bit )
                crc_register ^= polynom;
        }
    }

    return ~crc_register;
}

void USBPacket::AddSyncAndPidFrames( USBAnalyzerResults* pResults, USBFrameFlags flagPID )
{
    // make the SYNC and PID analyzer frames for this packet
    Frame f;

    // SYNC
    f.mStartingSampleInclusive = mBitBeginSamples.front();
    f.mEndingSampleInclusive = *( mBitBeginSamples.begin() + 8 );
    f.mType = FT_SYNC;
    f.mData1 = f.mData2 = 0;
    f.mFlags = FF_None;
    pResults->AddFrame( f );
    FrameV2 frame_v2_sync;
    pResults->AddFrameV2( frame_v2_sync, "sync", mBitBeginSamples.front(), *(mBitBeginSamples.begin() + 8) );

    // PID
    f.mStartingSampleInclusive = *( mBitBeginSamples.begin() + 8 );
    f.mEndingSampleInclusive = *( mBitBeginSamples.begin() + 16 );
    f.mType = FT_PID;
    f.mData1 = mPID;
    f.mData2 = 0;
    f.mFlags = flagPID;
    pResults->AddFrame( f );
    FrameV2 frame_v2_pid;
    //frame_v2_pid.AddByte( "pid", ( U8 )mPID );
    frame_v2_pid.AddString( "value", GetPIDName( mPID ).c_str() );
    pResults->AddFrameV2( frame_v2_pid, "pid",*( mBitBeginSamples.begin() + 8), *( mBitBeginSamples.begin() + 16) );
}

void USBPacket::AddEOPFrame( USBAnalyzerResults* pResults )
{
    Frame f;

    // add the EOP frame
    f.mStartingSampleInclusive = mBitBeginSamples.back();
    // EOP is 2 bits SE0 and one bit J, so add another bit
    f.mEndingSampleInclusive = mSampleEnd;
    f.mData1 = f.mData2 = 0;
    f.mFlags = FF_None;
    f.mType = FT_EOP;
    pResults->AddFrame( f );

    FrameV2 frame_v2_eop;
    pResults->AddFrameV2( frame_v2_eop, "eop",*( mBitBeginSamples.begin() + 8), *( mBitBeginSamples.begin() + 16) );
}

void USBPacket::AddCRC16Frame( USBAnalyzerResults* pResults )
{
    Frame f;

    // CRC16
    f.mStartingSampleInclusive = *( mBitBeginSamples.end() - 17 );
    f.mEndingSampleInclusive = mBitBeginSamples.back();

    f.mType = FT_CRC16;
    f.mData1 = mCRC;
    f.mData2 = CalcCRC16();
    pResults->AddFrame( f );

    FrameV2 frame_v2_crc;
    U8 crc_bytearray[ 2 ];
    U8 ccrc_bytearray[ 2 ];

    crc_bytearray[ 0 ] = ( U8 )( f.mData1 >> 8 );
    crc_bytearray[ 1 ] = ( U8 )( f.mData1 >> 0 );
    ccrc_bytearray[ 0 ] = ( U8 )(f.mData2 >> 8 );
    ccrc_bytearray[ 1 ] = ( U8 )(f.mData2 >> 0 );

    frame_v2_crc.AddByteArray( "value", crc_bytearray, 2 );
    frame_v2_crc.AddByteArray( "value2", ccrc_bytearray, 2 );
    pResults->AddFrameV2( frame_v2_crc, "crc16", *( mBitBeginSamples.end() - 17 ), mBitBeginSamples.back() );
}

U64 USBPacket::AddPacketFrames( USBAnalyzerResults* pResults, USBFrameFlags flagPID )
{
    AddSyncAndPidFrames( pResults, flagPID );

    // make the analyzer frames for this packet
    Frame f;
    f.mFlags = FF_None;
    FrameV2 fv2;
    const char* fv2_type;
    U8 frame_bytearray[ 2 ];

    // do the payload & CRC frames
    if( IsTokenPacket() || IsSOFPacket() )
    {
        // address/endpoint  or  frame number
        f.mStartingSampleInclusive = *( mBitBeginSamples.begin() + 16 );
        f.mEndingSampleInclusive = *( mBitBeginSamples.begin() + 27 );

        // is this a SOF packet?
        if( mPID == PID_SOF )
        {
            f.mType = FT_FrameNum;
            f.mData1 = GetFrameNum();
            f.mData2 = 0;
            frame_bytearray[ 0 ] = ( U8 )(f.mData1 >> 8);
            frame_bytearray[ 1 ] = ( U8 )(f.mData1 >> 0);
            fv2.AddByteArray( "value", frame_bytearray, 2 );
            fv2_type = "frame";
        }
        else
        {
            f.mType = FT_AddrEndp;
            f.mData1 = GetAddress();
            f.mData2 = GetEndpoint();
            fv2.AddByte( "value", ( U8 )f.mData1 );
            fv2.AddByte( "value2", ( U8 )f.mData2 );
            fv2_type = "addrendp";
        }

        pResults->AddFrame( f );
        pResults->AddFrameV2( fv2, fv2_type, *( mBitBeginSamples.begin() + 16 ), *( mBitBeginSamples.begin() + 27 ) );

        // CRC5
        f.mStartingSampleInclusive = *( mBitBeginSamples.begin() + 27 );
        f.mEndingSampleInclusive = mBitBeginSamples.back();

        f.mType = FT_CRC5;
        f.mData1 = mCRC;
        f.mData2 = CalcCRC5( GetLastWord() & 0x7ff );
        pResults->AddFrame( f );

        FrameV2 frame_v2_crc;
        U8 crc_bytearray[ 2 ];
        U8 ccrc_bytearray[ 2 ];

        crc_bytearray[ 0 ] = ( U8 )(f.mData1 >> 8);
        crc_bytearray[ 1 ] = ( U8 )(f.mData1 >> 0);
        ccrc_bytearray[ 0 ] = ( U8 )(f.mData2 >> 8);
        ccrc_bytearray[ 1 ] = ( U8 )(f.mData2 >> 0);

        frame_v2_crc.AddByteArray( "value", crc_bytearray, 2 );
        frame_v2_crc.AddByteArray( "value2", ccrc_bytearray, 2 );
        pResults->AddFrameV2( frame_v2_crc, "crc5", *( mBitBeginSamples.begin() + 27 ), mBitBeginSamples.back() );
    }
    else if( IsDataPacket() )
    {
        // raw data
        FrameV2 fv2;
        size_t bc;
        f.mType = FT_Byte;
        f.mData2 = 0;
        for( bc = 2; bc < mData.size() - 2; ++bc )
        {
            f.mStartingSampleInclusive = *( mBitBeginSamples.begin() + bc * 8 );
            f.mEndingSampleInclusive = *( mBitBeginSamples.begin() + ( bc + 1 ) * 8 );
            f.mData1 = mData[ bc ];

            pResults->AddFrame( f );
            fv2.AddByte( "data", mData[ bc ] );
            pResults->AddFrameV2( fv2, "data", *( mBitBeginSamples.begin() + bc * 8 ), *( mBitBeginSamples.begin() + ( bc + 1 ) * 8 ) );
        }

        AddCRC16Frame( pResults );
    }

    if( mPID != PID_PRE )
        AddEOPFrame( pResults );

    pResults->CommitResults();

    return mSampleEnd;
}

U64 USBPacket::AddRawByteFrames( USBAnalyzerResults* pResults )
{
    // raw data
    size_t bc;
    Frame f;
    FrameV2 fv2;
    f.mType = FT_Byte;
    f.mData2 = 0;
    f.mFlags = FF_None;
    //std::string bytes_row;
    for( bc = 0; bc < mData.size(); ++bc )
    {
        //bytes_row += int2str_sal( mData[ bc ], Hexadecimal, 8 ) + ", ";

        f.mStartingSampleInclusive = *( mBitBeginSamples.begin() + bc * 8 );
        f.mEndingSampleInclusive = *( mBitBeginSamples.begin() + ( bc + 1 ) * 8 );
        f.mData1 = mData[ bc ];
        pResults->AddFrame( f );
        fv2.AddByte( "data", mData[ bc ] );
        pResults->AddFrameV2( fv2, "data", *( mBitBeginSamples.begin() + bc * 8 ), *( mBitBeginSamples.begin() + ( bc + 1 ) * 8 ) );
    }

    // add the EOP frame
    f.mStartingSampleInclusive = mBitBeginSamples.back();
    // EOP is 2 bits SE0 and one bit J, so add another bit
    f.mEndingSampleInclusive = mSampleEnd;
    f.mData1 = f.mData2 = 0;
    f.mFlags = FF_None;
    f.mType = FT_EOP;
    pResults->AddFrame( f );
    FrameV2 frame_v2_eop;
    pResults->AddFrameV2( frame_v2_eop, "eop", mBitBeginSamples.back(), mSampleEnd );

    pResults->CommitResults();

    return f.mEndingSampleInclusive;
}

U64 USBPacket::AddErrorFrame( USBAnalyzerResults* pResults )
{
    // add the Error frame -- parser can't decode the packet
    Frame f;
    f.mStartingSampleInclusive = mSampleBegin;
    f.mEndingSampleInclusive = mSampleEnd;
    f.mData1 = f.mData2 = 0;
    f.mFlags = FF_None;
    f.mType = FT_Error;
    pResults->AddFrame( f );

    FrameV2 frame_v2_error;
    pResults->AddFrameV2( frame_v2_error, "error", mSampleBegin, mSampleEnd );

    pResults->CommitResults();

    return f.mEndingSampleInclusive;
}

void USBSignalState::AddFrame( USBAnalyzerResults* res )
{
    Frame f;
    FrameV2 fv2;
    f.mStartingSampleInclusive = mSampleBegin;
    f.mEndingSampleInclusive = mSampleEnd;
    f.mType = FT_Signal;
    f.mData1 = mState;
    f.mData2 = 0;

    res->AddFrame( f );
    fv2.AddByte( "data", mState );
    res->AddFrameV2( fv2, "signal", mSampleBegin, mSampleEnd );
    
    res->CommitResults();
}

USBSignalFilter::USBSignalFilter( USBAnalyzer* pAnalyzer, USBAnalyzerResults* pResults, USBAnalyzerSettings* pSettings,
                                  AnalyzerChannelData* pDP, AnalyzerChannelData* pDM, USBSpeed speed )
    : mAnalyzer( pAnalyzer ),
      mResults( pResults ),
      mSettings( pSettings ),
      mDP( pDP ),
      mDM( pDM ),
      mSpeed( speed ),
      mExpectLowSpeed( false ),
      mSampleDur( 1e9 / mAnalyzer->GetSampleRate() )
{
    mStateStartSample = mDP->GetSampleNumber();
}

bool USBSignalFilter::SkipNoise( AnalyzerChannelData* pNearer, AnalyzerChannelData* pFurther )
{
    if( mSampleDur > 20 // sample rate < 50Mhz?
        || mSpeed == FULL_SPEED )
        return false;

    // up to 20ns
    const U32 IGNORE_PULSE_SAMPLES = mSampleDur == 10 ? 2 : 1;

    // skip the glitch
    if( pNearer->WouldAdvancingCauseTransition( IGNORE_PULSE_SAMPLES ) )
    {
        pNearer->AdvanceToNextEdge();
        pFurther->AdvanceToAbsPosition( pNearer->GetSampleNumber() );

        return true;
    }

    return false;
}

U64 USBSignalFilter::DoFilter( AnalyzerChannelData* mDP, AnalyzerChannelData* mDM )
{
    AnalyzerChannelData* pFurther;
    AnalyzerChannelData* pNearer;
    U64 next_edge_further;
    U64 next_edge_nearer;

    // this loop consumes all short (1 sample) pulses caused by noise and/or high speed signals
    do
    {
        if( mDP->GetSampleOfNextEdge() > mDM->GetSampleOfNextEdge() )
            pFurther = mDP, pNearer = mDM;
        else
            pFurther = mDM, pNearer = mDP;

        next_edge_further = pFurther->GetSampleOfNextEdge();
        next_edge_nearer = pNearer->GetSampleOfNextEdge();

        pNearer->AdvanceToNextEdge();
        pFurther->AdvanceToAbsPosition( next_edge_nearer );
    } while( SkipNoise( pNearer, pFurther ) );

    const int FILTER_THLD = mSettings->mSpeed == LOW_SPEED ? 300 : 50; // filtering threshold in ns

    U64 diff_samples = next_edge_further - next_edge_nearer;
    // if there's a pulse on pNearer and no transition on pFurther
    if( !pNearer->WouldAdvancingToAbsPositionCauseTransition( next_edge_further )
        // if the transitions happened withing FILTER_THLD time of eachother
        && diff_samples * mSampleDur <= FILTER_THLD )
    {
        for( ;; )
        {
            pNearer->AdvanceToAbsPosition( next_edge_further );
            pFurther->AdvanceToAbsPosition( next_edge_further );

            if( !SkipNoise( pFurther, pNearer ) )
                break;

            pFurther->AdvanceToNextEdge();
            next_edge_further = pFurther->GetSampleNumber();
        }

        // return the filtered position of the transition
        return ( next_edge_nearer + next_edge_further ) / 2;
    }

    // return state up until the nearer transition
    return next_edge_nearer;
}

USBSignalState USBSignalFilter::GetState()
{
    USBSignalState ret_val;

    ret_val.mSampleBegin = mStateStartSample;

    // determine the USB signal state
    BitState dp_state = mDP->GetBitState();
    BitState dm_state = mDM->GetBitState();
    if( dp_state == dm_state )
        ret_val.mState = dp_state == BIT_LOW ? S_SE0 : S_SE1;
    else
        ret_val.mState = ( mSpeed == LOW_SPEED ? ( dp_state == BIT_LOW ? S_J : S_K ) : ( dp_state == BIT_LOW ? S_K : S_J ) );

    // do the filtering and remember the sample begin for the next iteration
    mStateStartSample = DoFilter( mDP, mDM );

    ret_val.mDur = ( mStateStartSample - ret_val.mSampleBegin ) * mSampleDur;
    ret_val.mSampleEnd = mStateStartSample;

    return ret_val;
}

bool USBSignalFilter::HasMoreData()
{
    return mDP->DoMoreTransitionsExistInCurrentData() || mDM->DoMoreTransitionsExistInCurrentData();
}

bool USBSignalFilter::IsDataSignal( const USBSignalState& s )
{
    if( mExpectLowSpeed )
        return ( s.IsData( FULL_SPEED ) && s.GetNumBits( FULL_SPEED ) == 1 ) || ( s.IsData( LOW_SPEED ) && s.GetNumBits( LOW_SPEED ) == 1 );

    return s.IsData( mSpeed ) && s.GetNumBits( mSpeed ) == 1;
}

bool USBSignalFilter::GetPacket( USBPacket& pckt, USBSignalState& sgnl )
{
    pckt.Clear();

    const U8 bits2add[] = { 0, 1, 1, 1, 1, 1, 1 };

    // a PRE packet switched us into low speed mode, so now we need to check
    // if this packet is low or full speed
    if( mExpectLowSpeed )
    {
        if( sgnl.IsData( FULL_SPEED ) && sgnl.GetNumBits( FULL_SPEED ) == 1 )
        {
            mSpeed = FULL_SPEED;
            mExpectLowSpeed = false; // switch back to full speed
        }
        else
        {
            mSpeed = LOW_SPEED;
        }
    }

    const double BIT_DUR = ( mSpeed == FULL_SPEED ? FS_BIT_DUR : LS_BIT_DUR );
    const double SAMPLE_DUR = 1e9 / mAnalyzer->GetSampleRate(); // 1 sample duration in ns
    const double BIT_SAMPLES = BIT_DUR / SAMPLE_DUR;

    std::vector<U8> bits;

    pckt.mSampleBegin = sgnl.mSampleBegin; // default for bad packets

    bool is_stuff_bit = false;
    bool is_pre_packet = false;
    while( sgnl.IsData( mSpeed ) && !is_pre_packet )
    {
        // get the number of bits in this signal
        int num_bits = sgnl.GetNumBits( mSpeed );

        const U8* add_begin = bits2add;
        const U8* add_end = bits2add + num_bits;

        // mind the stuffing bit
        if( is_stuff_bit )
            ++add_begin;

        // now add the data bits
        if( ( is_stuff_bit && num_bits > 1 ) || ( num_bits > 0 ) )
            bits.insert( bits.end(), add_begin, add_end );

        // do the bit markers and remember the samples at which a bit begins
        int bc;
        for( bc = 0; bc < num_bits; ++bc )
        {
            if( !is_stuff_bit || bc > 0 )
                pckt.mBitBeginSamples.push_back( sgnl.mSampleBegin + U64( BIT_SAMPLES * bc + .5 ) );

            mResults->AddMarker( sgnl.mSampleBegin + U64( BIT_SAMPLES * ( bc + .5 ) + .5 ),
                                 bc == 0 ? ( is_stuff_bit ? AnalyzerResults::ErrorX : AnalyzerResults::Zero ) : AnalyzerResults::One,
                                 mSettings->mDPChannel );
        }

        // check if this is a PRE token, in which case we switch to low-speed,
        // return and wait for the next packet
        if( mSpeed == FULL_SPEED && bits.size() == 16 ) // 8 bits for SYNC and 8 bits for PRE PID
        {
            const U8 preArr[ 16 ] = { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0 }; // SYNC and PRE as they appear on the bus
            if( std::equal( bits.begin(), bits.end(), preArr ) )
            {
                mSpeed = LOW_SPEED;
                mExpectLowSpeed = true;
                is_pre_packet = true;
            }
        }

        // set the stuff bit flag for the next state
        is_stuff_bit = ( num_bits == 7 );

        // get the next signal state
        sgnl = GetState();
    }

    pckt.mSampleEnd = sgnl.mSampleEnd; // default for bad packets

    // add another element to mBitBeginSamples to mark the end of the last bit
    pckt.mBitBeginSamples.push_back( sgnl.mSampleBegin );

    // check the number of bits
    if( bits.empty() || ( bits.size() % 8 ) != 0 || ( pckt.mBitBeginSamples.size() % 8 ) != 1 )
    {
        return false;
    }

    // remember the begin & end samples for the entire packet
    pckt.mSampleBegin = pckt.mBitBeginSamples.front();
    pckt.mSampleEnd = sgnl.mSampleEnd + S64( BIT_DUR / SAMPLE_DUR + 0.5 );

    // make bytes out of these bits
    U8 val = 0;
    std::vector<U8>::iterator i( bits.begin() );
    while( i != bits.end() )
    {
        // least significant bit first
        val >>= 1;
        if( *i )
            val |= 0x80;

        ++i;

        // do we have a full byte?
        if( ( ( i - bits.begin() ) % 8 ) == 0 )
            pckt.mData.push_back( val );
    }

    // extract the PID
    if( pckt.mData.size() > 1 )
        pckt.mPID = USB_PID( pckt.mData[ 1 ] );

    // extract the CRC fields
    if( pckt.mData.size() >= 4 )
    {
        U16 last_word = ( pckt.mData.back() << 8 ) | *( pckt.mData.end() - 2 );

        if( pckt.mData.size() == 4 )
            pckt.mCRC = last_word >> 11;
        else
            pckt.mCRC = last_word;
    }

    return ( pckt.IsTokenPacket() && pckt.mData.size() == 4 ) || ( pckt.IsDataPacket() && pckt.mData.size() >= 4 ) ||
           ( pckt.IsHandshakePacket() && pckt.mData.size() == 2 ) || ( pckt.mPID == PID_PRE && pckt.mData.size() == 2 ) ||
           ( pckt.IsSOFPacket() && pckt.mData.size() == 4 );
}

extern DisplayBase last_display_base; // hack

std::string int2str_sal( const U64 i, DisplayBase base, const int max_bits )
{
    last_display_base = base;
    char number_str[ 256 ];
    AnalyzerHelpers::GetNumberString( i, base, max_bits, number_str, sizeof( number_str ) );
    return number_str;
}
