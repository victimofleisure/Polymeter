// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version

*/

#pragma once

#include "mmsystem.h"
#include "Track.h"
#include "Midi.h"
#include "CritSec.h"

#define SEQ_DUMP_EVENTS 0

class CSequencer : public WObject, public CTrackBase {
public:
// Construction
	CSequencer();
	virtual ~CSequencer();

// Types
	struct STATS {
		int		nCallbacks;		// total number of callbacks
		int		nOverruns;		// number of callbacks that took too long 
		double	fCBTimeMin;		// minimum callback time in seconds
		double	fCBTimeMax;		// maximum callback time in seconds
		double	fCBTimeSum;		// total callback time in seconds
	};

// Constants
	enum {
		SEQERR_FIRST = INT_MAX - 256,
		#define SEQERRDEF(name) SEQERR_##name,
		#include "SequencerErrors.h"	// generate error enum
		SEQERR_LAST,
	};

// Attributes
	bool	IsOpen() const;
	bool	IsPlaying() const;
	bool	IsPaused() const;
	UINT	GetOutputDevice() const;
	void	SetOutputDevice(UINT iOutputDevice);
	bool	GetPosition(LONGLONG& nTicks);
	void	SetPosition(int nTicks);
	bool	GetPosition(MMTIME& time, UINT wType = TIME_TICKS);
	int		GetCallbackTime() const;
	int		GetCallbackLength() const;
	double	GetTempo() const;
	void	SetTempo(double fTempo);
	int		GetTimeDivision() const;
	bool	SetTimeDivision(int nTimeDiv);
	int		GetLatency() const;
	void	SetLatency(int nLatency);
	int		GetBufferSize() const;
	void	SetBufferSize(int nEvents);
	void	GetStatistics(STATS& stats) const;
	int		GetTrackCount() const;
	void	SetTrackCount(int nTracks);
	const CTrack&	GetTrack(int iTrack) const;
	void	SetTrack(int iTrack, const CTrack& track);
	CString	GetName(int iTrack) const;
	void	SetName(int iTrack, const CString& sName);
	int		GetChannel(int iTrack) const;
	void	SetChannel(int iTrack, int nChannel);
	int		GetNote(int iTrack) const;
	void	SetNote(int iTrack, int nNote);
	int		GetQuant(int iTrack) const;
	void	SetQuant(int iTrack, int nQuant);
	int		GetLength(int iTrack) const;
	void	SetLength(int iTrack, int nLength);
	int		GetOffset(int iTrack) const;
	void	SetOffset(int iTrack, int nOffset);
	int		GetSwing(int iTrack) const;
	void	SetSwing(int iTrack, int nSwing);
	int		GetVelocity(int iTrack) const;
	void	SetVelocity(int iTrack, int nVelocity);
	int		GetDuration(int iTrack) const;
	void	SetDuration(int iTrack, int nDuration);
	bool	GetMute(int iTrack) const;
	void	SetMute(int iTrack, bool bMute);
	BYTE	GetEvent(int iTrack, int iEvent) const;
	void	SetEvent(int iTrack, int iEvent, BYTE nVal);
	void	GetEvents(int iTrack, CByteArrayEx& arrEvent) const;
	void	SetEvents(int iTrack, const CByteArrayEx& arrEvent);
	void	GetTrackProperty(int iTrack, int iProp, CComVariant& var) const;
	void	SetTrackProperty(int iTrack, int iProp, const CComVariant& var);
	int		GetUsedTrackCount(bool bExcludeMuted = false) const;
	void	GetUsedTracks(CIntArrayEx& arrUsedTrack, bool bExcludeMuted = false) const;

// Operations
	bool	Play(bool bEnable);
	bool	Pause(bool bEnable);
	void	Abort();
	bool	Export(LPCTSTR pszPath, int nDuration);
	void	CopyTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack) const;
	void	InsertTracks(int iTrack, int nCount = 1);
	void	InsertTrack(int iTrack, CTrack& track);
	void	InsertTracks(int iTrack, CTrackArray& arrTrack);
	void	InsertTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack);
	void	DeleteTracks(int iTrack, int nCount = 1);
	void	DeleteTracks(const CIntArrayEx& arrSelection);
	void	ConvertPositionToBeat(LONGLONG nPos, LONGLONG& nBeat, LONGLONG& nTick) const;
	void	ConvertBeatToPosition(LONGLONG nBeat, LONGLONG nTick, LONGLONG& nPos) const;
	void	ConvertPositionToString(const LONGLONG& nPos, CString& sPos) const;
	bool	ConvertStringToPosition(const CString& sPos, LONGLONG& nPos) const;

protected:
// Types
	class CEvent {
	public:
		CEvent() {};
		CEvent(DWORD dwDeltaTime, DWORD dwEvent);
		bool	operator==(const CEvent &evt) const;
		bool	operator!=(const CEvent &evt) const;
		bool	operator<(const CEvent &evt) const;
		bool	operator>(const CEvent &evt) const;
		bool	operator<=(const CEvent &evt) const;
		bool	operator>=(const CEvent &evt) const;
		int		m_dwTime;			// time in ticks
		DWORD	m_dwEvent;			// MIDI event
	};
	typedef CArrayEx<CEvent, CEvent&> CEventArray;
	struct MIDI_STREAM_EVENT {
		DWORD   dwDeltaTime;		// ticks since last event
		DWORD   dwStreamID;			// reserved; must be zero
		DWORD   dwEvent;			// event type and parameters
	};
	typedef CArrayEx<MIDI_STREAM_EVENT, MIDI_STREAM_EVENT&> CMidiEventStream;

// Constants
	enum {
		BUFFERS = 2,				// number of playback buffers
		DEF_BUFFER_SIZE = 4096,		// default buffer size, in events
	};

// Member data
	HMIDISTRM	m_hStrm;			// MIDI stream handle
	MIDIHDR	m_arrMsgHdr[BUFFERS];	// array of MIDI message headers
	double	m_fTempo;				// tempo, in beats per minute
	UINT	m_iOutputDevice;		// index of output MIDI device
	int		m_nTimeDiv;				// time division, in pulses per quarter note
	int		m_nCBTime;				// time at start of next callback, in ticks
	int		m_nCBLen;				// length of a callback period, in ticks
	int		m_nLatency;				// desired latency of playback in milliseconds
	int		m_nBufferSize;			// size of event buffers, in events
	int		m_iBuffer;				// index of most recently queued buffer
	int		m_nStartPos;			// starting position of playback, in ticks
	int		m_nPosOffset;			// total position correction, in ticks
	bool	m_bIsPlaying;			// true if playing
	bool	m_bIsPaused;			// true if paused
	bool	m_bIsStopping;			// true if stopping
	bool	m_bIsTempoChange;		// true if tempo changed
	bool	m_bIsPositionChange;	// true if position changed
	STATS	m_stats;				// timing statistics
	CTrackArray	m_arrTrack;			// array of track instances
	CMidiEventStream	m_arrMidiEvent[BUFFERS];	// array of MIDI event stream buffers
	CEventArray	m_arrEvent;			// array of track events
	CEventArray	m_arrNoteOff;		// array of pending note off events
	WCritSec	m_csCallback;		// critical section for serializing access to callback shared state
#if SEQ_DUMP_EVENTS
	CArrayEx<CMidiEventArray, CMidiEventArray&>	m_arrDumpEvent;	// for debug only
#endif	// SEQ_DUMP_EVENTS

// Overridables
	virtual	void	OnMidiError(MMRESULT nResult);

// Helpers
	static	void	CALLBACK MidiOutProc(HMIDIOUT hMidiOut, UINT wMsg, W64UINT dwInstance, W64UINT dwParam1, W64UINT dwParam2);
	void	AddTrackEvents(int iTrack, int nCBStart);
	void	AddNoteOffs(int nCBStart, int nCBEnd);
	bool	OutputMidiBuffer();
	bool	WriteTimeDivision(int nTimeDiv);
	bool	WriteTempo(double fTempo);
	int		GetCallbackLength(int nLatency) const;
	void	UpdateCallbackLength();
	bool	ExportImpl(LPCTSTR pszPath, int nDuration);
#if SEQ_DUMP_EVENTS
	void	AddDumpEvent(const CMidiEventArray& arrEvt, int nEvents);
	void	DumpEvents(LPCTSTR pszPath);
#endif	// SEQ_DUMP_EVENTS
	friend class CSequencerReader;
};

class CSequencerReader : public CSequencer {
public:
	CSequencerReader(CSequencer& Seq);
	virtual ~CSequencerReader();
};

inline CSequencer::CEvent::CEvent(DWORD dwTime, DWORD dwEvent)
{
	m_dwTime = dwTime;
	m_dwEvent = dwEvent;
}

inline bool CSequencer::CEvent::operator==(const CEvent &evt) const
{
	return m_dwTime == evt.m_dwTime && m_dwEvent == evt.m_dwEvent;
}

inline bool CSequencer::CEvent::operator!=(const CEvent &evt) const
{
	return !operator==(evt);
}

inline bool CSequencer::CEvent::operator<(const CEvent &evt) const
{
	return m_dwTime < evt.m_dwTime || (m_dwTime == evt.m_dwTime && m_dwEvent < evt.m_dwEvent);
}

inline bool CSequencer::CEvent::operator>(const CEvent &evt) const
{
	return !operator>=(evt);
}

inline bool CSequencer::CEvent::operator<=(const CEvent &evt) const
{
	return m_dwTime < evt.m_dwTime || (m_dwTime == evt.m_dwTime && m_dwEvent <= evt.m_dwEvent);
}

inline bool CSequencer::CEvent::operator>=(const CEvent &evt) const
{
	return !operator<(evt);
}

inline bool CSequencer::IsOpen() const
{
	return m_hStrm != 0;
}

inline bool CSequencer::IsPlaying() const
{
	return m_bIsPlaying;
}

inline bool CSequencer::IsPaused() const
{
	return m_bIsPaused;
}

inline UINT CSequencer::GetOutputDevice() const
{
	return m_iOutputDevice;
}

inline double CSequencer::GetTempo() const
{
	return m_fTempo;
}

inline int CSequencer::GetCallbackTime() const
{
	return m_nCBTime;
}

inline int CSequencer::GetCallbackLength() const
{
	return m_nCBLen;
}

inline int CSequencer::GetTimeDivision() const
{
	return m_nTimeDiv;
}

inline int CSequencer::GetLatency() const
{
	return m_nLatency;
}

inline int CSequencer::GetBufferSize() const
{
	return m_nBufferSize;
}

inline void CSequencer::GetStatistics(STATS& stats) const
{
	stats = m_stats;
}

inline int CSequencer::GetTrackCount() const
{
	return m_arrTrack.GetSize();
}

inline const CTrack& CSequencer::GetTrack(int iTrack) const
{
	return m_arrTrack[iTrack];
}

inline int CSequencer::GetChannel(int iTrack) const
{
	return m_arrTrack[iTrack].m_nChannel;
}

inline CString CSequencer::GetName(int iTrack) const
{
	return m_arrTrack[iTrack].m_sName;
}

inline int CSequencer::GetNote(int iTrack) const
{
	return m_arrTrack[iTrack].m_nNote;
}

inline int CSequencer::GetQuant(int iTrack) const
{
	return m_arrTrack[iTrack].m_nQuant;
}

inline int CSequencer::GetLength(int iTrack) const
{
	return m_arrTrack[iTrack].m_arrEvent.GetSize();
}

inline int CSequencer::GetOffset(int iTrack) const
{
	return m_arrTrack[iTrack].m_nOffset;
}

inline int CSequencer::GetSwing(int iTrack) const
{
	return m_arrTrack[iTrack].m_nSwing;
}

inline int CSequencer::GetVelocity(int iTrack) const
{
	return m_arrTrack[iTrack].m_nVelocity;
}

inline int CSequencer::GetDuration(int iTrack) const
{
	return m_arrTrack[iTrack].m_nDuration;
}

inline bool CSequencer::GetMute(int iTrack) const
{
	return m_arrTrack[iTrack].m_bMute;
}

inline BYTE CSequencer::GetEvent(int iTrack, int iEvent) const
{
	return m_arrTrack[iTrack].m_arrEvent[iEvent];
}

inline void CSequencer::GetEvents(int iTrack, CByteArrayEx& arrEvent) const
{
	arrEvent = m_arrTrack[iTrack].m_arrEvent;
}
