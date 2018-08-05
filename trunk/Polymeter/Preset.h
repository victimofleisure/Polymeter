// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		14jun18	initial version
		
*/

#pragma once

#include "ArrayEx.h"
#include "Track.h"

class CPreset : public CTrackBase
{
// Construction
public:

// Attributes
public:

// Operations
public:
	void	Dump() const;

// Public data
public:
	CString	m_sName;			// preset name
	CMuteArray	m_arrMute;		// array of boolean track mute flags

protected:
// Types

// Constants

// Member data

// Helpers
};

class CPresetArray : public CArrayEx<CPreset, CPreset&> {
public:
// Operations
	static	void	PackBools(const CBoolArrayEx& arrSrc, CByteArrayEx& arrDst);
	static	void	UnpackBools(const CByteArrayEx& arrSrc, CBoolArrayEx& arrDst);
	static	int		CalcPackedSize(UINT nBits);
	void	Read(int nTracks);
	void	Write() const;
	void	OnTrackArrayEdit(const CIntArrayEx& arrTrackMap, int nNewTracks);
	void	Dump() const;
	int		Find(const CBoolArrayEx& arrMute) const;
};
