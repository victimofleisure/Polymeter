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
		04		12jan19	add recursive position modulation
		05		20feb19	add note reference counts
		06		09sep19	add tempo event array
		07		17feb20	move MIDI event class into track base
		08		29feb20	add support for recording live events
		09		16mar20	add scale, index and voicing modulation
		10		20mar20	add mapping
		11		03apr20	add milliseconds to time format
		12		04apr20	add chord modulation
		13		14apr20	add send MIDI clock option
		14		19may20	refactor record dub methods to include conditional
		15		17jul20	refactor set song mode
		16		16dec20	add looping of playback
		17		07jun21	rename rounding functions
		18		30oct21	remove song duration method
		19		30dec21	remove unused function prototype

*/

#pragma once

#include "mmsystem.h"
#include "SeqTrackArray.h"
#include "Mapping.h"

#define SEQ_DUMP_EVENTS 0

class CSequencer : public CSeqTrackArray {
public:
// Construction
	CSequencer();
	virtual ~CSequencer();

// Types
	struct STATS {
		int		nCallbacks;		// total number of callbacks
		int		nUnderruns;		// number of callbacks that took too long 
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

// Public data
	CSeqMapping	m_mapping;		// maps MIDI input to parameters

// Attributes
	bool	IsOpen() const;
	bool	IsPlaying() const;
	bool	IsPaused() const;
	bool	IsStopping() const;
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
	bool	GetSongMode() const;
	void	SetSongMode(bool bEnable, bool bChaseDubs = false);
	int		GetStartPosition() const;
	void	SetMidiOutputCapture(bool bEnable);
	void	GetMidiOutputEvents(CMidiEventArray& arrMidiEvent);
	bool	GetNoteOverlapMode() const;
	void	SetNoteOverlapMode(bool bPrevent);
	int		GetRecordedEventCount() const;
	const CMidiEventArray&	GetRecordedEvents() const;
	void	SetRecordedEvents(const CMidiEventArray& arrEvent);
	int		GetRecordOffset() const;
	void	SetRecordOffset(int nTicks);
	void	SetSendMidiClock(bool bEnable);
	bool	IsValidMidiSongPosition(int nSongPos) const;
	bool	IsLooping() const;
	void	SetLooping(bool bEnable);
	void	GetLoopRange(CLoopRange& rngTicks) const;
	void	SetLoopRange(CLoopRange rngTicks);

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
	void	ConvertPositionToTimeString(LONGLONG nPos, CString& sTime) const;
	bool	ConvertTimeStringToPosition(const CString& sPos, LONGLONG& nPos) const;
	LONGLONG	ConvertPositionToSeconds(LONGLONG nPos) const;
	LONGLONG	ConvertSecondsToPosition(LONGLONG nSecs) const;
	LONGLONG	ConvertPositionToMilliseconds(LONGLONG nPos) const;
	LONGLONG	ConvertMillisecondsToPosition(LONGLONG nMillis) const;
	void	RecordDub(int iTrack);
	void	RecordDub(const CIntArrayEx& arrSelection);
	void	RecordDub();
	void	AddDubNow(int iTrack);
	void	AddDubNow(const CIntArrayEx& arrSelection);
	void	AddDubNow();
	void	ChaseDubsFromCurPos();
	void	FlushMidiOutputEvents();
	static	int		ModWrap(int nVal, int nModulo);
	void	AttachRecordedEvents(CMidiEventArray& arrEvent);
	void	RemoveAllRecordedEvents();
	void	AppendRecordedEvents(const CMidiEventArray& arrEvent);
	void	TruncateRecordedEvents(int nStartTime);
	void	DeleteRecordedEvents(int nStartTime, int nEndTime);
	void	InsertRecordedEvents(int nInsertTime, int nDuration, CMidiEventArray& arrEvent);

protected:
// Types
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
		OCTAVE = 12,				// size of an octave in semitones
	};

// Member data
	HMIDISTRM	m_hStrm;			// MIDI stream handle
	MIDIHDR	m_arrMsgHdr[BUFFERS];	// array of MIDI message headers
	double	m_fTempo;				// tempo, in beats per minute
	int		m_nAltTempo;			// altered tempo, in microseconds per quarter note
	int		m_iOutputDevice;		// index of output MIDI device
	int		m_nTimeDiv;				// time division, in ticks per quarter note
	int		m_nMeter;				// number of beats in a measure
	int		m_nCBTime;				// time at start of next callback, in ticks
	int		m_nCBLen;				// length of a callback period, in ticks
	int		m_nLatency;				// desired latency of playback in milliseconds
	int		m_nBufferSize;			// size of event buffers, in events
	int		m_iBuffer;				// index of most recently queued buffer
	int		m_nStartPos;			// starting position of playback, in ticks
	int		m_nPosOffset;			// total position correction, in ticks
	int		m_nRecursions;			// current depth of recursive modulation
	int		m_iRecordEvent;			// index of next recorded event to output
	int		m_nRecordOffset;		// offset added to recorded event times during playback, in ticks
	int		m_nMidiClockPeriod;		// number of ticks per MIDI clock (timebase / 24)
	int		m_nMidiClockTimer;		// number of ticks remaining until next MIDI clock
	bool	m_bIsPlaying;			// true if playing
	bool	m_bIsPaused;			// true if paused
	bool	m_bIsStopping;			// true if stopping
	bool	m_bIsRecording;			// true if recording
	bool	m_bIsTempoChange;		// true if tempo changed
	bool	m_bIsPositionChange;	// true if position changed
	bool	m_bIsSongMode;			// true if applying track dubs
	bool	m_bIsOutputCapture;		// true if capturing output events
	bool	m_bPreventNoteOverlap;	// true if preventing note overlaps
	bool	m_bIsSendingMidiClock;	// true if sending MIDI clock
	bool	m_bIsLooping;			// true if looping
	STATS	m_stats;				// timing statistics
	double	m_fLatencySecs;			// desired playback latency in fractional seconds
	CMidiEventStream	m_arrMidiEvent[BUFFERS];	// array of MIDI event stream buffers
	CMidiEventArray	m_arrEvent;			// array of track events
	CMidiEventArray	m_arrNoteOff;		// array of pending note off events
	CMidiEventArray	m_arrTempoEvent;	// array of tempo events
	CDWordArrayEx	m_arrInitMidiEvent;	// array of MIDI events to output at start of playback
	MIDI_PARAMS	m_MidiCache;		// cached values of MIDI parameters
	BYTE	m_arrNoteRef[MIDI_CHANNELS][MIDI_NOTES];	// note reference counts
	CMidiEventArray	m_arrOutputEvent;	// MIDI event buffer for capturing output
	CMidiEventArray	m_arrRecordEvent;	// MIDI event buffer for recording live MIDI
	CIntArrayEx	m_arrScale;			// array of notes in current scale, as signed note offsets
	CIntArrayEx	m_arrChord;			// array of tones to select from current scale, as scale indices
	CIntArrayEx	m_arrVoicing;		// array of voices to drop an octave, as one-origin indices from top
	CDWordArrayEx	m_arrMappedEvent;	// array of translated MIDI events from mapping
	CLoopRange	m_rngLoop;			// loop range in ticks

#if SEQ_DUMP_EVENTS
	CArrayEx<CMidiEventStream, CMidiEventStream&>	m_arrDumpEvent;	// for debug only
#endif	// SEQ_DUMP_EVENTS

// Overridables
	virtual	void	OnMidiError(MMRESULT nResult);

// Helpers
	static	void	CALLBACK MidiOutProc(HMIDIOUT hMidiOut, UINT wMsg, W64UINT dwInstance, W64UINT dwParam1, W64UINT dwParam2);
	int		GetNoteDuration(const CStepArray& arrStep, int nSteps, int iCurStep) const;
	bool	RecurseModulations(int iTrack, int nAbsEvtTime, int& nPosMod);
	void	AddTrackEvents(int iTrack, int nCBStart);
	void	AddNoteOffs(int nCBStart, int nCBEnd);
	bool	IsRecordedEventPlayback() const;
	void	AddRecordedEvents(int nCBStart, int nCBEnd);
	bool	OutputMidiBuffer();
	bool	WriteTimeDivision(int nTimeDiv);
	bool	WriteTempo(double fTempo);
	int		GetCallbackLength(int nLatency) const;
	void	UpdateCallbackLength();
	bool	ExportImpl(LPCTSTR pszPath, int nDuration);
	void	ResetCachedParameters();
	void	OutputControlEvent(const CTrack& track, int nTime, int nVal);
	static	int		ApplyNoteRange(int nNote, int nRangeStart, int iRangeType);
	void	QueueOutputEvents(int nEvents);
	void	FixNoteOverlaps();
	void	ChaseDubs(int nTime, bool bUpdateMutes = false);
	int		SumModulations(const CTrack& trk, int iModType, int nAbsEvtTime);
	DWORD	GetMidiSongPositionMsg(int nMidiSongPos) const;
	void	QuantizeStartPositionForSync(int& nMidiSongPos);
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
	CMuteArray	m_arrTrackMute;		// copy of track mutes
};

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

inline bool CSequencer::IsStopping() const
{
	return m_bIsStopping;
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

inline int CSequencer::GetStartPosition() const
{
	return m_nStartPos;
}

inline int CSequencer::ModWrap(int nVal, int nModulo)
{
	nVal %= nModulo;
	if (nVal < 0)
		nVal += nModulo;
	return nVal;
}

inline bool CSequencer::GetNoteOverlapMode() const
{
	return m_bPreventNoteOverlap;
}

inline int CSequencer::GetRecordedEventCount() const
{
	return m_arrRecordEvent.GetSize();
}

inline const CSequencer::CMidiEventArray& CSequencer::GetRecordedEvents() const
{
	return m_arrRecordEvent;
}

inline int CSequencer::GetRecordOffset() const
{
	return m_nRecordOffset;
}

inline void CSequencer::SetRecordOffset(int nTicks)
{
	m_nRecordOffset = nTicks;
}

inline void CSequencer::SetSendMidiClock(bool bEnable)
{
	ASSERT(!m_bIsPlaying);
	m_bIsSendingMidiClock = bEnable;
}

inline LONGLONG CSequencer::ConvertPositionToSeconds(LONGLONG nPos) const
{
	return Round64(static_cast<double>(nPos) / m_nTimeDiv / m_fTempo * 60);
}

inline LONGLONG CSequencer::ConvertSecondsToPosition(LONGLONG nSecs) const
{
	return Round64(static_cast<double>(nSecs) / 60 * m_fTempo * m_nTimeDiv);
}

inline LONGLONG CSequencer::ConvertPositionToMilliseconds(LONGLONG nPos) const
{
	return Round64(static_cast<double>(nPos) / m_nTimeDiv / m_fTempo * 60000);
}

inline LONGLONG CSequencer::ConvertMillisecondsToPosition(LONGLONG nMillis) const
{
	return Round64(static_cast<double>(nMillis) / 60000 * m_fTempo * m_nTimeDiv);
}

inline void CSequencer::RecordDub(int iTrack)
{
	if (IsRecording())
		AddDubNow(iTrack);
}

inline void CSequencer::RecordDub(const CIntArrayEx& arrSelection)
{
	if (IsRecording())
		AddDubNow(arrSelection);
}

inline void CSequencer::RecordDub()
{
	if (IsRecording())
		AddDubNow();
}

inline bool CSequencer::IsLooping() const
{
	return m_bIsLooping;
}

inline void CSequencer::SetLooping(bool bEnable)
{
	m_bIsLooping = bEnable;
}

inline void CSequencer::GetLoopRange(CLoopRange& rngTicks) const
{
	rngTicks = m_rngLoop;
}
