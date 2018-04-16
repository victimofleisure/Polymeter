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

#include "stdafx.h"
#include "Sequencer.h"
#include "Benchmark.h"
#include "float.h"
#include "VariantHelper.h"
#include "MidiFile.h"

#define CHECK(x) { MMRESULT nResult = x; if (MIDI_FAILED(nResult)) { OnMidiError(nResult); return false; } }

CSequencer::CSequencer()
{
	m_hStrm = 0;
	ZeroMemory(&m_arrMsgHdr, sizeof(m_arrMsgHdr));
	m_fTempo = 120;
	m_iOutputDevice = 0;
	m_nTimeDiv = 120;
	m_nCBTime = 0;
	m_nCBLen = 0;
	m_nLatency = 50;
	m_nBufferSize = DEF_BUFFER_SIZE;
	m_iBuffer = 0;
	m_nStartPos = 0;
	m_nPosOffset = 0;
	m_bIsPlaying = false;
	m_bIsPaused = false;
	m_bIsStopping = false;
	m_bIsTempoChange = false;
	m_bIsPositionChange = false;
	m_qLiveEvent.Create(MIDI_CHANNELS);
	ZeroMemory(&m_stats, sizeof(m_stats));
}

CSequencer::~CSequencer()
{
	Abort();
}

void CSequencer::OnMidiError(MMRESULT nResult)
{
	UNREFERENCED_PARAMETER(nResult);
}

bool CSequencer::GetPosition(MMTIME& time, UINT wType)
{
	time.wType = wType;
	CHECK(midiStreamPosition(m_hStrm, &time, sizeof(time)));
	if (time.wType != wType) {
		OnMidiError(SEQERR_BAD_TIME_FORMAT);
		return false;
	}
	return true;
}

bool CSequencer::WriteTimeDivision(int nTimeDiv)
{
	MIDIPROPTIMEDIV	mpTimeDiv = {sizeof(mpTimeDiv), nTimeDiv};
	CHECK(midiStreamProperty(m_hStrm, (BYTE *)&mpTimeDiv, MIDIPROP_SET | MIDIPROP_TIMEDIV));
	return true;
}

bool CSequencer::WriteTempo(double fTempo)
{
	MIDIPROPTEMPO	mpTempo = {sizeof(mpTempo), round(60000000.0 / fTempo)};
	CHECK(midiStreamProperty(m_hStrm, (BYTE *)&mpTempo, MIDIPROP_SET | MIDIPROP_TEMPO));
	return true;
}

void CSequencer::SetOutputDevice(UINT iOutputDevice)
{
	m_iOutputDevice = iOutputDevice;
}

bool CSequencer::GetPosition(LONGLONG& nTicks)
{
	if (IsOpen()) {	// if device open
		MMTIME	time;
		if (!GetPosition(time))
			return false;
		// convert ticks from unsigned 32-bit to 64-bit to allow negative values
		nTicks = static_cast<LONGLONG>(time.u.ticks) - m_nCBLen + m_nPosOffset;
	} else {	// device closed
		nTicks = m_nCBTime;	// decent approximation if latency is small
	}
	return true;
}

void CSequencer::SetPosition(int nTicks)
{
	if (m_bIsPlaying) {	// if playing
		WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
		m_nPosOffset += nTicks - m_nCBTime;
		m_nCBTime = nTicks;
		m_bIsPositionChange = true;	// signal position change
	} else {	// stopped
		m_nCBTime = nTicks;
		m_nStartPos = nTicks;
	}
}

void CSequencer::SetTempo(double fTempo)
{
	ASSERT(fTempo > 0);
	if (m_bIsPlaying) {	// if playing
		WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
		m_fTempo = fTempo;
		UpdateCallbackLength();
		m_bIsTempoChange = true;	// signal tempo change
	} else {	// stopped
		m_fTempo = fTempo;
	}
}

bool CSequencer::SetTimeDivision(int nTimeDiv)
{
	ASSERT(nTimeDiv > 0);
	ASSERT(!m_bIsPlaying);	// time division change during playback is unsupported
	if (m_bIsPlaying)
		return false;
	m_nTimeDiv = nTimeDiv;
	return true;
}

void CSequencer::SetLatency(int nLatency)
{
	ASSERT(nLatency > 0);
	m_nLatency = nLatency;
}

void CSequencer::SetBufferSize(int nEvents)
{
	ASSERT(nEvents > 1);
	m_nBufferSize = nEvents;
}

void CSequencer::SetTrackCount(int nTracks)
{
	WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
	int	nPrevTracks = GetTrackCount();
	m_arrTrack.SetSize(nTracks);
	if (nTracks > nPrevTracks) {	// if array grew
		for (int iTrack = nPrevTracks; iTrack < nTracks; iTrack++)
			m_arrTrack[iTrack].SetDefaults();
	}
}

void CSequencer::SetTrack(int iTrack, const CTrack& track)
{
	WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
	m_arrTrack[iTrack] = track;
}

void CSequencer::SetName(int iTrack, const CString& sName)
{
	m_arrTrack[iTrack].m_sName = sName;
}

void CSequencer::SetChannel(int iTrack, int nChannel)
{
	ASSERT(IsMidiChan(nChannel));
	m_arrTrack[iTrack].m_nChannel = nChannel;
}

void CSequencer::SetNote(int iTrack, int nNote)
{
	ASSERT(IsMidiParam(nNote));
	m_arrTrack[iTrack].m_nNote = nNote;
}

void CSequencer::SetQuant(int iTrack, int nQuant)
{
	ASSERT(nQuant > 0);
	m_arrTrack[iTrack].m_nQuant = nQuant;
}

void CSequencer::SetLength(int iTrack, int nLength)
{
	ASSERT(nLength > 0);
	WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
	m_arrTrack[iTrack].m_arrEvent.SetSize(nLength);
}

void CSequencer::SetOffset(int iTrack, int nOffset)
{
	m_arrTrack[iTrack].m_nOffset = nOffset;
}

void CSequencer::SetSwing(int iTrack, int nSwing)
{
	m_arrTrack[iTrack].m_nSwing = nSwing;
}

void CSequencer::SetVelocity(int iTrack, int nVelocity)
{
	m_arrTrack[iTrack].m_nVelocity = nVelocity;
}

void CSequencer::SetDuration(int iTrack, int nDuration)
{
	m_arrTrack[iTrack].m_nDuration = nDuration;
}

void CSequencer::SetMute(int iTrack, bool bMute)
{
	m_arrTrack[iTrack].m_bMute = bMute;
}

void CSequencer::SetEvent(int iTrack, int iEvent, BYTE nVal)
{
	m_arrTrack[iTrack].m_arrEvent[iEvent] = nVal;
}

void CSequencer::SetEvents(int iTrack, const CByteArrayEx& arrEvent)
{
	WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
	m_arrTrack[iTrack].m_arrEvent = arrEvent;
}

void CSequencer::GetTrackProperty(int iTrack, int iProp, CComVariant& var) const
{
	switch (iProp) {
	#define TRACKDEF(type, prefix, name, defval, offset) \
		case PROP_##name: var = Get##name(iTrack); break;
	#include "TrackDef.h"		// generate code to get track properties
	default:
		NODEFAULTCASE;
	}
}

void CSequencer::SetTrackProperty(int iTrack, int iProp, const CComVariant& var)
{
	switch (iProp) {
	#define TRACKDEF(type, prefix, name, defval, offset) \
		case PROP_##name: { type val; GetVariant(var, val); Set##name(iTrack, val); } break;
	#include "TrackDef.h"		// generate code to set track properties
	default:
		NODEFAULTCASE;
	}
}

void CSequencer::GetUsedTracks(CIntArrayEx& arrUsedTrack, bool bExcludeMuted) const
{
	arrUsedTrack.RemoveAll();
	int	nTracks = m_arrTrack.GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		if (!GetMute(iTrack) || !bExcludeMuted) {	// if track is unmuted, or we're including muted tracks
			if (m_arrTrack[iTrack].GetUsedEventCount())	// if track is non-empty
				arrUsedTrack.Add(iTrack);	// add track's index to used array
		}
	}
}

int CSequencer::GetUsedTrackCount(bool bExcludeMuted) const
{
	CIntArrayEx	arrUsedTrack;
	GetUsedTracks(arrUsedTrack, bExcludeMuted);
	return arrUsedTrack.GetSize();
}

int CSequencer::GetCallbackLength(int nLatency) const
{
	const int MIN_CALLBACK_LENGTH = 2;	// in ticks
	int	nTicks = trunc(nLatency / 1000.0 * m_fTempo / 60.0 * m_nTimeDiv) + 1;	// round up
	return max(nTicks, MIN_CALLBACK_LENGTH);	// helps to avoid glitches
}

void CSequencer::UpdateCallbackLength()
{
	m_nCBLen = GetCallbackLength(m_nLatency);
}

bool CSequencer::Play(bool bEnable)
{
	if (bEnable == m_bIsPlaying)	// if already in requested state
		return true;	// nothing to do
	if (bEnable) {	// if playing
		ZeroMemory(&m_stats, sizeof(m_stats));
		m_stats.fCBTimeMin = DBL_MAX;
		UpdateCallbackLength();
		m_nCBTime = m_nStartPos;
		m_nPosOffset = m_nStartPos;
		m_iBuffer = 0;
		m_bIsStopping = false;
		m_bIsTempoChange = false;
		m_bIsPositionChange = false;
		CHECK(midiStreamOpen(&m_hStrm, &m_iOutputDevice, 1, W64UINT(MidiOutProc), W64UINT(this), CALLBACK_FUNCTION));
		ZeroMemory(&m_arrMsgHdr, sizeof(m_arrMsgHdr));
		for (int iBuf = 0; iBuf < BUFFERS; iBuf++) {	// for each buffer
			MIDIHDR&	hdr = m_arrMsgHdr[iBuf];
			m_arrMidiEvent[iBuf].SetSize(m_nBufferSize);
			hdr.lpData = reinterpret_cast<char *>(m_arrMidiEvent[iBuf].GetData());
			hdr.dwBufferLength = m_nBufferSize * sizeof(MIDI_STREAM_EVENT);
			CHECK(midiOutPrepareHeader((HMIDIOUT)m_hStrm, &hdr, sizeof(MIDIHDR)));	// prepare buffer
		}
		CMidiEventStream&	arrEvt = m_arrMidiEvent[0];
		const int	nInitEvents = m_arrInitEvent.GetSize();
		for (int iEvent = 0; iEvent < nInitEvents; iEvent++) {
			arrEvt[iEvent].dwDeltaTime = 0;
			arrEvt[iEvent].dwEvent = m_arrInitEvent[iEvent];
		}
		int	nEvents = nInitEvents;
		arrEvt[nEvents].dwDeltaTime = m_nCBLen;
		arrEvt[nEvents].dwEvent = MEVT_NOP << 24;
		nEvents++;
#if SEQ_DUMP_EVENTS
		m_arrDumpEvent.RemoveAll();
		AddDumpEvent(m_arrMidiEvent[0], nEvents);
#endif	// SEQ_DUMP_EVENTS
		WriteTimeDivision(m_nTimeDiv);	// output time division
		WriteTempo(m_fTempo);	// output tempo
		m_arrMsgHdr[0].dwBytesRecorded = nEvents * sizeof(MIDI_STREAM_EVENT);
		CHECK(midiStreamOut(m_hStrm, &m_arrMsgHdr[0], sizeof(MIDIHDR)));	// output lead-in
		if (!OutputMidiBuffer())	// output first chunk of sequence in second buffer
			return false;
		CHECK(midiStreamRestart(m_hStrm));	// start playback
		m_bIsPlaying = true;	// set playing last
	} else {	// stopping
		m_bIsPlaying = false;	// clear playing first
		m_bIsStopping = true;	// signal callback to stop outputting events
		CHECK(midiStreamStop(m_hStrm));	// stop playback
		for (int iBuf = 0; iBuf < BUFFERS; iBuf++) {	// for each buffer, unprepare buffer
			CHECK(midiOutUnprepareHeader((HMIDIOUT)m_hStrm, &m_arrMsgHdr[iBuf], sizeof(MIDIHDR)));
		}
		CHECK(midiStreamClose(m_hStrm));	// close stream
		m_hStrm = NULL;
#if SEQ_DUMP_EVENTS
		DumpEvents(_T("seqdump.txt"));
#endif	// SEQ_DUMP_EVENTS
	}
	m_bIsPaused = false;
	return true;
}

bool CSequencer::Pause(bool bEnable)
{
	if (!m_bIsPlaying)	// if stopped
		return false;	// can't pause
	if (bEnable == m_bIsPaused)	// if already in requested state
		return true;	// nothing to do
	if (bEnable) {	// if pausing
		CHECK(midiStreamPause(m_hStrm));
	} else {	// continuing
		CHECK(midiStreamRestart(m_hStrm));
	}
	m_bIsPaused = bEnable;
	return true;
}

void CSequencer::Abort()
{
	m_bIsPlaying = false;
	m_bIsPaused = false;
	m_bIsStopping = true;
	midiStreamClose(m_hStrm);
	m_hStrm = NULL;
}

void CALLBACK CSequencer::MidiOutProc(HMIDIOUT hMidiOut, UINT wMsg, W64UINT dwInstance, W64UINT dwParam1, W64UINT dwParam2)
{
	UNREFERENCED_PARAMETER(dwParam1);
	UNREFERENCED_PARAMETER(dwParam2);
	UNREFERENCED_PARAMETER(hMidiOut);
	switch (wMsg) {
	case MOM_DONE:
		{
			CSequencer	*m_pThis = reinterpret_cast<CSequencer *>(dwInstance);
			if (!m_pThis->m_bIsStopping)
				m_pThis->OutputMidiBuffer();
		}
		break;
	case MOM_POSITIONCB:
		break;
	case MOM_OPEN:
		break;
	case MOM_CLOSE:
		break;
	}
}

__forceinline void CSequencer::AddTrackEvents(int iTrack, int nCBStart)
{
	CTrack&	trk = m_arrTrack[iTrack];
	int	nOffset = trk.m_nOffset;	// cache these values for thread safety
	int	nLength = trk.m_arrEvent.GetSize();
	int	nQuant = trk.m_nQuant;
	int	nSwing = trk.m_nSwing;
	nSwing = CLAMP(nSwing, 1 - nQuant, nQuant - 1);	// limit swing within quant
	int	nTrkStart = nCBStart - nOffset;
	int	nPairQuant = nQuant * 2;	// pair of quants, to handle swing
	int	iPairQuant = nTrkStart / nPairQuant;
	int	nEvtTime = nTrkStart % nPairQuant;
	if (nEvtTime < 0) {	// if negative event time
		iPairQuant--;	// compensate quant pair index
		nEvtTime += nPairQuant;	// wrap event time
	}
	int	iEvt = (iPairQuant * 2) % nLength;
	if (iEvt < 0)	// if negative event index
		iEvt += nLength;	// wrap event index
	nEvtTime = -nEvtTime;	// start far enough back to discard up to two events
	bool	bIsOdd = false;	// starting on even step, due to pair of quants logic
	while (nEvtTime < m_nCBLen) {	// while event time within callback period
		if (nEvtTime >= 0) {	// discard already played events
			if (trk.m_arrEvent[iEvt]) {
				CEvent	evt;
				evt.m_dwTime = nEvtTime;
				int	nVel = CLAMP(64 + trk.m_nVelocity, 0, 127);
				evt.m_dwEvent = MakeMidiMsg(NOTE_ON, trk.m_nChannel, trk.m_nNote, nVel);
				m_arrEvent.InsertSorted(evt);
				int	nDuration = nQuant + trk.m_nDuration;	// add duration offset
				if (bIsOdd)	// if odd step
					nDuration -= nSwing;	// subtract swing from duration
				else	// even step
					nDuration += nSwing;	// add swing to duration
				nDuration = max(nDuration, 1);	// keep duration above zero
				evt.m_dwTime = nCBStart + nEvtTime + nDuration;	// absolute time
				evt.m_dwEvent &= ~0xff0000;	// zero note's velocity
				m_arrNoteOff.InsertSorted(evt);	// add pending note off to array
			}
		}
		iEvt++;	// advance to next track event
		if (iEvt >= nLength)	// if track length reached
			iEvt -= nLength;	// wrap to start of track
		nEvtTime += nQuant;	// advance event time by track's time quant
		if (bIsOdd)	// if odd step
			nEvtTime -= nSwing;	// subtract swing from event time
		else	// even step
			nEvtTime += nSwing;	// add swing to event time
		bIsOdd ^= 1;	// toggle odd step flag
	}
}

__forceinline void CSequencer::AddNoteOffs(int nCBStart, int nCBEnd)
{
	int	nOffs = m_arrNoteOff.GetSize();
	while (nOffs > 0 && m_arrNoteOff[0].m_dwTime < nCBEnd) {
		m_arrNoteOff[0].m_dwTime -= nCBStart;	// make time relative to this callback
		ASSERT(m_arrNoteOff[0].m_dwTime >= 0);	// time must be positive
		m_arrEvent.InsertSorted(m_arrNoteOff[0]);
		m_arrNoteOff.RemoveAt(0);
		nOffs--;
	}
}

bool CSequencer::OutputMidiBuffer()
{
	CBenchmark b;	// for timing statistics only
	int	nCBStart, nCBEnd;
	{
		WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
		m_arrEvent.FastRemoveAll();
		// handle tempo change first to improve responsiveness
		if (m_bIsTempoChange) {	// if tempo changed
			m_bIsTempoChange = false;	// reset signal
			int	iTempo = round(60000000.0 / m_fTempo);
			CEvent	evtTempo(0, (MEVT_TEMPO << 24) | iTempo);	// at callback start time
			m_arrEvent.Add(evtTempo);	// add tempo event
		}
		// if position changed, turn off any active notes before adding new events
		if (m_bIsPositionChange) {	// if position changed
			m_bIsPositionChange = false;	// reset signal
			int	nEvents = m_arrNoteOff.GetSize();
			for (int iEvt = 0; iEvt < nEvents; iEvt++) {	// for each active note
				m_arrNoteOff[iEvt].m_dwTime = 0;	// at callback start time
				m_arrEvent.Add(m_arrNoteOff[iEvt]);	// add note off event
			}
			m_arrNoteOff.FastRemoveAll();	// empty note off array
		}
		DWORD	dwLiveEvent;
		while (m_qLiveEvent.Pop(dwLiveEvent)) {	// while live events remain to be played
			CEvent	evtLive(0, dwLiveEvent);	// at callback start time
			m_arrEvent.Add(evtLive);	// add live event
		}
		nCBStart = m_nCBTime;
		nCBEnd = nCBStart + m_nCBLen;
		m_nCBTime = nCBEnd;	// advance callback time by one period
		int	nTracks = m_arrTrack.GetSize();
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			if (!m_arrTrack[iTrack].m_bMute)	// if track isn't muted
				AddTrackEvents(iTrack, nCBStart);	// add events for this callback period
		}
	}	// leave callback critical section, releasing lock on callback state
	AddNoteOffs(nCBStart, nCBEnd);	// add note off events this callback period
	m_iBuffer ^= 1;	// swap playback buffers
	// fill MIDI output buffer with events created above
	CMidiEventStream&	arrEvt = m_arrMidiEvent[m_iBuffer];
	int	nEvents = m_arrEvent.GetSize();
	if (nEvents) {	// if any events to output
		CEvent	evt(m_nCBLen, MEVT_NOP << 24);
		m_arrEvent.Add(evt);	// pad time to start of next callback
		nEvents++;
		if (nEvents >= arrEvt.GetSize()) {	// if events exceed output buffer size
			OnMidiError(SEQERR_BUFFER_OVERRUN);
			return false;
		}
		// convert event times (relative to start of callback period) to delta times
		int	nPrevTime = 0;
		for (int iEvt = 0; iEvt < nEvents; iEvt++) {	// for each event
			const CEvent&	evt = m_arrEvent[iEvt];
			arrEvt[iEvt].dwDeltaTime = evt.m_dwTime - nPrevTime;	// convert to delta time
			arrEvt[iEvt].dwEvent = evt.m_dwEvent;
			nPrevTime = evt.m_dwTime;
		}
	} else {	// no events to output, so output NOP
		nEvents = 1;
		if (!arrEvt.GetSize()) {	// if events exceed buffer size
			OnMidiError(SEQERR_BUFFER_OVERRUN);
			return false;
		}
		arrEvt[0].dwDeltaTime = m_nCBLen;
		arrEvt[0].dwEvent = MEVT_NOP << 24;
	}
#if SEQ_DUMP_EVENTS
	AddDumpEvent(arrEvt, nEvents);
#endif	// SEQ_DUMP_EVENTS
	MIDIHDR&	hdr = m_arrMsgHdr[m_iBuffer];
	hdr.dwBytesRecorded = nEvents * sizeof(MIDI_STREAM_EVENT);
	CHECK(midiStreamOut(m_hStrm, &hdr, sizeof(MIDIHDR)));	// output MIDI buffer
	// update statistics only after this point
	double t = b.Elapsed();
	m_stats.nCallbacks++;
	if (t < m_stats.fCBTimeMin)
		m_stats.fCBTimeMin = t;
	if (t > m_stats.fCBTimeMax)
		m_stats.fCBTimeMax = t;
	m_stats.fCBTimeSum += t;
	if (t > m_nLatency / 1000.0)
		m_stats.nOverruns++;
	return true;
}

bool CSequencer::ExportImpl(LPCTSTR pszPath, int nDuration)
{
	// note that this method may throw CFileException (from CMidiFile)
	if (nDuration <= 0)	// if invalid duration (in seconds)
		return false;	// avoid divide by zero
	CIntArrayEx	arrUsedTrack;
	GetUsedTracks(arrUsedTrack, true);	// exclude muted tracks
	int	nUsedTracks = arrUsedTrack.GetSize();
	if (!nUsedTracks || nUsedTracks > USHRT_MAX)	// if no tracks, or too many tracks
		return false;
	CMidiFile	fMidi(pszPath, CFile::modeCreate | CFile::modeWrite);	// create MIDI file
	USHORT	uTracks = static_cast<USHORT>(nUsedTracks);
	USHORT	uTimeDiv = static_cast<USHORT>(m_nTimeDiv);
	fMidi.WriteHeader(uTracks, uTimeDiv, m_fTempo);	// write MIDI file header
	const int	nChunkDuration = 1000;	// desired chunk duration, in milliseconds
	int	nChunkLen = GetCallbackLength(nChunkDuration);	// convert chunk duration to ticks
	int	nChunks = (nDuration * 1000 - 1) / nChunkDuration + 1;	// number of chunks, rounded up
	m_nCBLen = nChunkLen;	// set callback length member; used in AddTrackEvents
	for (int iUsed = 0; iUsed < nUsedTracks; iUsed++) {	// for each non-empty track
		int	iTrack = arrUsedTrack[iUsed];	// get track index
		int	nCBTime = 0;
		int	nPadTime = 0;
		CMidiEventArray arrMidiEvt;
		for (int iChunk = 0; iChunk < nChunks; iChunk++) {	// for each time chunk
			m_arrEvent.FastRemoveAll();	// empty event array
			AddTrackEvents(iTrack, nCBTime);	// get track's events for this chunk
			AddNoteOffs(nCBTime, nCBTime + nChunkLen);	// add note offs for this chunk
			int	nEvents = m_arrEvent.GetSize();
			if (nEvents) {	// if any events to output
				// convert event times (relative to start of callback period) to delta times
				int	nPrevTime = -nPadTime;
				for (int iEvt = 0; iEvt < nEvents; iEvt++) {	// for each event
					const CEvent&	evt = m_arrEvent[iEvt];
					MIDI_EVENT	midiEvt;
					midiEvt.DeltaT = evt.m_dwTime - nPrevTime;	// convert to delta time
					midiEvt.Msg = evt.m_dwEvent;
					arrMidiEvt.Add(midiEvt);
					nPrevTime = evt.m_dwTime;
				}
				nPadTime = nChunkLen - m_arrEvent[nEvents - 1].m_dwTime;
			} else	// no events to output
				nPadTime += nChunkLen;
			nCBTime += nChunkLen;
		}
		fMidi.WriteTrack(arrMidiEvt);	// write track to MIDI file
	}
	return true;
}

bool CSequencer::Export(LPCTSTR pszPath, int nDuration)
{
	CSequencerReader	seq(*this);	// give export its own sequencer instance
	return seq.ExportImpl(pszPath, nDuration);
}

CSequencerReader::CSequencerReader(CSequencer& seq)
{
	// attach our track array to sequencer's track array; risky but OK so long as
	// our instance is destroyed before sequencer, and never modifies track array
	m_arrTrack.Attach(seq.m_arrTrack.GetData(), seq.m_arrTrack.GetSize());
	m_fTempo = seq.m_fTempo;
	m_nTimeDiv = seq.m_nTimeDiv;
}

CSequencerReader::~CSequencerReader()
{
	CTrack	*pTrack;
	W64INT	nSize;
	m_arrTrack.Detach(pTrack, nSize);	// detach our track array from sequencer's
}

bool CSequencer::OutputLiveEvent(DWORD dwEvent)
{
	ASSERT(IsOpen());	// device must be open
	if (!IsOpen())
		return false;
	return m_qLiveEvent.Push(dwEvent);	// fails if queue is full
}

void CSequencer::CopyTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack) const
{
	int	nSels = arrSelection.GetSize();
	arrTrack.SetSize(nSels);
	int	iTrack = 0;
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		arrTrack[iTrack] = m_arrTrack[arrSelection[iSel]];
		iTrack++;
	}
}

void CSequencer::InsertTracks(int iTrack, int nCount)
{
	WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
	CTrack	trk(true);	// initialize to defaults
	for (int iCopy = 0; iCopy < nCount; iCopy++)	// for each copy
		m_arrTrack.InsertAt(iTrack + iCopy, trk);
}

void CSequencer::InsertTrack(int iTrack, CTrack& track)
{
	WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
	m_arrTrack.InsertAt(iTrack, track);
}

void CSequencer::InsertTracks(int iTrack, CTrackArray& arrTrack)
{
	WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
	m_arrTrack.InsertAt(iTrack, &arrTrack);
}

void CSequencer::InsertTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack)
{
	WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
	// assume selection array's track indices are in ascending order
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selected track
		m_arrTrack.InsertAt(arrSelection[iSel], arrTrack[iSel]);
}

void CSequencer::DeleteTracks(int iTrack, int nCount)
{
	WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
	m_arrTrack.RemoveAt(iTrack, nCount);
}

void CSequencer::DeleteTracks(const CIntArrayEx& arrSelection)
{
	WCritSec::Lock	lock(m_csCallback);	// serialize access to callback shared state
	// assume selection array's track indices are in ascending order
	int	nSels = arrSelection.GetSize();	// reverse iterate for deletion stability
	for (int iSel = nSels - 1; iSel >= 0; iSel--)	// for each selected track
		m_arrTrack.RemoveAt(arrSelection[iSel]);
}

void CSequencer::ConvertPositionToBeat(LONGLONG nPos, LONGLONG& nBeat, LONGLONG& nTick) const
{
	nBeat = nPos / m_nTimeDiv;
	nTick = nPos % m_nTimeDiv;
	if (nTick < 0) {	// if negative tick
		nBeat--;	// compensate beat
		nTick += m_nTimeDiv;	// wrap tick to make it positive
	}
}

void CSequencer::ConvertBeatToPosition(LONGLONG nBeat, LONGLONG nTick, LONGLONG& nPos) const
{
	nPos = nBeat * m_nTimeDiv + nTick;
}

void CSequencer::ConvertPositionToString(const LONGLONG& nPos, CString& sPos) const
{
	LONGLONG	nBeat, nTick;
	ConvertPositionToBeat(nPos, nBeat, nTick);
	sPos.Format(_T("%lld:%03lld"), nBeat + 1, nTick);	// convert beat from zero-origin to one-origin
}

bool CSequencer::ConvertStringToPosition(const CString& sPosition, LONGLONG& nPos) const
{
	LONGLONG	nBeat, nTick = 0;	// tick is optional, so initialize it
	if (_stscanf_s(sPosition, _T("%lld:%lld"), &nBeat, &nTick) < 1)	// if no fields converted
		return false;
	ConvertBeatToPosition(nBeat - 1, nTick, nPos);	// convert beat from one-origin to zero-origin
	return true;
}

#if SEQ_DUMP_EVENTS

void CSequencer::AddDumpEvent(const CMidiEventStream& arrEvt, int nEvents)
{
	int	iEvt = m_arrDumpEvent.GetSize();
	m_arrDumpEvent.SetSize(iEvt + 1);
	m_arrDumpEvent[iEvt].SetSize(nEvents);
	memcpy(m_arrDumpEvent[iEvt].GetData(), arrEvt.GetData(), nEvents * sizeof(MIDI_STREAM_EVENT));
}

void CSequencer::DumpEvents(LPCTSTR pszPath)
{
	BYTE	bNote[MIDI_CHANNELS][MIDI_NOTES];
	ZeroMemory(bNote, sizeof(bNote));
	CStdioFile	fOut(pszPath, CFile::modeWrite | CFile::modeCreate);
	int	nCallbacks = m_arrDumpEvent.GetSize();
	CString	sLine;
	int	nTime = -m_nCBLen;	// assume callback length constant during playback
	int	nErrors = 0;
	for (int iCallback = 0; iCallback < nCallbacks; iCallback++) {
		const CMidiEventStream&	arrEvt = m_arrDumpEvent[iCallback];
		sLine.Format(_T("%d: %d\n"), iCallback, nTime);
		fOut.WriteString(sLine);
		int	nEvents = arrEvt.GetSize();
		int	nSum = 0;
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {
			const MIDI_STREAM_EVENT& evt = arrEvt[iEvent];
			sLine.Format(_T("\t%d\t%x\n"), evt.dwDeltaTime, evt.dwEvent);
			fOut.WriteString(sLine);
			if (MIDI_CMD(evt.dwEvent) == NOTE_ON) {
				int	iChan = MIDI_CHAN(evt.dwEvent);
				int	iNote = MIDI_P1(evt.dwEvent);
				BYTE	nVel = MIDI_P2(evt.dwEvent);
				bool	bIsNoteOn = nVel != 0;
				bool	bIsPrevNoteOn = bNote[iChan][iNote] != 0;
				if (!(bIsNoteOn ^ bIsPrevNoteOn)) {
					fOut.WriteString(_T("ERROR collision\n"));
					nErrors++;
				}
				bNote[iChan][iNote] = nVel;
			}
			nSum += evt.dwDeltaTime;
		}
		if (nSum != m_nCBLen) {
			sLine.Format(_T("ERROR %d != %d\n"), nSum, m_nCBLen);
			fOut.WriteString(sLine);
			nErrors++;
		}
		nTime += m_nCBLen;
	}
	if (nErrors)
		printf("ERRORS!\n");
}
#endif
