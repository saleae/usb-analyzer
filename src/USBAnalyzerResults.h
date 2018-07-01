#ifndef USB_ANALYZER_RESULTS_H
#define USB_ANALYZER_RESULTS_H

#include <AnalyzerResults.h>

#include "USBTypes.h"

class USBAnalyzer;
class USBAnalyzerSettings;

class USBAnalyzerResults: public AnalyzerResults
{
public:
	USBAnalyzerResults(USBAnalyzer* analyzer, USBAnalyzerSettings* settings);
	virtual ~USBAnalyzerResults();

	virtual void GenerateBubbleText(U64 frame_index, Channel& channel, DisplayBase display_base);
	virtual void GenerateExportFile(const char* file, DisplayBase display_base, U32 export_type_user_id);

	void GenerateExportFileControlTransfers(const char* file, DisplayBase display_base);
	void GenerateExportFilePackets(const char* file, DisplayBase display_base);
	void GenerateExportFileBytes(const char* file, DisplayBase display_base);
	void GenerateExportFileSignals(const char* file, DisplayBase display_base);

	virtual void GenerateFrameTabularText(U64 frame_index, DisplayBase display_base);
	virtual void GeneratePacketTabularText(U64 packet_id, DisplayBase display_base);
	virtual void GenerateTransactionTabularText(U64 transaction_id, DisplayBase display_base);

	void AddStringDescriptor(int addr, int id, const std::string& stringdesc)
	{
		mAllStringDescriptors[std::make_pair(addr, id)] = stringdesc;
	}

	double GetSampleTime(S64 sample) const;
	std::string GetSampleTimeStr(S64 sample) const;

	typedef std::map<std::pair<U8, U8>, std::string>	USBStringContainer;

protected:	// functions

protected:	// vars

	USBAnalyzerSettings*	mSettings;
	public:
	USBAnalyzer*			mAnalyzer;

	USBStringContainer		mAllStringDescriptors;
};

#endif	// USB_ANALYZER_RESULTS_H
