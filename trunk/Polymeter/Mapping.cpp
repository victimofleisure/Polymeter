// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      20mar20	initial version

*/

#include "stdafx.h"
#include "Polymeter.h"
#include "Mapping.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "Persist.h"

#define RK_MAPPING_COUNT _T("Count")	// registry keys
#define RK_MAPPING_SECTION _T("Mapping")
#define RK_MAPPING_IN_EVENT _T("InEvent")
#define RK_MAPPING_OUT_EVENT _T("OutEvent")

void CMapping::SetDefaults()
{
	m_nInEvent = MIDI_CVM_CONTROL;
	m_nInChannel = 0;
	m_nInControl = 1;
	m_nOutEvent = MIDI_CVM_CONTROL;
	m_nOutChannel = 0;
	m_nOutControl = 1;
	m_nRangeStart = 0;
	m_nRangeEnd = 127;
	m_nTrack = -1;
}

int CMapping::GetProperty(int iProp) const
{
	ASSERT(iProp >= 0 && iProp < PROPERTIES);
	switch (iProp) {
	#define MAPPINGDEF(name, align, width, member, minval, maxval) case PROP_##name: return m_n##member;
	#include "MappingDef.h"	// generate cases for each member var
	}
	return 0;	// error
}

void CMapping::SetProperty(int iProp, int nVal)
{
	ASSERT(iProp >= 0 && iProp < PROPERTIES);
	enum {
		#define MAPPINGDEF(name, align, width, member, minval, maxval) PROP_##name,
		#include "MappingDef.h"	// generate member enumeration
	};
	switch (iProp) {
	#define MAPPINGDEF(name, align, width, member, minval, maxval) case PROP_##name: m_n##member = nVal; break;
	#include "MappingDef.h"	// generate cases for each member
	}
}

void CMapping::Read(LPCTSTR pszSection)
{
	CString	sName;
	sName = CPersist::GetString(pszSection, RK_MAPPING_IN_EVENT);
	m_nInEvent = FindInputEventName(sName);
	sName = CPersist::GetString(pszSection, RK_MAPPING_OUT_EVENT);
	m_nOutEvent = FindOutputEventName(sName);
	// conditional to exclude events is optimized away in release build
	#define MAPPINGDEF(name, align, width, member, minval, maxval) \
		if (PROP_##name != PROP_IN_EVENT && PROP_##name != PROP_OUT_EVENT) \
			m_n##member = CPersist::GetInt(pszSection, _T(#member), 0);
	#include "MappingDef.h"	// generate profile read for each member, excluding events
}

void CMapping::Write(LPCTSTR pszSection) const
{
	CPersist::WriteString(pszSection, RK_MAPPING_IN_EVENT, GetInputEventName(m_nInEvent));
	CPersist::WriteString(pszSection, RK_MAPPING_OUT_EVENT, GetOutputEventName(m_nOutEvent));
	// conditional to exclude events is optimized away in release build
	#define MAPPINGDEF(name, align, width, member, minval, maxval) \
		if (PROP_##name != PROP_IN_EVENT && PROP_##name != PROP_OUT_EVENT) \
			CPersist::WriteInt(pszSection, _T(#member), m_n##member);
	#include "MappingDef.h"	// generate profile write for each member, excluding events
}

void CMappingArray::Read()
{
	CString	sSectionIdx;
	int	nItems = CPersist::GetInt(RK_MAPPING_SECTION, _T("Count"), 0);
	SetSize(nItems);
	for (int iItem = 0; iItem < nItems; iItem++) {	// for each mapping
		sSectionIdx.Format(_T("%d"), iItem);
		GetAt(iItem).Read(RK_MAPPING_SECTION _T("\\") + sSectionIdx);
	}
}

DWORD CMapping::GetInputMidiMsg() const
{
	return MakeMidiMsg((m_nInEvent + 8) << 4, m_nInChannel, m_nInControl, 0);
}

bool CMapping::SetInputMidiMsg(DWORD nMsg)
{
	int	nEvent = MIDI_CMD_IDX(nMsg);
	if (nEvent >= 0 && nEvent < MIDI_CHANNEL_VOICE_MESSAGES) {
		int	nChannel = MIDI_CHAN(nMsg);
		int	nControl;
		if (nEvent <= MIDI_CVM_CONTROL)	// if message has control parameter
			nControl = MIDI_P1(nMsg);
		else	// no control parameter
			nControl = 0;
		m_nInEvent = nEvent;
		m_nInChannel = nChannel;
		m_nInControl = nControl;
		return true;
	}
	return false;
}

void CMappingArray::Write() const
{
	CString	sSectionIdx;
	int	nItems = GetSize();
	CPersist::WriteInt(RK_MAPPING_SECTION, _T("Count"), nItems);
	for (int iItem = 0; iItem < nItems; iItem++) {	// for each mapping
		sSectionIdx.Format(_T("%d"), iItem);
		GetAt(iItem).Write(RK_MAPPING_SECTION _T("\\") + sSectionIdx);
	}
}

bool CMappingArray::MapMidiEvent(DWORD dwInEvent, CDWordArrayEx& arrOutEvent) const
{
	// This method is called from worker threads, so access data carefully.
	// Lock the mapping array critical section before calling this method.
	// The track property case locks the track critical section as needed.
	arrOutEvent.FastRemoveAll();
	bool	bIsMapped = false;
	int	iInCmd = MIDI_CMD_IDX(dwInEvent);	// MIDI command index
	int	nInChan = MIDI_CHAN(dwInEvent);	// MIDI channel 
	// The first four MIDI channel voice messages have a control parameter in P1, but the others don't.
	int	nInControl;
	if (iInCmd <= MIDI_CVM_CONTROL)	// if input message has a control parameter
		nInControl = MIDI_P1(dwInEvent);	// MIDI control parameter
	else	// input message lacks a control parameter
		nInControl = 0;
	int	nMappings = GetSize();
	for (int iMapping = 0; iMapping < nMappings; iMapping++) {	// for each mapping
		const CMapping&	map = GetAt(iMapping);
		// compare the attributes that are less likely to match first, for a slight time saving
		if (map.m_nInControl == nInControl && map.m_nInEvent == iInCmd && map.m_nInChannel == nInChan) {
			bIsMapped = true;
			int	nDataVal;
			if (iInCmd <= MIDI_CVM_CONTROL)	// if input message has a control parameter
				nDataVal = MIDI_P2(dwInEvent);	// get data value from input message P2
			else	// input message lacks a control parameter
				nDataVal = MIDI_P1(dwInEvent);	// get data value from input message P1
			int	nDeltaRange = map.m_nRangeEnd - map.m_nRangeStart;	// can be negative if start > end
			nDataVal = round(nDataVal / 127.0 * nDeltaRange) + map.m_nRangeStart;	// apply range
			int	iTrackProp = map.m_nOutEvent - MIDI_CHANNEL_VOICE_MESSAGES;
			if (iTrackProp < 0) {	// if output event is a channel voice message
				nDataVal = CLAMP(nDataVal, 0, MIDI_NOTE_MAX);	// clamp result to valid MIDI data range
				int	nP1, nP2;
				if (map.m_nOutEvent <= MIDI_CVM_CONTROL) { // if output message has a control parameter
					nP1 = map.m_nOutControl;	// control parameter goes in output message P1
					nP2 = nDataVal;	// data value goes in output message P2
				} else {	// output message lacks a control parameter
					nP1 = nDataVal;	// data value goes in output message P1
					nP2 = 0;
				}
				DWORD	nMsg = MakeMidiMsg((map.m_nOutEvent + 8) << 4, map.m_nOutChannel, nP1, nP2);
				arrOutEvent.Add(nMsg);	// add translated event to destination array
			} else {	// output event is a track property
				CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
				if (pDoc != NULL) {	// if active document
					switch (iTrackProp) {
					case CTrack::PROP_Name:
						break;	// not supported, fail quietly
					default:
						int	iTrack = map.m_nTrack;
						if (iTrack >= 0) {	// if track index specified
							int	nMinVal, nMaxVal;
							CTrackBase::GetPropertyRange(iTrackProp, nMinVal, nMaxVal);
							if (nMinVal != nMaxVal)	// if property specifies a range
								nDataVal = CLAMP(nDataVal, nMinVal, nMaxVal);	// apply property range
							{	// lock track critical section
								WCritSec::Lock	lock(pDoc->m_Seq.GetCritSec());	// serialize access to track array
								if (iTrack < pDoc->GetTrackCount()) {	// if track index within valid range
									CComVariant	var(nDataVal);
									pDoc->m_Seq.SetTrackProperty(iTrack, iTrackProp, var);	// update track property
								}
							}	// unlock track critical section
							// recipient should check track index to make sure it's still within valid range
							theApp.GetMainFrame()->PostMessage(UWM_TRACK_PROPERTY,	// post message to update UI
								iTrack, MAKELONG(nDataVal, iTrackProp));
						}
					}
				}
			}
		}
	}
	return bIsMapped;
}

void CSeqMapping::GetProperty(const CIntArrayEx& arrSelection, int iProp, CIntArrayEx& arrProp) const
{
	int	nSels = arrSelection.GetSize();
	arrProp.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected mapping
		int	iMapping = arrSelection[iSel];
		arrProp[iSel] = m_arrMapping[iMapping].GetProperty(iProp);
	}
}

void CSeqMapping::SetProperty(const CIntArrayEx& arrSelection, int iProp, const CIntArrayEx& arrProp)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected mapping
		int	iMapping = arrSelection[iSel];
		m_arrMapping[iMapping].SetProperty(iProp, arrProp[iSel]);
	}
}

void CSeqMapping::SetProperty(const CIntArrayEx& arrSelection, int iProp, int nVal)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected mapping
		int	iMapping = arrSelection[iSel];
		m_arrMapping[iMapping].SetProperty(iProp, nVal);
	}
}

void CSeqMapping::SetArray(const CMappingArray& arrMapping)
{
	WCritSec::Lock	lock(m_csMapping);	// serialize access to mappings
	m_arrMapping = arrMapping;
}

void CSeqMapping::Attach(CMappingArray& arrMapping)
{
	WCritSec::Lock	lock(m_csMapping);	// serialize access to mappings
	m_arrMapping.Swap(arrMapping);
}

void CSeqMapping::Insert(int iInsert, CMappingArray& arrMapping)
{
	WCritSec::Lock	lock(m_csMapping);	// serialize access to mappings
	m_arrMapping.InsertAt(iInsert, &arrMapping);
}

void CSeqMapping::Insert(const CIntArrayEx& arrSelection, CMappingArray& arrMapping)
{
	WCritSec::Lock	lock(m_csMapping);	// serialize access to mappings
	m_arrMapping.InsertSelection(arrSelection, arrMapping);
}

void CSeqMapping::Delete(int iMapping, int nCount)
{
	WCritSec::Lock	lock(m_csMapping);	// serialize access to mappings
	m_arrMapping.RemoveAt(iMapping, nCount);
}

void CSeqMapping::Delete(const CIntArrayEx& arrSelection)
{
	WCritSec::Lock	lock(m_csMapping);	// serialize access to mappings
	m_arrMapping.DeleteSelection(arrSelection);
}

void CSeqMapping::Move(const CIntArrayEx& arrSelection, int iDropPos)
{
	WCritSec::Lock	lock(m_csMapping);	// serialize access to mappings
	m_arrMapping.MoveSelection(arrSelection, iDropPos);
}

void CSeqMapping::Sort(int iProp)
{
	WCritSec::Lock	lock(m_csMapping);	// serialize access to mappings
	m_iSortProp = iProp;
	qsort(m_arrMapping.GetData(), m_arrMapping.GetSize(), sizeof(CMapping), SortCompare);
}

int CSeqMapping::m_iSortProp;	// index of property to sort mappings by

int CSeqMapping::SortCompare(const void *arg1, const void *arg2)
{
	const CMapping*	pMapping1 = (CMapping *)arg1;
	const CMapping*	pMapping2 = (CMapping *)arg2;
	return CTrack::Compare(pMapping1->GetProperty(m_iSortProp), pMapping2->GetProperty(m_iSortProp));
}

void CSeqMapping::OnTrackArrayEdit(const CIntArrayEx& arrTrackMap)
{
	int	nMappings = m_arrMapping.GetSize();
	for (int iMapping = 0; iMapping < nMappings; iMapping++) {	// for each mapping
		CMapping&	map = m_arrMapping[iMapping];
		if (map.m_nTrack >= 0)	// if mapping has a valid track index
			map.m_nTrack = arrTrackMap[map.m_nTrack];	// repair track index
	}
}

void CSeqMapping::GetTrackIndices(CIntArrayEx& arrTrackIdx) const
{
	int	nMappings = GetCount();
	arrTrackIdx.SetSize(nMappings);
	for (int iMapping = 0; iMapping < nMappings; iMapping++) {	// for each mapping
		arrTrackIdx[iMapping] = m_arrMapping[iMapping].m_nTrack;
	}
}

void CSeqMapping::SetTrackIndices(const CIntArrayEx& arrTrackIdx)
{
	int	nMappings = GetCount();
	for (int iMapping = 0; iMapping < nMappings; iMapping++) {	// for each mapping
		m_arrMapping[iMapping].m_nTrack = arrTrackIdx[iMapping];
	}
}
