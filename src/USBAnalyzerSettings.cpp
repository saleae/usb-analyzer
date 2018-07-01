#include <AnalyzerHelpers.h>

#include "USBAnalyzerSettings.h"
#include "USBAnalyzerResults.h"
#include "USBTypes.h"

USBAnalyzerSettings::USBAnalyzerSettings()
:	mDMChannel(UNDEFINED_CHANNEL),
	mDPChannel(UNDEFINED_CHANNEL),
	mSpeed(LOW_SPEED),
	mDecodeLevel(OUT_CONTROL_TRANSFERS)
{
	// init the interface
	mDPChannelInterface.SetTitleAndTooltip("D+", "USB D+ (green)");
	mDPChannelInterface.SetChannel(mDPChannel);

	mDMChannelInterface.SetTitleAndTooltip("D-", "USB D- (white)");
	mDMChannelInterface.SetChannel(mDMChannel);

	mSpeedInterface.SetTitleAndTooltip("USB bit-rate", "USB data bit-rate");
	mSpeedInterface.AddNumber(LOW_SPEED, "Low speed (1.5 Mbps)", "Low speed (1.5 mbit/s)");
	mSpeedInterface.AddNumber(FULL_SPEED, "Full speed (12 Mbps)", "Full speed (12 mbit/s)");

	mSpeedInterface.SetNumber(mSpeed);

	mDecodeLevelInterface.SetTitleAndTooltip("USB decode level", "Type of decoded USB output");
	mDecodeLevelInterface.AddNumber(OUT_CONTROL_TRANSFERS, "Control transfers", "Decodes the standard USB enpoint 0 control transfers.");
	mDecodeLevelInterface.AddNumber(OUT_PACKETS, "Packets", "Decode all the fields of USB packets");
	mDecodeLevelInterface.AddNumber(OUT_BYTES, "Bytes", "Decode the data as raw bytes");
	mDecodeLevelInterface.AddNumber(OUT_SIGNALS, "Signals", "Decode the USB signal states: J, K and SE0");

	mDecodeLevelInterface.SetNumber(OUT_CONTROL_TRANSFERS);

	// add the interface
	AddInterface(&mDPChannelInterface);
	AddInterface(&mDMChannelInterface);
	AddInterface(&mSpeedInterface);
	AddInterface(&mDecodeLevelInterface);

	// describe export
	AddExportOption(0, "Export as text file");
	AddExportExtension(0, "text", "txt");

	ClearChannels();

	AddChannel(mDPChannel, "D+", false);
	AddChannel(mDMChannel, "D-", false);
}

USBAnalyzerSettings::~USBAnalyzerSettings()
{}

bool USBAnalyzerSettings::SetSettingsFromInterfaces()
{
	if (mDPChannelInterface.GetChannel() == UNDEFINED_CHANNEL)
	{
		SetErrorText("Please select an input for the D+ channel.");
		return false;
	}

	if (mDMChannelInterface.GetChannel() == UNDEFINED_CHANNEL)
	{
		SetErrorText("Please select an input for the D- channel.");
		return false;
	}

	mDPChannel = mDPChannelInterface.GetChannel();
	mDMChannel = mDMChannelInterface.GetChannel();
	mSpeed = USBSpeed(int(mSpeedInterface.GetNumber()));
	mDecodeLevel = USBDecodeLevel(int(mDecodeLevelInterface.GetNumber()));

	if (mDMChannel == mDPChannel)
	{
		SetErrorText("Please select different inputs for the D- and D+ channels.");
		return false;
	}

	ClearChannels();

	AddChannel(mDPChannel, "D+", true);
	AddChannel(mDMChannel, "D-", true);

	return true;
}

void USBAnalyzerSettings::UpdateInterfacesFromSettings()
{
	mDPChannelInterface.SetChannel(mDPChannel);
	mDMChannelInterface.SetChannel(mDMChannel);
	mSpeedInterface.SetNumber(mSpeed);
	mDecodeLevelInterface.SetNumber(mDecodeLevel);
}

void USBAnalyzerSettings::LoadSettings(const char* settings)
{
	SimpleArchive text_archive;
	text_archive.SetString(settings);

	text_archive >> mDPChannel;
	text_archive >> mDMChannel;
	int s;
	text_archive >> s;
	mSpeed = USBSpeed(s);

	text_archive >> s;
	mDecodeLevel = USBDecodeLevel(s);

	ClearChannels();

	AddChannel(mDPChannel, "D+", true);
	AddChannel(mDMChannel, "D-", true);

	UpdateInterfacesFromSettings();
}

const char* USBAnalyzerSettings::SaveSettings()
{
	SimpleArchive text_archive;

	text_archive << mDPChannel;
	text_archive << mDMChannel;
	text_archive << mSpeed;
	text_archive << mDecodeLevel;

	return SetReturnString(text_archive.GetString());
}
