#ifndef USB_ANALYZER_SETTINGS_H
#define USB_ANALYZER_SETTINGS_H

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

#include "USBTypes.h"

class USBAnalyzerSettings : public AnalyzerSettings
{
public:
	USBAnalyzerSettings();
	virtual ~USBAnalyzerSettings();

	virtual bool SetSettingsFromInterfaces();
	virtual void LoadSettings(const char* settings);
	virtual const char* SaveSettings();

	void UpdateInterfacesFromSettings();

	Channel			mDPChannel;
	Channel			mDMChannel;

	USBSpeed		mSpeed;
	USBDecodeLevel	mDecodeLevel;

protected:
	AnalyzerSettingInterfaceChannel		mDPChannelInterface;
	AnalyzerSettingInterfaceChannel		mDMChannelInterface;

	AnalyzerSettingInterfaceNumberList	mSpeedInterface;
	AnalyzerSettingInterfaceNumberList	mDecodeLevelInterface;
};

#endif	// USB_ANALYZER_SETTINGS_H
