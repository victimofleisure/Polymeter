// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      15apr18	initial version
		01		10feb19	in channel array, add method to get MIDI event array
		02		21jan22	add property for note overlap method
		03		27jan22	fix Write's default value test to handle note overlap
		04		19feb22	use INI file class directly instead of via profile

*/

#include "stdafx.h"
#include "Channel.h"
#include "IniFile.h"

#define RK_CHANNEL_COUNT _T("Channels")
#define RK_CHANNEL_INDEX _T("Channel")
#define RK_CHANNEL_SECTION _T("Channel%d")

const CChannel CChannel::m_chanDefault(true);	// set default values

void CChannel::SetDefaults()
{
	#define CHANNELDEF(name, align, width) m_n##name = -1;
	#include "ChannelDef.h"	// generate member initialization
	m_nOverlaps = 0;
}

bool CChannel::operator==(const CChannel& chan) const
{
	#define CHANNELDEF(name, align, width) if (chan.m_n##name != m_n##name) return false;
	#include "ChannelDef.h"	// generate member initialization
	return true;
}

int CChannel::GetProperty(int iProp) const
{
	switch (iProp) {
	#define CHANNELDEF(name, align, width) case PROP_##name: return m_n##name;
	#include "ChannelDef.h"	// generate code to get properties
	default:
		NODEFAULTCASE;
	}
	return 0;
}

void CChannel::SetProperty(int iProp, int nVal)
{
	switch (iProp) {
	#define CHANNELDEF(name, align, width) case PROP_##name: m_n##name = nVal; break;
	#include "ChannelDef.h"	// generate code to set properties
	default:
		NODEFAULTCASE;
	}
}

int CChannelArray::GetUsedCount() const
{
	int	nChans = 0;
	for (int iChan = 0; iChan < MIDI_CHANNELS; iChan++) {	// for each channel
		if (!(*this)[iChan].IsDefault())
			nChans++;
	}
	return nChans;
}

DWORD CChannelArray::GetMidiEvent(int iChan, int iProp) const
{
	const CChannel&	chan = (*this)[iChan];
	switch (iProp) {
	case CChannel::PROP_BankMSB:
		if (chan.m_nBankMSB >= 0)
			return MakeMidiMsg(CONTROL, iChan, BANK_SELECT, chan.m_nBankMSB);
		break;
	case CChannel::PROP_BankLSB:
		if (chan.m_nBankLSB >= 0)
			return MakeMidiMsg(CONTROL, iChan, BANK_SELECT_LSB, chan.m_nBankLSB);
		break;
	case CChannel::PROP_Patch:
		if (chan.m_nPatch >= 0)
			return MakeMidiMsg(PATCH, iChan, chan.m_nPatch, 0);
		break;
	case CChannel::PROP_Volume:
		if (chan.m_nVolume >= 0)
			return MakeMidiMsg(CONTROL, iChan, VOLUME, chan.m_nVolume);
		break;
	case CChannel::PROP_Pan:
		if (chan.m_nPan >= 0)
			return MakeMidiMsg(CONTROL, iChan, PAN, chan.m_nPan);
		break;
	case CChannel::PROP_Overlaps:
		return 0;	// property doesn't map to a MIDI event
	default:
		NODEFAULTCASE;
	}
	return 0;
}

USHORT CChannelArray::GetMidiEvents(CDWordArrayEx& arrMidiEvent) const
{
	ASSERT(arrMidiEvent.IsEmpty());
	USHORT	nNoteOverlapMethodMask = 0;
	for (int iChan = 0; iChan < MIDI_CHANNELS; iChan++) {	// for each channel
		for (int iProp = 0; iProp < CChannel::EVENT_PROPERTIES; iProp++) {	// for each event-based channel property
			DWORD	dwEvent = GetMidiEvent(iChan, iProp);	// get property's MIDI event
			if (dwEvent)	// if property is specified
				arrMidiEvent.Add(dwEvent);	// add MIDI event to destination array
		}
		if ((*this)[iChan].m_nOverlaps)	// if this channel wants note overlaps merged
			nNoteOverlapMethodMask |= static_cast<USHORT>(1 << iChan);
	}
	return nNoteOverlapMethodMask;
}

void CChannelArray::Read(CIniFile& fIni)
{
	int	nUsedChans = 0;
	fIni.Get(RK_CHANNEL_COUNT, nUsedChans);
	for (int iUsedChan = 0; iUsedChan < nUsedChans; iUsedChan++) {	// for each used channel
		CString	sChan;
		sChan.Format(RK_CHANNEL_SECTION, iUsedChan);
		int	iChan = 0;
		fIni.Get(sChan, RK_CHANNEL_INDEX, iChan);
		if (iChan >= 0 && iChan < MIDI_CHANNELS) {	// range check channel index just in case
			CChannel&	chan = (*this)[iChan];
			#define CHANNELDEF(name, align, width) fIni.Get(sChan, _T(#name), chan.m_n##name);
			#include "ChannelDef.h"	// generate code to read channel properties
		}
	}
}

void CChannelArray::Write(CIniFile& fIni) const
{
	int	nUsedChans = GetUsedCount();
	fIni.Put(RK_CHANNEL_COUNT, nUsedChans);
	int	iUsedChan = 0;
	for (int iChan = 0; iChan < MIDI_CHANNELS; iChan++) {	// for each channel
		const CChannel&	chan = (*this)[iChan];
		if (!chan.IsDefault()) {	// if channel is used
			CString	sChan;
			sChan.Format(RK_CHANNEL_SECTION, iUsedChan);
			fIni.Put(sChan, RK_CHANNEL_INDEX, iChan);
			#define CHANNELDEF(name, align, width) if (chan.m_n##name >= 0) fIni.Put(sChan, _T(#name), chan.m_n##name);
			#define CHANNELDEF_EXCLUDE_NON_EVENTS	// only event properties
			#include "ChannelDef.h"	// generate code to write channel event properties
			#define CHANNELDEF(name, align, width) if (chan.m_n##name != CChannel::m_chanDefault.m_n##name) \
				fIni.Put(sChan, _T(#name), chan.m_n##name);
			#define CHANNELDEF_EXCLUDE_EVENTS	// only non-event properties
			#include "ChannelDef.h"	// generate code to write channel non-event properties
			iUsedChan++;
		}
	}
}
