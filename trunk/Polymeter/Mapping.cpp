// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      20mar20	initial version
		01		27mar20	match control parameter only if event uses it
		02		29mar20	add get/set input message for selected mappings
		03		05apr20	add track step mapping
		04		07sep20	add preset and part mapping
		05		15feb21	add mapping targets for transport commands
		06		07jun21	rename rounding functions
		07		30jun21	move step mapping range checks into critical section
		08		25oct21	add optional sort direction
		09		21jan22	add tempo mapping target

*/

#include "stdafx.h"
#include "Polymeter.h"
#include "Mapping.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "Persist.h"
#include <math.h>

#define RK_MAPPING_COUNT _T("Count")	// registry keys
#define RK_MAPPING_SECTION _T("Mapping")
#define RK_MAPPING_IN_EVENT _T("InEvent")
#define RK_MAPPING_OUT_EVENT _T("OutEvent")

const LPCTSTR CMapping::m_arrSpecialTarget[SPECIAL_TARGETS] = {
	#define MAPPINGDEF_SPECIAL_TARGET(name, strid) _T(#name),
	#include "MappingDef.h"	// generate names of special output events
};

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
	switch (iProp) {
	#define MAPPINGDEF(name, align, width, member, minval, maxval) case PROP_##name: m_n##member = nVal; break;
	#include "MappingDef.h"	// generate cases for each member
	}
}

LPCTSTR CMapping::GetOutputEventName(int nOutEvent)
{
	ASSERT(nOutEvent >= 0 && nOutEvent < OUTPUT_EVENTS);
	if (nOutEvent < MIDI_CHANNEL_VOICE_MESSAGES)
		return CTrackBase::GetMidiChannelVoiceMsgName(nOutEvent);
	nOutEvent -= MIDI_CHANNEL_VOICE_MESSAGES;
	if (nOutEvent < CTrackBase::PROPERTIES)
		return CTrackBase::GetPropertyInternalName(nOutEvent);
	nOutEvent -= CTrackBase::PROPERTIES;
	return m_arrSpecialTarget[nOutEvent];
}

int CMapping::FindOutputEventName(LPCTSTR pszName)
{
	int	nEvent = CTrackBase::FindMidiChannelVoiceMsgName(pszName);	// search channel voice message names
	if (nEvent >= 0)	// if name is channel voice message
		return nEvent;
	nEvent = CTrackBase::FindPropertyInternalName(pszName);	// search track property names
	if (nEvent >= 0)	// if name is track property
		return nEvent + MIDI_CHANNEL_VOICE_MESSAGES;	// account for channel voice messages
	nEvent = ArrayFind(m_arrSpecialTarget, SPECIAL_TARGETS, pszName);
	if (nEvent >= 0)	// if name is special output event
		return nEvent + MIDI_CHANNEL_VOICE_MESSAGES + CTrackBase::PROPERTIES;	// account for track properties too
	return -1;	// unknown name
}

void CMapping::Read(LPCTSTR pszSection)
{
	CString	sName;
	sName = CPersist::GetString(pszSection, RK_MAPPING_IN_EVENT);
	m_nInEvent = FindInputEventName(sName);
	ASSERT(m_nInEvent >= 0);	// check for unknown input event name
	if (m_nInEvent < 0)	// if unknown input event name
		m_nInEvent = 0;		// avoid range errors downstream
	sName = CPersist::GetString(pszSection, RK_MAPPING_OUT_EVENT);
	m_nOutEvent = FindOutputEventName(sName);
	ASSERT(m_nOutEvent >= 0);	// check for unknown output event name
	if (m_nOutEvent < 0)	// if unknown output event name
		m_nOutEvent = 0;		// avoid range errors downstream
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

void CMapping::SetInputMidiMsg(DWORD nInMidiMsg)
{
	// channel voice messages only; caller is responsible for ensuring this
	int	iEvent = MIDI_CMD_IDX(nInMidiMsg);	// convert MIDI status to event index
	ASSERT(iEvent >= 0 && iEvent < INPUT_EVENTS);	// check event index range
	m_nInEvent = iEvent;
	m_nInChannel = MIDI_CHAN(nInMidiMsg);
	m_nInControl = MIDI_P1(nInMidiMsg);
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
	int	iInChan = MIDI_CHAN(dwInEvent);		// MIDI channel index
	int	iInControl = MIDI_P1(dwInEvent);	// MIDI control parameter
	// The first four MIDI channel voice messages have a control parameter in P1, but the others don't.
	int	nMappings = GetSize();
	for (int iMapping = 0; iMapping < nMappings; iMapping++) {	// for each mapping
		const CMapping&	map = GetAt(iMapping);
		// compare the attributes that are less likely to match first, for a slight time saving
		if ((map.m_nInControl == iInControl || map.m_nInEvent > MIDI_CVM_CONTROL)	// if control parameter matches or doesn't matter
		&& map.m_nInEvent == iInCmd && map.m_nInChannel == iInChan) {	// and command index and channel index also match
			bIsMapped = true;
			int	nDataVal;
			if (iInCmd <= MIDI_CVM_CONTROL)	// if input message has a control parameter
				nDataVal = MIDI_P2(dwInEvent);	// get data value from input message P2
			else	// input message lacks a control parameter
				nDataVal = MIDI_P1(dwInEvent);	// get data value from input message P1
			int	nDeltaRange = map.m_nRangeEnd - map.m_nRangeStart;	// can be negative if start > end
			double	fDataVal = nDataVal / 127.0 * nDeltaRange;	// apply scaling; save intermediate result
			nDataVal = Round(fDataVal) + map.m_nRangeStart;	// round and offset by start of range
			if (map.m_nOutEvent < MIDI_CHANNEL_VOICE_MESSAGES) {	// if output event is a channel voice message
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
			} else {	// output event isn't a channel voice property
				CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
				if (pDoc != NULL) {	// if active document
					switch (map.m_nOutEvent) {
					case CMapping::OUT_Step:
						{
							int	iTrack = map.m_nTrack;
							if (iTrack >= 0) {	// if track index specified
								nDataVal = CLAMP(nDataVal, 0, MIDI_NOTE_MAX);	// clamp data to step range
								{	// lock track critical section
									WCritSec::Lock	lock(pDoc->m_Seq.GetCritSec());	// serialize access to track array
									// check index ranges within critical section to avoid races
									if (iTrack < pDoc->GetTrackCount()	// if valid track index
									&& map.m_nOutControl < pDoc->m_Seq.GetLength(iTrack)) {	// and valid step index
										pDoc->m_Seq.SetStep(iTrack, map.m_nOutControl, 
											static_cast<CTrackBase::STEP>(nDataVal));
									}
								}	// unlock track critical section ASAP
								// recipient should check track and step indices to make sure they're valid
								theApp.GetMainFrame()->PostMessage(UWM_TRACK_STEP_CHANGE,	// post message to update UI
									iTrack, map.m_nOutControl);
							}
						}
						break;
					case CMapping::OUT_Preset:
						theApp.GetMainFrame()->PostMessage(UWM_PRESET_APPLY, nDataVal);
						break;
					case CMapping::OUT_Part:
						theApp.GetMainFrame()->PostMessage(UWM_PART_APPLY, map.m_nOutControl, nDataVal);
						break;
					case CMapping::OUT_Play:
						theApp.GetMainFrame()->PostMessage(UWM_MAPPED_COMMAND, ID_TRANSPORT_PLAY, nDataVal);
						break;
					case CMapping::OUT_Pause:
						theApp.GetMainFrame()->PostMessage(UWM_MAPPED_COMMAND, ID_TRANSPORT_PAUSE, nDataVal);
						break;
					case CMapping::OUT_Record:
						theApp.GetMainFrame()->PostMessage(UWM_MAPPED_COMMAND, ID_TRANSPORT_RECORD, nDataVal);
						break;
					case CMapping::OUT_Loop:
						theApp.GetMainFrame()->PostMessage(UWM_MAPPED_COMMAND, ID_TRANSPORT_LOOP, nDataVal);
						break;
					case CMapping::OUT_Tempo:
						{
							// use unrounded intermediate result (scaled data value) for increased precision
							double	fExponent = (fDataVal + map.m_nRangeStart) / 127.0;	// offset and normalize
							double	fScaledTempo = pDoc->m_fTempo * pow(2, fExponent);	// scale base tempo
							pDoc->m_Seq.SetTempo(fScaledTempo);
						}
						break;
					default:
						int	iTrack = map.m_nTrack;
						if (iTrack >= 0) {	// if track index specified
							int	iTrackProp = map.m_nOutEvent - MIDI_CHANNEL_VOICE_MESSAGES;	// track property index
							if (iTrackProp == CTrackBase::PROP_Name)	// if name property
								break;	// unsupported, fail silently
							int	nMinVal, nMaxVal;
							CTrackBase::GetPropertyRange(iTrackProp, nMinVal, nMaxVal);
							if (nMinVal != nMaxVal)	// if property specifies a range
								nDataVal = CLAMP(nDataVal, nMinVal, nMaxVal);	// clamp data to property range
							{	// lock track critical section
								WCritSec::Lock	lock(pDoc->m_Seq.GetCritSec());	// serialize access to track array
								if (iTrack < pDoc->GetTrackCount()) {	// if track index within valid range
									CComVariant	var(nDataVal);
									pDoc->m_Seq.SetTrackProperty(iTrack, iTrackProp, var);	// update track property
								}
							}	// unlock track critical section ASAP
							// recipient should check track index to make sure it's still within valid range
							theApp.GetMainFrame()->PostMessage(UWM_TRACK_PROPERTY_CHANGE,	// post message to update UI
								iTrack, iTrackProp);
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

void CSeqMapping::Sort(int iProp, bool bDescending)
{
	WCritSec::Lock	lock(m_csMapping);	// serialize access to mappings
	m_iSortProp = iProp;
	m_bSortDescending = bDescending;
	qsort(m_arrMapping.GetData(), m_arrMapping.GetSize(), sizeof(CMapping), SortCompare);
}

int CSeqMapping::m_iSortProp;	// index of property to sort mappings by
bool CSeqMapping::m_bSortDescending;	// true if sort should be descending

int CSeqMapping::SortCompare(const void *arg1, const void *arg2)
{
	const CMapping*	pMapping1 = (CMapping *)arg1;
	const CMapping*	pMapping2 = (CMapping *)arg2;
	int	nResult = CTrack::Compare(pMapping1->GetProperty(m_iSortProp), pMapping2->GetProperty(m_iSortProp));
	if (m_bSortDescending)
		nResult = -nResult;
	return nResult;
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

void CSeqMapping::GetInputMidiMsg(const CIntArrayEx& arrSelection, CIntArrayEx& arrInMidiMsg) const
{
	int	nSels = arrSelection.GetSize();
	arrInMidiMsg.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected mapping
		int	iMapping = arrSelection[iSel];
		arrInMidiMsg[iSel] = m_arrMapping[iMapping].GetInputMidiMsg();
	}
}

void CSeqMapping::SetInputMidiMsg(const CIntArrayEx& arrSelection, DWORD nInMidiMsg)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected mapping
		int	iMapping = arrSelection[iSel];
		m_arrMapping[iMapping].SetInputMidiMsg(nInMidiMsg);	// set input MIDI message
	}
}

void CSeqMapping::SetInputMidiMsg(const CIntArrayEx& arrSelection, const CIntArrayEx& arrInMidiMsg)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected mapping
		int	iMapping = arrSelection[iSel];
		m_arrMapping[iMapping].SetInputMidiMsg(arrInMidiMsg[iSel]);
	}
}
