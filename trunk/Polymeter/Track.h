// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		07dec18	add used track flags
		02		11dec18	add range types
		03		13dec18	add import/export steps to track array
		04		14dec18	add sort to modulation array
        05      15dec18	add find by name to track array
		06		15dec18	add import/export to packed modulations array
		07		17dec18	move MIDI file types into class scope
		08		18dec18	add import/export tracks
		09		19dec18	refactor property info to support value range
		10		25jan19	add modulation crawler to track array
		11		27jan19	cache type name strings instead of loading every time
		12		03feb19	add return value to track array's import MIDI file
		13		12dec19	add GetPeriod
		14		17feb20	move MIDI event class into track base
		15		29feb20	add MIDI event array methods
		16		16mar20	move get step index into track
		17		19mar20	add MIDI message name lookup
		18		17apr20	add track color
		19		30apr20	add step velocity accessors
		20		28sep20	add sort methods to track group array
		21		30sep20	add get track selection to track group array
		22		07oct20	in stretch, make interpolation optional
		23		16nov20	add tick dependencies
		24		01dec20	add dub array method to detect last duplicate
		25		16dec20	add loop range class
		26		08jun21	add cast in MIDI stream event operator to fix warning
        27		11nov21	refactor modulation crawler to support levels
		28		19nov21	in track array, add find type
		29		21jan22	add per-channel note overlap methods
		30		05feb22	add step tie accessors
		31		15feb22	add validate modulations method
		32		19feb22	use INI file class directly instead of via profile
		33		19may22	add offset method to loop range class

*/

#pragma once

#include "ArrayEx.h"
#include "FixedArray.h"
#include "Properties.h"	// for option info
#include "Midi.h"		// for MIDI message enums
#include "MidiFile.h"	// for import track array class
#include "CritSec.h"

class CTrackBase {
public:
// Constants
	enum {	// track properties
		#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) PROP_##name,
		#include "TrackDef.h"		// generate enumeration
		PROPERTIES,
		PROP_COLOR = -1,			// color property isn't a track grid column
	};
	enum {
		INIT_STEPS = 32,			// initial number of steps
		MIN_DUB_TIME = INT_MIN,		// minimum dub time
	};
	enum {	// step bitmasks; these define the layout of a sequencer step
		// for track types other than note, the velocity carries the event's
		// parameter if any, but the tie bit is inapplicable and should be zero
		SB_VELOCITY		= 0x7f,		// velocity for note tracks, else event parameter if any
		SB_TIE			= 0x80,		// if non-zero, note is tied; applies to note tracks only
		// this flag can be used when toggling multiple steps; applies to note tracks only
		SB_TOGGLE_TIE	= 0x100,	// if non-zero, toggle note's tie state
		TIE_BIT_SHIFT	= 7,		// shift this many places to access tie bit
	};
	enum {	// mute bitmasks
		MB_MUTE			= 0x01,		// mute state
		MB_TOGGLE		= 0x02,		// toggle mute state
	};
	enum {	// track types
		#define TRACKTYPEDEF(name) TT_##name,
		#include "TrackDef.h"	// generate track type enum
		TRACK_TYPES
	};
	enum {	// modulation types
		#define MODTYPEDEF(name) MT_##name,
		#include "TrackDef.h"	// generate modulation type enum
		MODULATION_TYPES
	};
	enum {	// modulation type bitmasks
		#define MODTYPEDEF(name) MTB_##name = 1 << MT_##name,	// limits us to 32 types
		#include "TrackDef.h"	// generate modulation type bitmasks
		// define bitmask for modulation types that fully support recursion
		RECURSIVE_MOD_TYPE_MASK = MTB_Mute | MTB_Position | MTB_Offset,
	};
	enum {
		#define RANGETYPEDEF(name) RT_##name,
		#include "TrackDef.h"	// generate range type enum
		RANGE_TYPES
	};
	enum {	// used track flags, for GetUsedTracks and GetUsedTrackCount
		UT_NO_MUTE		= 0x01,		// exclude muted tracks
		UT_NO_MODULATOR	= 0x02,		// exclude modulator tracks
	};
	enum {	// per-channel note overlap methods
		CHAN_NOTE_OVERLAP_SPLIT,	// split overlapping notes into shorter notes
		CHAN_NOTE_OVERLAP_MERGE,	// merge overlapping notes into one long note
		CHAN_NOTE_OVERLAP_METHODS,
	};
	enum {	// modulation errors, for CheckModulations
		MODERR_NO_ERROR,
		MODERR_UNSUPPORTED_MOD_TYPE,	// modulation type isn't supported
		MODERR_INFINITE_LOOP,			// infinite modulation loop detected
		MODULATION_ERRORS
	};

// Types
	struct PROPERTY_INFO {	// information about each track property
		LPCTSTR	pszName;		// internal name
		int		nNameID;		// string resource ID of display name
		const type_info	*pType;	// pointer to type info
		int		iPropType;		// property type; enumerated below
		int		nOptions;		// if combo box, number of options
		const CProperties::OPTION_INFO	*pOption;	// if combo box, pointer to array of options
		CComVariant	vMinVal;	// minimum value, if applicable
		CComVariant	vMaxVal;	// maximum value, if applicable
	};
	typedef BYTE STEP;				// sequencer step
	typedef CByteArrayEx CStepArray;	// array of sequencer steps
	typedef CArrayEx<CStepArray, CStepArray&> CStepArrayArray;	// array of step arrays
	typedef CBoolArrayEx CMuteArray;	// array of mutes
	struct MIDI_STREAM_EVENT {
		DWORD   dwDeltaTime;		// ticks since last event
		DWORD   dwStreamID;			// reserved; must be zero
		DWORD   dwEvent;			// event type and parameters
	};
	typedef CArrayEx<MIDI_STREAM_EVENT, MIDI_STREAM_EVENT&> CMidiEventStream;
	class CMidiEvent {
	public:
		CMidiEvent() {};
		CMidiEvent(DWORD nTime, DWORD dwEvent);
		bool	operator==(const CMidiEvent &evt) const;
		bool	operator!=(const CMidiEvent &evt) const;
		bool	operator<(const CMidiEvent &evt) const;
		bool	operator>(const CMidiEvent &evt) const;
		bool	operator<=(const CMidiEvent &evt) const;
		bool	operator>=(const CMidiEvent &evt) const;
		operator	MIDI_STREAM_EVENT() const;	// casting operator
		int		m_nTime;		// time in ticks
		DWORD	m_dwEvent;		// MIDI event
		bool	TimeInRange(int nStartTime, int nEndTime) const;
	};
	class CMidiEventArray : public CArrayEx<CMidiEvent, CMidiEvent&> {
	public:
		int		Chase(int nTime) const;
		int		FindEvents(int nStartTime, int nEndTime, int& iEndEvent) const;
		void	GetEvents(int nStartTime, int nEndTime, CMidiEventArray& arrEvent) const;
		void	TruncateEvents(int nStartTime);
		void	DeleteEvents(int nStartTime, int nEndTime);
		void	InsertEvents(int nInsertTime, int nDuration, CMidiEventArray& arrEvent);
		void	Dump() const;
	};
	class CDub {
	public:
	// Construction
		CDub();
		CDub(int nTime, bool bMute);

	// Public data
		int		m_nTime;			// timestamp, in absolute ticks
		union {
			bool	m_bMute;		// true if muted
			int		m_bMute32;		// includes unused bytes
		};
		bool operator<=(const CDub& dub) const { return m_nTime <= dub.m_nTime; }
	};
	class CDubArray : public CArrayEx<CDub, CDub&> {
	public:
		int		FindDub(int nTime, int iStartDub = 0) const;
		bool	GetDubs(int nStartTime, int nEndTime) const;
		void	GetDubs(int nStartTime, int nEndTime, CDubArray& arrDub) const;
		void	SetDubs(int nStartTime, int nEndTime, bool bMute);
		void	AddDub(int nTime, bool bMute);
		void	InsertDub(int iDub, int nTime, bool bMute);
		void	DeleteDubs(int nStartTime, int nEndTime);
		void	InsertDubs(int nTime, CDubArray& arrDub); 
		void	RemoveDuplicates();
		void	Dump() const;
		bool	IsLastDuplicate(W64INT iDub) const;
	};
	typedef CArrayEx<CDubArray, CDubArray&> CDubArrayArray;	// array of dub arrays
	class CModulation {
	public:
		CModulation();
		CModulation(int iType, int iSource);
		bool	operator==(const CModulation& mod) const;
		bool	operator!=(const CModulation& mod) const;
		bool	IsRecursiveType() const;
		int		m_iType;	// index of modulation type, enumerated above
		int		m_iSource;	// index of modulation source in track array, or -1 if none
	};
	class CModulationArray : public CArrayEx<CModulation, CModulation&> {
	public:
		void	SortByType();
		void	SortBySource();

	protected:
		static	int		SortCompareByType(const void *arg1, const void *arg2);
		static	int		SortCompareBySource(const void *arg1, const void *arg2);
	};
	typedef CArrayEx<CModulationArray, CModulationArray&> CModulationArrayArray;
	class CPackedModulation {
	public:
		CPackedModulation();
		CPackedModulation(int iType, int iSource, int iTarget);
		int		m_iType;	// index of modulation type
		int		m_iSource;	// index of source track
		int		m_iTarget;	// index of target track
		static	int		SortCompare(const void *p1, const void *p2);
	};
	class CPackedModulationArray : public CArrayEx<CPackedModulation, CPackedModulation&> {
	public:
		void	Import(LPCTSTR pszPath, int nTracks);
		void	Export(LPCTSTR pszPath) const;
		void	SortByTarget();
	};
	class CModulationError : public CPackedModulation {
	public:
		CModulationError();
		CModulationError(int iType, int iSource, int iTarget, int nError);
		CModulationError(const CPackedModulation& mod, int nError);
		int		m_nError;	// modulation error code, enumerated above
	};
	class CModulationErrorArray : public CArrayEx<CModulationError, CModulationError&> {
	public:
		void	SortByTarget();
	};
	struct STEP_EVENT {
		int		nStart;		// event start time, as step index
		int		nDuration;	// event duration, in steps (note events only)
		int		nVelocity;	// event velocity, as MIDI value
	};
	typedef CArrayEx<STEP_EVENT, STEP_EVENT&> CStepEventArray;
	class CTickDepends {
	public:
		#define TICKDEPENDSDEF(x) int x;
		#include "TrackDef.h"	// generate declarations for tick-dependent member vars
		CIntArrayEx	m_arrDubTime;	// array of song dub times in ticks
	};
	typedef CArrayEx<CTickDepends, CTickDepends&> CTickDependsArray;
	class CLoopRange {
	public:
		CLoopRange();
		CLoopRange(int nFrom, int nTo);
		bool	IsValid() const;
		void	Offset(int nOffset);
		int		m_nFrom;	// start position of loop, in ticks
		int		m_nTo;		// end position of loop, in ticks
	};

// Attributes
	static	const PROPERTY_INFO&	GetPropertyInfo(int iProp);
	static	LPCTSTR	GetPropertyInternalName(int iProp);
	static	int		FindPropertyInternalName(LPCTSTR pszName);
	static	int		GetPropertyNameID(int iProp);
	static	void	GetPropertyRange(int iProp, int& nMinVal, int& nMaxVal);
	static	CString	GetTrackTypeName(int iType);
	static	LPCTSTR	GetTrackTypeInternalName(int iType);
	static	CString	GetModulationTypeName(int iType);
	static	LPCTSTR	GetModulationTypeInternalName(int iType);
	static	int		FindModulationTypeInternalName(LPCTSTR pszName);
	static	CString	GetRangeTypeName(int iType);
	static	LPCTSTR	GetRangeTypeInternalName(int iType);
	static	LPCTSTR	GetMidiChannelVoiceMsgName(int iMsg);
	static	LPCTSTR	GetMidiSystemStatusMsgName(int iMsg);
	static	int		FindMidiChannelVoiceMsgName(LPCTSTR pszName);
	static	int		FindMidiSystemStatusMsgName(LPCTSTR pszName);
	static	CString	GetTrackPrefixString();
	static	CString	GetTrackNoneString();

// Operations
	static	void	LoadStringResources();

protected:
// Constants
	static const PROPERTY_INFO m_arrPropertyInfo[PROPERTIES];
	static const CProperties::OPTION_INFO m_oiTrackType[TRACK_TYPES];
	static const CProperties::OPTION_INFO m_oiModulationType[MODULATION_TYPES];
	static const CProperties::OPTION_INFO m_oiRangeType[RANGE_TYPES];
	static CString m_sTrackTypeName[TRACK_TYPES];
	static CString m_sModulationTypeName[MODULATION_TYPES];
	static CString m_sRangeTypeName[RANGE_TYPES];
	static CString m_sTrackPrefix;	// prefix for unnamed tracks
	static CString m_sTrackNone;	// caption for unspecified track
	static const LPCTSTR m_arrMidiChannelVoiceMsgName[MIDI_CHANNEL_VOICE_MESSAGES];
	static const LPCTSTR m_arrMidiSystemStatusMsgName[MIDI_SYSTEM_STATUS_MESSAGES];
};

inline const CTrackBase::PROPERTY_INFO& CTrackBase::GetPropertyInfo(int iProp)
{
	ASSERT(iProp >= 0 && iProp < PROPERTIES);
	return m_arrPropertyInfo[iProp];
}

inline LPCTSTR CTrackBase::GetPropertyInternalName(int iProp)
{
	return GetPropertyInfo(iProp).pszName;
}

inline int CTrackBase::FindPropertyInternalName(LPCTSTR pszName)
{
	for (int iProp = 0; iProp < PROPERTIES; iProp++) {	// for each property
		if (!_tcscmp(pszName, m_arrPropertyInfo[iProp].pszName))
			return iProp;
	}
	return -1;
}

inline int CTrackBase::GetPropertyNameID(int iProp)
{
	return GetPropertyInfo(iProp).nNameID;
}

inline void	CTrackBase::GetPropertyRange(int iProp, int& nMinVal, int& nMaxVal)
{
	const PROPERTY_INFO&	info = GetPropertyInfo(iProp);
	if (info.nOptions) {	// if options list
		nMinVal = 0;
		nMaxVal = info.nOptions - 1;
	} else {	// normal case
		ASSERT(info.vMinVal.vt == VT_I4 && info.vMaxVal.vt == VT_I4);	// integer ranges only
		nMinVal = info.vMinVal.intVal;
		nMaxVal = info.vMaxVal.intVal;
	}
}

inline CString CTrackBase::GetTrackTypeName(int iType)
{
	ASSERT(iType >= 0 && iType < TRACK_TYPES);
	return m_sTrackTypeName[iType];
}

inline LPCTSTR CTrackBase::GetTrackTypeInternalName(int iType)
{
	ASSERT(iType >= 0 && iType < TRACK_TYPES);
	return m_oiTrackType[iType].pszName;
}

inline CString CTrackBase::GetModulationTypeName(int iType)
{
	ASSERT(iType >= 0 && iType < MODULATION_TYPES);
	return m_sModulationTypeName[iType];
}

inline LPCTSTR CTrackBase::GetModulationTypeInternalName(int iType)
{
	ASSERT(iType >= 0 && iType < MODULATION_TYPES);
	return m_oiModulationType[iType].pszName;
}

inline int CTrackBase::FindModulationTypeInternalName(LPCTSTR pszName)
{
	for (int iType = 0; iType < MODULATION_TYPES; iType++) {	// for each modulation type
		if (!_tcscmp(pszName, m_oiModulationType[iType].pszName))
			return iType;
	}
	return -1;
}

inline CString CTrackBase::GetRangeTypeName(int iType)
{
	ASSERT(iType >= 0 && iType < RANGE_TYPES);
	return m_sRangeTypeName[iType];
}

inline LPCTSTR CTrackBase::GetRangeTypeInternalName(int iType)
{
	ASSERT(iType >= 0 && iType < RANGE_TYPES);
	return m_oiRangeType[iType].pszName;
}

inline LPCTSTR CTrackBase::GetMidiChannelVoiceMsgName(int iMsg)
{
	ASSERT(iMsg >= 0 && iMsg < MIDI_CHANNEL_VOICE_MESSAGES);
	return m_arrMidiChannelVoiceMsgName[iMsg];
}

inline LPCTSTR CTrackBase::GetMidiSystemStatusMsgName(int iMsg)
{
	ASSERT(iMsg >= 0 && iMsg < MIDI_SYSTEM_STATUS_MESSAGES);
	return m_arrMidiSystemStatusMsgName[iMsg];
}

inline int CTrackBase::FindMidiChannelVoiceMsgName(LPCTSTR pszName)
{
	return ARRAY_FIND(m_arrMidiChannelVoiceMsgName, pszName);
}

inline int CTrackBase::FindMidiSystemStatusMsgName(LPCTSTR pszName)
{
	return ARRAY_FIND(m_arrMidiSystemStatusMsgName, pszName);
}

inline CString CTrackBase::GetTrackPrefixString()
{
	return m_sTrackPrefix;
}

inline CString CTrackBase::GetTrackNoneString()
{
	return m_sTrackNone;
}

inline CTrackBase::CMidiEvent::CMidiEvent(DWORD nTime, DWORD dwEvent)
{
	m_nTime = nTime;
	m_dwEvent = dwEvent;
}

inline bool CTrackBase::CMidiEvent::operator==(const CMidiEvent &evt) const
{
	return m_nTime == evt.m_nTime && m_dwEvent == evt.m_dwEvent;
}

inline bool CTrackBase::CMidiEvent::operator!=(const CMidiEvent &evt) const
{
	return !operator==(evt);
}

inline bool CTrackBase::CMidiEvent::operator<(const CMidiEvent &evt) const
{
	return m_nTime < evt.m_nTime || (m_nTime == evt.m_nTime && m_dwEvent < evt.m_dwEvent);
}

inline bool CTrackBase::CMidiEvent::operator>(const CMidiEvent &evt) const
{
	return !operator>=(evt);
}

inline bool CTrackBase::CMidiEvent::operator<=(const CMidiEvent &evt) const
{
	return m_nTime < evt.m_nTime || (m_nTime == evt.m_nTime && m_dwEvent <= evt.m_dwEvent);
}

inline bool CTrackBase::CMidiEvent::operator>=(const CMidiEvent &evt) const
{
	return !operator<(evt);
}

inline CTrackBase::CMidiEvent::operator CTrackBase::MIDI_STREAM_EVENT() const
{
	MIDI_STREAM_EVENT	evt = {static_cast<DWORD>(m_nTime), 0, m_dwEvent};
	return evt;
}

inline bool CTrackBase::CMidiEvent::TimeInRange(int nStartTime, int nEndTime) const
{
	return m_nTime >= nStartTime && m_nTime < nEndTime;
}

inline CTrackBase::CDub::CDub()
{
}

inline CTrackBase::CDub::CDub(int nTime, bool bMute)
{
	m_nTime = nTime;
	m_bMute32 = bMute;		// avoid uninitialized bytes
}

inline int CTrackBase::CDubArray::FindDub(int nTime, int iStartDub) const
{
	// find nearest dub at or before specified time
	int	nDubs = GetSize();
	if (!nDubs)	// if no dubs
		return -1;	// return error
	for (int iDub = iStartDub; iDub < nDubs; iDub++) {	// for each dub
		if (GetAt(iDub).m_nTime > nTime)	// if dub occurs after specified time
			return iDub - 1;	// return previous dub
	}
	return nDubs - 1;	// return last dub
}

inline void CTrackBase::CDubArray::AddDub(int nTime, bool bMute)
{
	CDub	dub(nTime, bMute);
	Add(dub);
}

inline void CTrackBase::CDubArray::InsertDub(int iDub, int nTime, bool bMute)
{
	CDub	dub(nTime, bMute);
	InsertAt(iDub, dub);
}

inline bool CTrackBase::CDubArray::IsLastDuplicate(W64INT iDub) const
{
	return iDub > 0 && iDub == GetSize() - 1 && GetAt(iDub - 1).m_bMute == GetAt(iDub).m_bMute;
}

inline CTrackBase::CModulation::CModulation()
{
}

inline CTrackBase::CModulation::CModulation(int iType, int iSource)
{
	m_iType = iType;
	m_iSource = iSource;
}

inline bool CTrackBase::CModulation::operator==(const CModulation& mod) const
{
	return mod.m_iType == m_iType && mod.m_iSource == m_iSource;
}

inline bool CTrackBase::CModulation::operator!=(const CModulation& mod) const
{
	return !operator==(mod);
}

inline bool CTrackBase::CModulation::IsRecursiveType() const
{
	return ((1 << m_iType) & CTrackBase::RECURSIVE_MOD_TYPE_MASK) != 0;
}

inline CTrackBase::CPackedModulation::CPackedModulation()
{
}

inline CTrackBase::CPackedModulation::CPackedModulation(int iType, int iSource, int iTarget)
{
	m_iType = iType;
	m_iSource = iSource;
	m_iTarget = iTarget;
}

inline CTrackBase::CModulationError::CModulationError()
{
}

inline CTrackBase::CModulationError::CModulationError(int iType, int iSource, int iTarget, int nError) 
	: CPackedModulation(iType, iSource, iTarget)
{
	m_nError = nError;
}

inline CTrackBase::CModulationError::CModulationError(const CPackedModulation& mod, int nError) 
	: CPackedModulation(mod)
{
	m_nError = nError;
}

inline CTrackBase::CLoopRange::CLoopRange()
{
}

inline CTrackBase::CLoopRange::CLoopRange(int nFrom, int nTo)
{
	m_nFrom = nFrom;
	m_nTo = nTo;
}

inline bool CTrackBase::CLoopRange::IsValid() const
{
	return m_nFrom < m_nTo;
}

inline void CTrackBase::CLoopRange::Offset(int nOffset)
{
	m_nFrom += nOffset;
	m_nTo += nOffset;
}

class CTrack : public CTrackBase {
public:
// Construction
	CTrack();
	CTrack(bool bInit);

// Public data
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) type m_##prefix##name;
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate definitions of track property member vars
	CStepArray	m_arrStep;		// array of steps
	CDubArray	m_arrDub;		// array of dubs
	UINT	m_nUID;				// unique ID
	int		m_iDub;				// index of current dub
	CModulationArray	m_arrModulator;	// array of track indices of modulators
	COLORREF	m_clrCustom;	// custom track color or -1 if none

// Attributes
	int		GetLength() const;
	int		GetPeriod() const;
	int		GetUsedStepCount() const;
	void	SetDefaults();
	bool	IsNote() const;
	bool	IsModulator() const;
	bool	IsModulated() const;
	void	GetPropertyValue(int iProp, void *pBuf, int nLen) const;
	int		GetStepIndex(LONGLONG nPos) const;
	bool	IsStepIndex(int iStep) const;
	int		GetStepVelocity(int iStep) const;
	void	SetStepVelocity(int iStep, int nVelocity);
	bool	GetStepTie(int iStep) const;
	void	SetStepTie(int iStep, bool bTie);
	void	GetTickDepends(CTickDepends& tickDepends) const;
	void	SetTickDepends(const CTickDepends& tickDepends);
	void	ScaleTickDepends(double fScale);

// Operations
	int		CompareProperty(int iProp, const CTrack& track) const;
	template<class T> static int Compare(const T& a, const T& b);
	CString	PropertyToString(int iProp) const;
	bool	StringToProperty(int iProp, const CString& str);
	bool	ValidateProperty(int iProp) const;
	void	CopyKeepingID(const CTrack& track);
	void	DumpModulations() const;
	void	GetEvents(CStepEventArray& arrNote) const;
	bool	Stretch(double fScale, CStepArray& arrStep, bool bInterpolate = true) const;
	static	void	Resample(const double *pInSamp, W64INT nInSamps, double *pOutSamp, W64INT nOutSamps, bool bInterpolate = true);

protected:
// Types
	struct PROPERTY_FIELD {	// property field descriptor
		int		nOffset;	// byte offset of property within CTrack
		int		nLen;		// size of property in bytes
	};

// Constants
	static const PROPERTY_FIELD	m_arrPropertyField[PROPERTIES];	// array of property field descriptors
};

inline CTrack::CTrack()
{
	m_nUID = 0;
	m_iDub = 0;
}

inline CTrack::CTrack(bool bInit)
{
	UNREFERENCED_PARAMETER(bInit);
	SetDefaults();
}

inline void CTrack::CopyKeepingID(const CTrack& track)
{
	UINT	nUID = m_nUID;	// save our ID
	*this = track;	// copy overwrites our ID
	m_nUID = nUID;	// restore our ID
}

inline int CTrack::GetLength() const
{
	return m_arrStep.GetSize();
}

inline int CTrack::GetPeriod() const
{
	return m_arrStep.GetSize() * m_nQuant;
}

inline bool CTrack::IsNote() const
{
	return m_iType == TT_NOTE;
}

inline bool CTrack::IsModulator() const
{
	return m_iType == TT_MODULATOR;
}

inline bool CTrack::IsModulated() const
{
	return m_arrModulator.GetSize() > 0;
}

inline bool CTrack::IsStepIndex(int iStep) const
{
	return m_arrStep.IsIndex(iStep);
}

inline int CTrack::GetStepVelocity(int iStep) const
{
	return m_arrStep[iStep] & SB_VELOCITY;
}

inline void CTrack::SetStepVelocity(int iStep, int nVelocity)
{
	m_arrStep[iStep] &= ~SB_VELOCITY;
	m_arrStep[iStep] |= nVelocity & SB_VELOCITY;
}

inline bool CTrack::GetStepTie(int iStep) const
{
	return (m_arrStep[iStep] & SB_TIE) != 0;
}

inline void CTrack::SetStepTie(int iStep, bool bTie)
{
	m_arrStep[iStep] &= ~SB_TIE;
	m_arrStep[iStep] |= bTie << TIE_BIT_SHIFT;
}

class CTrackArray : public CArrayEx<CTrack, CTrack&>, public CTrackBase {
public:
// Constants
	enum {	// find flags
		FINDF_IGNORE_CASE		= 0x01,		// case-insensitive
		FINDF_PARTIAL_MATCH		= 0x02,		// match substrings
		FINDF_NO_WRAP_SEARCH	= 0x04,		// don't wrap search
	};
	enum {	// modulation linkage flags
		MODLINKF_SOURCE			= 0x01,		// source links
		MODLINKF_TARGET			= 0x02,		// target links
	};

// Operations
	void	ImportSteps(LPCTSTR pszPath);
	void	ExportSteps(const CIntArrayEx *parrSelection, LPCTSTR pszPath) const;
	void	ImportTracks(LPCTSTR pszPath);
	void	ExportTracks(const CIntArrayEx *parrSelection, LPCTSTR pszPath) const;
	int		FindName(const CString& sName, int iStart = 0, UINT nFlags = 0) const;
	int		FindType(int iType, int iStart = 0) const;
	void	GetModulationTargets(CModulationArrayArray& arrTarget) const;
	bool	CheckModulations(CModulationErrorArray& arrError) const;
	void	GetLinkedTracks(const CIntArrayEx& arrSelection, CPackedModulationArray& arrMod, UINT nLinkFlags = MODLINKF_SOURCE, int nLevels = 1) const;

protected:
// Types
	class CModulationCrawler {
	public:
		CModulationCrawler(const CTrackArray& arrTrack, CPackedModulationArray& arrMod, UINT nLinkFlags = MODLINKF_SOURCE, int nLevels = 1);
		void	Crawl(const CIntArrayEx& arrSelection);
		bool	LoopCheck();

	protected:
		const CTrackArray&	m_arrTrack;	// reference to parent track array
		CPackedModulationArray&	m_arrMod;	// reference to output array of modulations
		CBoolArrayEx	m_arrIsCrawled;	// for each track, true if track has already been crawled
		CModulationArrayArray	m_arrTarget;	// for each track, array of modulation targets
		UINT	m_nLinkFlags;	// modulation linkage flags; see enum above
		int		m_nLevels;		// maximum number of modulation levels
		int		m_nDepth;		// current recursion depth of crawl
		void	Recurse(int iTrack);
		bool	LoopCheckRecurse(int iTrack);
	};

// Helpers
	static	void	OnImportTracksError(int nErrMsgFormatID, int iRow, int iCol);
};

class CImportTrackArray : public CTrackArray {
public:
	bool	ImportMidiFile(LPCTSTR pszPath, int nOutTimeDiv, double fQuantization);
	bool	ImportMidiFile(const CMidiFile::CMidiTrackArray& arrInTrack, const CStringArrayEx& arrInTrackName, int nInTimeDiv, int nOutTimeDiv, double fQuantization);

protected:
	struct TRACK_INFO {
		int		iTrack;			// index in track array
		int		nStartTime;		// current note's start time in ticks, or -1 if none
		int		nVelocity;		// current note's velocity
	};
	static	int		ImportSortCmp(const void *arg1, const void *arg2);
};

class CTrackGroup {
public:
	CString	m_sName;				// group name
	CIntArrayEx	m_arrTrackIdx;		// array of track indices
};

class CIniFile;

class CTrackGroupArray : public CArrayEx<CTrackGroup, CTrackGroup&> {
public:
	void	OnTrackArrayEdit(const CIntArrayEx& arrTrackMap);
	void	Read(CIniFile& fIni, LPCTSTR pszSection);
	void	Write(CIniFile& fIni, LPCTSTR pszSection) const;
	void	Dump() const;
	void	GetTrackRefs(CIntArrayEx& arrTrackIdx) const;
	void	SortByName(CPtrArrayEx *parrSortedPtr = NULL);
	void	SortByTrack(CPtrArrayEx *parrSortedPtr = NULL);
	void	GetTrackSelection(const CIntArrayEx& arrGroupSel, CIntArrayEx& arrTrackSel) const;

protected:
	static int	SortCompareName(const void *pElem1, const void *pElem2);
	static int	SortCompareTrack(const void *pElem1, const void *pElem2);
};
