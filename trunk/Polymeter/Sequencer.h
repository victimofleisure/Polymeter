// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
        01      19nov18	add recursive modulation
		02		02dec18	add conversion from milliseconds to position
		03		03jan19	add MIDI output capture

*/

#pragma once

#include "mmsystem.h"
#include "SeqTrackArray.h"
#include "ILockRingBuf.h"

#define SEQ_DUMP_EVENTS 0

class CSequencer : public CSeqTrackArray {
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
	struct MIDI_EVENT {
		int		dwTime;			// delta time in ticks
		DWORD	dwEvent;		// MIDI event
	};
	typedef CArrayEx<MIDI_EVENT, MIDI_EVENT&> CMidiEventArray;

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
	bool	IsRecording() const;
	int		GetOutputDevice() const;
	void	SetOutputDevice(int iOutputDevice);
	bool	GetPosition(LONGLONG& nTicks);
	void	SetPosition(int nTicks);
	bool	GetPosition(MMTIME& time, UINT wType = TIME_TICKS);
	int		GetCallbackTime() const;
	int		GetCallbackLength() const;
	double	GetTempo() const;
	void	SetTempo(double fTempo);
	int		GetTimeDivision() const;
	bool	SetTimeDivision(int nTimeDiv);
	int		GetMeter() const;
	void	SetMeter(int nMeter);
	int		GetLatency() const;
	void	SetLatency(int nLatency);
	int		GetBufferSize() const;
	void	SetBufferSize(int nEvents);
	void	GetStatistics(STATS& stats) const;
	void	GetInitialMidiEvents(CDWordArrayEx& arrMidiEvent) const;
	void	SetInitialMidiEvents(const CDWordArrayEx& arrMidiEvent);
	int		GetStepIndex(int iTrack, LONGLONG nPos) const;
	bool	GetSongMode() const;
	void	SetSongMode(bool bEnable);
	int		GetSongDurationSeconds() const;
	int		GetStartPosition() const;
	void	SetMidiOutputCapture(bool bEnable);
	void	GetMidiOutputEvents(CMidiEventArray& arrMidiEvent);

// Operations
	bool	Play(bool bEnable, bool bRecord = false);
	bool	Pause(bool bEnable);
	void	Abort();
	bool	Export(LPCTSTR pszPath, int nDuration, bool bSongMode, int nStartPos);
	bool	OutputLiveEvent(DWORD dwEvent);
	void	ConvertPositionToBeat(LONGLONG nPos, LONGLONG& nMeasure, LONGLONG& nBeat, LONGLONG& nTick) const;
	void	ConvertBeatToPosition(LONGLONG nMeasure, LONGLONG nBeat, LONGLONG nTick, LONGLONG& nPos) const;
	void	ConvertPositionToString(LONGLONG nPos, CString& sPos) const;
	bool	ConvertStringToPosition(const CString& sPos, LONGLONG& nPos) const;
	LONGLONG	ConvertPositionToSeconds(LONGLONG nPos) const;
	LONGLONG	ConvertSecondsToPosition(LONGLONG nSecs) const;
	LONGLONG	ConvertMillisecondsToPosition(LONGLONG nMillis) const;
	void	ConvertPositionToTimeString(LONGLONG nPos, CString& sTime) const;
	void	RecordDub(int iTrack);
	void	RecordDub(const CIntArrayEx& arrSelection);
	void	RecordDub();
	void	ChaseDubsFromCurPos();
	void	FlushMidiOutputEvents();

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
	struct MIDI_PARAMS {
	public:
		char	arrKeyAft[MIDI_CHANNELS][MIDI_NOTES];	// key aftertouch
		char	arrControl[MIDI_CHANNELS][MIDI_NOTES];	// continuous controllers
		char	arrPatch[MIDI_CHANNELS];	// program change
		char	arrChanAft[MIDI_CHANNELS];	// channel aftertouch
		char	arrWheel[MIDI_CHANNELS];	// pitch bend
	};

// Constants
	enum {
		BUFFERS = 2,				// number of playback buffers
		DEF_BUFFER_SIZE = 4096,		// default buffer size, in events
		MOD_MAX_RECURSIONS = 32,	// maximum number of modulation recursions
	};

// Member data
	HMIDISTRM	m_hStrm;			// MIDI stream handle
	MIDIHDR	m_arrMsgHdr[BUFFERS];	// array of MIDI message headers
	double	m_fTempo;				// tempo, in beats per minute
	int		m_iOutputDevice;		// index of output MIDI device
	int		m_nTimeDiv;				// time division, in pulses per quarter note
	int		m_nMeter;				// number of beats in a measure
	int		m_nCBTime;				// time at start of next callback, in ticks
	int		m_nCBLen;				// length of a callback period, in ticks
	int		m_nLatency;				// desired latency of playback in milliseconds
	int		m_nBufferSize;			// size of event buffers, in events
	int		m_iBuffer;				// index of most recently queued buffer
	int		m_nStartPos;			// starting position of playback, in ticks
	int		m_nPosOffset;			// total position correction, in ticks
	int		m_nRecursions;			// current depth of recursive modulation
	bool	m_bIsPlaying;			// true if playing
	bool	m_bIsPaused;			// true if paused
	bool	m_bIsStopping;			// true if stopping
	bool	m_bIsRecording;			// true if recording
	bool	m_bIsTempoChange;		// true if tempo changed
	bool	m_bIsPositionChange;	// true if position changed
	bool	m_bIsSongMode;			// true if applying track dubs
	bool	m_bIsOutputCapture;		// true if capturing output events
	STATS	m_stats;				// timing statistics
	CMidiEventStream	m_arrMidiEvent[BUFFERS];	// array of MIDI event stream buffers
	CEventArray	m_arrEvent;			// array of track events
	CEventArray	m_arrNoteOff;		// array of pending note off events
	CDWordArrayEx	m_arrInitMidiEvent;	// array of MIDI events to output at start of playback
	CILockRingBuf<DWORD>	m_qLiveEvent;	// thread-safe queue of live events to be output
	MIDI_PARAMS	m_MidiCache;		// cached values of MIDI parameters
	CMidiEventArray	m_arrOutputEvent;	// MIDI event buffer for capturing output

#if SEQ_DUMP_EVENTS
	CArrayEx<CMidiEventStream, CMidiEventStream&>	m_arrDumpEvent;	// for debug only
#endif	// SEQ_DUMP_EVENTS

// Overridables
	virtual	void	OnMidiError(MMRESULT nResult);

// Helpers
	static	void	CALLBACK MidiOutProc(HMIDIOUT hMidiOut, UINT wMsg, W64UINT dwInstance, W64UINT dwParam1, W64UINT dwParam2);
	int		GetNoteDuration(const CStepArray& arrStep, int nSteps, int iCurStep) const;
	bool	RecurseModulations(int iTrack, int nAbsEvtTime);
	void	MakeEvent(const CTrack& trk, DWORD dwTime, int nVal, CEvent& evt);
	void	AddTrackEvents(int iTrack, int nCBStart);
	void	AddNoteOffs(int nCBStart, int nCBEnd);
	bool	OutputMidiBuffer();
	bool	WriteTimeDivision(int nTimeDiv);
	bool	WriteTempo(double fTempo);
	int		GetCallbackLength(int nLatency) const;
	void	UpdateCallbackLength();
	bool	ExportImpl(LPCTSTR pszPath, int nDuration);
	void	ResetCachedParameters();
	void	OutputControlEvent(const CTrack& track, DWORD dwTime, int nVal);
	static	int		ApplyNoteRange(int nNote, int nRangeStart, int iRangeType);
	void	QueueOutputEvents(int nEvents);
#if SEQ_DUMP_EVENTS
	void	AddDumpEvent(const CMidiEventStream& arrEvt, int nEvents);
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

inline bool CSequencer::IsRecording() const
{
	return m_bIsRecording;
}

inline int CSequencer::GetOutputDevice() const
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

inline int CSequencer::GetMeter() const
{
	return m_nMeter;
}

inline void CSequencer::SetMeter(int nMeter)
{
	m_nMeter = nMeter;
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

inline void CSequencer::GetInitialMidiEvents(CDWordArrayEx& arrMidiEvent) const
{
	arrMidiEvent = m_arrInitMidiEvent;
}

inline void CSequencer::SetInitialMidiEvents(const CDWordArrayEx& arrMidiEvent)
{
	m_arrInitMidiEvent = arrMidiEvent;
}

inline bool CSequencer::GetSongMode() const
{
	return m_bIsSongMode;
}

inline void CSequencer::SetSongMode(bool bEnable)
{
	m_bIsSongMode = bEnable;
}

inline int CSequencer::GetStartPosition() const
{
	return m_nStartPos;
}
