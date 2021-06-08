// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		14jun18	initial version
		01		08jun21	fix warning for CString as variadic argument
		
*/

#include "stdafx.h"
#include "Preset.h"
#include "RegTempl.h"

#define RK_PRESET_SECTION _T("Preset")
#define RK_PRESET_COUNT _T("Count")
#define RK_PRESET_NAME _T("Name")
#define RK_PRESET_MUTE _T("Mute")

void CPreset::Dump() const
{
	int	nMutes = m_arrMute.GetSize();
	_tprintf(_T("'%s' %d:"), m_sName.GetString(), nMutes);
	for (int iMute = 0; iMute < nMutes; iMute++)
		printf(" %d", m_arrMute[iMute]);
	printf("\n");
}

int CPresetArray::CalcPackedSize(UINT nBits)
{
	return ((nBits - 1) >> 3) + 1;
}

void CPresetArray::PackBools(const CBoolArrayEx& arrSrc, CByteArrayEx& arrDst)
{
	UINT	nBits = arrSrc.GetSize();
	UINT	nDstSize = ((nBits - 1) >> 3) + 1;
	arrDst.RemoveAll();
	arrDst.SetSize(nDstSize);
	for (UINT iBit = 0; iBit < nBits; iBit++)
		arrDst[iBit >> 3] |= arrSrc[iBit] << (iBit & 7);
}

void CPresetArray::UnpackBools(const CByteArrayEx& arrSrc, CBoolArrayEx& arrDst)
{
	UINT nBits = arrDst.GetSize();
	arrDst.SetSize(nBits);
	for (UINT iBit = 0; iBit < nBits; iBit++)
		arrDst[iBit] = (arrSrc[iBit >> 3] & (1 << (iBit & 7))) != 0;
}

void CPresetArray::Read(int nTracks)
{
	int	nPresets = 0;
	RdReg(RK_PRESET_SECTION, RK_PRESET_COUNT, nPresets);
	CByteArrayEx	arrBit;
	SetSize(nPresets);
	CString	sKey;
	for (int iPreset = 0; iPreset < nPresets; iPreset++) {
		CPreset& preset = GetAt(iPreset);
		sKey.Format(_T("%s\\%d"), RK_PRESET_SECTION, iPreset);
		RdReg(sKey, RK_PRESET_NAME, preset.m_sName);
		DWORD	nSize = CalcPackedSize(nTracks);
		arrBit.SetSize(nSize);
		CPersist::GetBinary(sKey, RK_PRESET_MUTE, arrBit.GetData(), &nSize);
		preset.m_arrMute.SetSize(nTracks);
		UnpackBools(arrBit, preset.m_arrMute);
	}
}

void CPresetArray::Write() const
{
	int	nPresets = GetSize();
	WrReg(RK_PRESET_SECTION, RK_PRESET_COUNT, nPresets);
	CByteArrayEx	arrBit;
	CString	sKey;
	for (int iPreset = 0; iPreset < nPresets; iPreset++) {
		const CPreset& preset = GetAt(iPreset);
		sKey.Format(_T("%s\\%d"), RK_PRESET_SECTION, iPreset);
		WrReg(sKey, RK_PRESET_NAME, preset.m_sName);
		PackBools(preset.m_arrMute, arrBit);
		CPersist::WriteBinary(sKey, RK_PRESET_MUTE, arrBit.GetData(), arrBit.GetSize());
	}
}

void CPresetArray::OnTrackArrayEdit(const CIntArrayEx& arrTrackMap, int nNewTracks)
{
	CBoolArrayEx	arrNewMute;
	int	nPresets = GetSize();
	for (int iPreset = 0; iPreset < nPresets; iPreset++) {	// for each of our presets
		CPreset&	preset = GetAt(iPreset);
		arrNewMute.SetSize(nNewTracks);
		memset(arrNewMute.GetData(), true, nNewTracks);	// init preset's mutes to true
		// mapping table has one entry for each pre-edit track, possibly followed by entries for pasted tracks
		int	nOldTracks = preset.m_arrMute.GetSize();	// old mute array has one element for each pre-edit track
		for (int iOldTrack = 0; iOldTrack < nOldTracks; iOldTrack++) {	// for each pre-edit track
			int	iNewTrack = arrTrackMap[iOldTrack];	// get track's new index, if any
			if (iNewTrack >= 0)	// if track still exists post-edit
				arrNewMute[iNewTrack] = preset.m_arrMute[iOldTrack];	// move mute to new location
		}
		preset.m_arrMute = arrNewMute;	// store updated mutes
	}
}

void CPresetArray::Dump() const
{
	int	nPresets = GetSize();
	printf("nPresets=%d\n", nPresets);
	for (int iPreset = 0; iPreset < nPresets; iPreset++) {	// for each of our presets
		printf("%d: ", iPreset);
		GetAt(iPreset).Dump();
	}
}

int CPresetArray::Find(const CBoolArrayEx& arrMute) const
{
	int	nPresets = GetSize();
	for (int iPreset = 0; iPreset < nPresets; iPreset++) {
		if (GetAt(iPreset).m_arrMute == arrMute)
			return iPreset;
	}
	return -1;
}
