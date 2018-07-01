#ifndef USB_SIMULATION_DATA_GENERATOR_H
#define USB_SIMULATION_DATA_GENERATOR_H

#include <AnalyzerHelpers.h>

#include "USBTypes.h"

class USBAnalyzerSettings;

class USBSimulationDataGenerator
{
public:
	USBSimulationDataGenerator();
	~USBSimulationDataGenerator();

	void Initialize(U32 simulation_sample_rate, USBAnalyzerSettings* settings);
	U32 GenerateSimulationData(U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels);

protected:
	USBAnalyzerSettings*				mSettings;
	U32									mSimulationSampleRateHz;

	ClockGenerator						mClockGenerator;

	SimulationChannelDescriptorGroup	mUSBSimulationChannels;
	SimulationChannelDescriptor*		mDP;
	SimulationChannelDescriptor*		mDM;

	const U16*	mpCurrSimData;
	const U16*	mpSimDataLoopHere;
	U16			mFrameCnt;

	// used to keep track of the 1ms frames of the USB bus
	double		mAccuDur;

	void ResetFrameDuration()
	{
		mAccuDur = 0;
	}

	// general outputs
	void SetJ();		// sets the lines into J state
	void SetK();		// sets the lines into K state
	void OutSE0(const double dur);
	void OutJ(const double dur);
	void OutK(const double dur);
	void OutReset();
	void OutFillFrame();	// outputs idle until the end of the 1ms frame
	const U16* OutPacket(const U16* pPacket);

	// LS outputs
	void OutLSKeepAlive();

	// FS outputs
	void OutFSSOF();
};

#endif	// USB_SIMULATION_DATA_GENERATOR_H
