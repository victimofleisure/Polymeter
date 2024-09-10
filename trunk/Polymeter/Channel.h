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
		03		19feb22	use INI file class directly instead of via profile
		04		01sep24	add property for duplicate notes method

*/

#pragma once

#include "FixedArray.h"
#include "Midi.h"

class CIniFile;

class CChannel {
public:
// Construction
	CChannel();
	CChannel(bool bInit);
	void	SetDefaults();

// Constants
	enum {	// channel properties
		#define CHANNELDEF(name, align, width) PROP_##name,
		#include "ChannelDef.h"	// generate enumeration
		PROPERTIES,
		EVENT_PROPERTIES = PROPERTIES - 1	// excludes note overlap method
	};

// Attributes
	bool	IsDefault() const;
	int		GetProperty(int iProp) const;
	void	SetProperty(int iProp, int nVal);

// Operations
	bool	operator==(const CChannel& chan) const;
	bool	operator!=(const CChannel& chan) const;

// Public data
	#define CHANNELDEF(name, align, width) int m_n##name;
	#include "ChannelDef.h"	// define member vars

	static const CChannel	m_chanDefault;	// channel default values
};

inline CChannel::CChannel()
{
}

inline CChannel::CChannel(bool bInit)
{
	UNREFERENCED_PARAMETER(bInit);
	SetDefaults();
}

inline bool CChannel::operator!=(const CChannel& chan) const
{
	return !operator==(chan);
}

inline bool CChannel::IsDefault() const
{
	return *this == m_chanDefault;
}

class CChannelArray : public CFixedArray<CChannel, MIDI_CHANNELS> {
public:
// Attributes
	int		GetUsedCount() const;
	DWORD	GetMidiEvent(int iChan, int iProp) const;
	USHORT	GetMidiEvents(CDWordArrayEx& arrMidiEvent, USHORT& nDuplicateNoteMethodMask) const;

// Operations
	void	Read(CIniFile& fIni);
	void	Write(CIniFile& fIni) const;
};
