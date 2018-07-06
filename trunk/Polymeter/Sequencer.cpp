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
#include "float.h"
#include "VariantHelper.h"
#include "MidiFile.h"
#include "Midi.h"

#define CHECK(x) { MMRESULT nResult = x; if (MIDI_FAILED(nResult)) { OnMidiError(nResult); return false; } }

CSequencer::CSequencer()
{
	m_hStrm = 0;
	ZeroMemory(&m_arrMsgHdr, sizeof(m_arrMsgHdr));
	m_fTempo = 120;
	m_iOutputDevice = 0;
	m_nTimeDiv = 120;
	m_nMeter = 1;
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
	m_bIsRecording = false;
	m_bIsTempoChange = false;
	m_bIsPositionChange = false;
	m_bIsSongMode = false;
	ZeroMemory(&m_stats, sizeof(m_stats));
	m_qLiveEvent.Create(DEF_BUFFER_SIZE);
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

void CSequencer::SetOutputDevice(int iOutputDevice)
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
		WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
		m_nPosOffset += nTicks - m_nCBTime;
		m_nCBTime = nTicks;
		m_bIsPositionChange = true;	// signal position change
		if (m_bIsSongMode)	// if song playback
			ChaseDubs(nTicks);	// reset dub indices
	} else {	// stopped
		m_nCBTime = nTicks;
		m_nStartPos = nTicks;
		if (m_bIsSongMode)	// if song playback
			ChaseDubs(nTicks, true);	// reset dub indices and update mutes
	}
}

void CSequencer::SetTempo(double fTempo)
{
	ASSERT(fTempo > 0);
	if (m_bIsPlaying) {	// if playing
		WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
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

bool CSequencer::Play(bool bEnable, bool bRecord)
{
	if (bEnable == m_bIsPlaying)	// if already in requested state
		return true;	// nothing to do
	if (bEnable) {	// if playing
		if (bRecord)	// if recording
			OnRecordStart();
		ZeroMemory(&m_stats, sizeof(m_stats));
		m_stats.fCBTimeMin = DBL_MAX;
		UpdateCallbackLength();
		ResetCachedParameters();
		m_nCBTime = m_nStartPos;
		m_nPosOffset = m_nStartPos;
		m_iBuffer = 0;
		m_bIsStopping = false;
		m_bIsTempoChange = false;
		m_bIsPositionChange = false;
		m_arrEvent.SetSize(DEF_BUFFER_SIZE);	// preallocate event array
		m_arrEvent.FastRemoveAll();
		m_arrNoteOff.SetSize(DEF_BUFFER_SIZE);	// preallocate note off array
		m_arrNoteOff.FastRemoveAll();
		if (m_bIsSongMode)
			ChaseDubs(m_nStartPos, true);
		UINT	uDevice = m_iOutputDevice;
		CHECK(midiStreamOpen(&m_hStrm, &uDevice, 1, reinterpret_cast<W64UINT>(MidiOutProc), 
			reinterpret_cast<W64UINT>(this), CALLBACK_FUNCTION));
		ZeroMemory(&m_arrMsgHdr, sizeof(m_arrMsgHdr));
		for (int iBuf = 0; iBuf < BUFFERS; iBuf++) {	// for each buffer
			MIDIHDR&	hdr = m_arrMsgHdr[iBuf];
			m_arrMidiEvent[iBuf].SetSize(m_nBufferSize);
			hdr.lpData = reinterpret_cast<char *>(m_arrMidiEvent[iBuf].GetData());
			hdr.dwBufferLength = m_nBufferSize * sizeof(MIDI_STREAM_EVENT);
			CHECK(midiOutPrepareHeader(reinterpret_cast<HMIDIOUT>(m_hStrm), &hdr, sizeof(MIDIHDR)));	// prepare buffer
		}
		CMidiEventStream&	arrEvt = m_arrMidiEvent[0];
		const int	nInitEvents = m_arrInitMidiEvent.GetSize();
		for (int iEvent = 0; iEvent < nInitEvents; iEvent++) {
			arrEvt[iEvent].dwDeltaTime = 0;
			arrEvt[iEvent].dwEvent = m_arrInitMidiEvent[iEvent];
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
		m_bIsRecording = bRecord;	// set recording flag before starting playback
		CHECK(midiStreamRestart(m_hStrm));	// start playback
		m_bIsPlaying = true;	// set playing flag last
	} else {	// stopping
		m_bIsPlaying = false;	// clear playing flag first
		m_bIsStopping = true;	// signal callback to stop outputting events
		bRecord = m_bIsRecording;	// save recording flag
		m_bIsRecording = false;	// clear recording flag
		LONGLONG	nEndTime = 0;
		if (bRecord)	// if recording
			GetPosition(nEndTime);	// get end of song position
		CHECK(midiStreamStop(m_hStrm));	// stop playback
		for (int iBuf = 0; iBuf < BUFFERS; iBuf++) {	// for each buffer, unprepare buffer
			CHECK(midiOutUnprepareHeader(reinterpret_cast<HMIDIOUT>(m_hStrm), &m_arrMsgHdr[iBuf], sizeof(MIDIHDR)));
		}
		CHECK(midiStreamClose(m_hStrm));	// close stream
		m_hStrm = NULL;
#if SEQ_DUMP_EVENTS
		DumpEvents(_T("seqdump.txt"));
#endif	// SEQ_DUMP_EVENTS
		if (bRecord)	// if recording
			OnRecordStop(static_cast<int>(nEndTime));
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
	m_bIsRecording = false;
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

__forceinline int CSequencer::GetNoteDuration(const CStepArray& arrStep, int nSteps, int iCurStep) const
{
	int	iPrevStep = iCurStep - 1;	// previous step
	if (iPrevStep < 0)	// if underrun
		iPrevStep = nSteps - 1;	// wrap to last step
	if (arrStep[iPrevStep] & SB_TIE)	// if previous step is tied
		return 0;	// failure; not at start of note
	if (!(arrStep[iCurStep] & SB_TIE))	// if current step isn't tied
		return 1;	// duration is one; optimized case
	// current step is tied; find first step that isn't tied
	for (int nDur = 1; nDur < nSteps; nDur++) {	// for remaining steps
		iCurStep++;	// next step
		if (iCurStep >= nSteps)	// if overrun
			iCurStep = 0;	// wrap to first step
		if (!(arrStep[iCurStep] & SB_TIE)) {	// if step isn't tied
			if (arrStep[iCurStep])	// if step is on
				return nDur + 1;	// duration includes this step
			else	// step is off
				return nDur;	// duration excludes this step
		}
	}
	return nSteps;	// degenerate case
}

__forceinline void CSequencer::CEvent::Create(const CTrack& trk, DWORD dwTime, int nVal)
{
	m_dwTime = dwTime;
	switch (trk.m_iType) {
	case TT_NOTE:
		m_dwEvent = MakeMidiMsg(NOTE_ON, trk.m_nChannel, trk.m_nNote, nVal);
		break;
	case TT_KEY_AFT:
		m_dwEvent = MakeMidiMsg(KEY_AFT, trk.m_nChannel, trk.m_nNote, nVal);
		break;
	case TT_CONTROL:
		m_dwEvent = MakeMidiMsg(CONTROL, trk.m_nChannel, trk.m_nNote, nVal);
		break;
	case TT_PATCH:
		m_dwEvent = MakeMidiMsg(PATCH, trk.m_nChannel, nVal, 0);
		break;
	case TT_CHAN_AFT:
		m_dwEvent = MakeMidiMsg(CHAN_AFT, trk.m_nChannel, nVal, 0);
		break;
	case TT_WHEEL:
		m_dwEvent = MakeMidiMsg(WHEEL, trk.m_nChannel, trk.m_nNote, nVal);
		break;
	default:
		NODEFAULTCASE;
	}
}

__forceinline void CSequencer::AddTrackEvents(int iTrack, int nCBStart)
{
	CTrack&	trk = GetAt(iTrack);
	int	nOffset = trk.m_nOffset;	// cache these values for thread safety
	int	nLength = trk.m_arrStep.GetSize();
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
	int	iStep = (iPairQuant * 2) % nLength;
	if (iStep < 0)	// if negative step index
		iStep += nLength;	// wrap step index
	nEvtTime = -nEvtTime;	// start far enough back to discard up to two events
	bool	bIsOdd = false;	// starting on even step, due to pair of quants logic
	int	nTracks = GetTrackCount();
	while (nEvtTime < m_nCBLen) {	// while event time within callback period
		if (nEvtTime >= 0) {	// discard already played events
			if (m_bIsSongMode) {	// if applying track dubs
				CTrack&	trk = GetAt(iTrack);
				int	nDubs = trk.m_arrDub.GetSize();
				while (trk.m_iDub < nDubs && nCBStart + nEvtTime >= trk.m_arrDub[trk.m_iDub].m_nTime) {
					trk.m_bMute = trk.m_arrDub[trk.m_iDub].m_bMute;
					trk.m_iDub++;
				}
			}
			int	arrMod[MODULATION_TYPES];
			for (int iModType = 0; iModType < MODULATION_TYPES; iModType++) {	// for each modulation type
				int	iModTrack = trk.m_arrModulator[iModType];
				if (iModTrack >= 0 && iModTrack < nTracks && !GetMute(iModTrack)) {	// if track is modulated
					int	iModStep = GetStepIndex(iModTrack, nCBStart + nEvtTime);
					int	nStepVal = GetTrack(iModTrack).m_arrStep[iModStep] & SB_VELOCITY;
					if (iModType != MT_Mute)	// if modulation isn't mute
						nStepVal = nStepVal - MIDI_NOTES / 2;	// convert step value to signed offset
					arrMod[iModType] = nStepVal;
				} else	// track doesn't have this type of modulation
					arrMod[iModType] = 0;
			}
			if (!trk.m_bMute && !arrMod[MT_Mute]) {	// if track isn't muted
				if (trk.m_iType == TT_NOTE) {	// if track type is note
					if (trk.m_arrStep[iStep]) {	// if step isn't empty
						int	nDurSteps = GetNoteDuration(trk.m_arrStep, nLength, iStep);
						if (nDurSteps) {	// if at start of note
							CEvent	evt;
							evt.m_dwTime = nEvtTime;
							int	nVel = (trk.m_arrStep[iStep] & SB_VELOCITY) + trk.m_nVelocity + arrMod[MT_Velocity];
							nVel = CLAMP(nVel, 0, MIDI_NOTE_MAX);
							int	nNote = trk.m_nNote + arrMod[MT_Note];
							nNote = CLAMP(nNote, 0, MIDI_NOTE_MAX);
							evt.m_dwEvent = MakeMidiMsg(NOTE_ON, trk.m_nChannel, nNote, nVel);
							m_arrEvent.InsertSorted(evt);	// add note to sorted array for output
							int	nDuration = nDurSteps * nQuant + trk.m_nDuration + arrMod[MT_Duration];	// add duration offset
							if (nDurSteps & 1) {	// if odd duration
								if (bIsOdd)	// if odd step
									nDuration -= nSwing;	// subtract swing from duration
								else	// even step
									nDuration += nSwing;	// add swing to duration
							}
							nDuration = max(nDuration, 1);	// keep duration above zero
							evt.m_dwTime = nCBStart + nEvtTime + nDuration;	// absolute time
							evt.m_dwEvent &= ~0xff0000;	// zero note's velocity
							m_arrNoteOff.InsertSorted(evt);	// add pending note off to array
						}
					}
				} else if (trk.m_iType != TT_MODULATOR) {	// track type isn't note or modulator
					int	nVal = (trk.m_arrStep[iStep] & SB_VELOCITY) + trk.m_nVelocity + arrMod[MT_Velocity];
					nVal = CLAMP(nVal, 0, MIDI_NOTE_MAX);
					if (nVal != trk.m_nCachedParam) {	// if value differs from cached parameter
 						trk.m_nCachedParam = nVal;	// update cached parameter
						CEvent	evt;
						evt.Create(trk, nEvtTime, nVal);
						m_arrEvent.InsertSorted(evt);	// add note to sorted array for output
					}
				}
			}
		}
		iStep++;	// advance to next track step
		if (iStep >= nLength)	// if track length reached
			iStep -= nLength;	// wrap to start of track
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
		WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
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
			for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each active note
				m_arrNoteOff[iEvent].m_dwTime = 0;	// at callback start time
				m_arrEvent.Add(m_arrNoteOff[iEvent]);	// add note off event
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
		int	nTracks = GetSize();
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			bool	bIsDubbing = false;
			if (m_bIsSongMode) {	// if applying track dubs
				const CTrack&	trk = GetAt(iTrack);
				if (trk.m_iDub < trk.m_arrDub.GetSize()) {	// if unplayed dubs remain
					if (trk.m_arrDub[trk.m_iDub].m_nTime < nCBEnd)	// if dubs during this callback
						bIsDubbing = true;	// call AddTrackEvents even if track is muted
				}
			}
			if (!GetAt(iTrack).m_bMute || bIsDubbing)	// if track isn't muted, or track is dubbing
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
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each event
			const CEvent&	evt = m_arrEvent[iEvent];
			arrEvt[iEvent].dwDeltaTime = evt.m_dwTime - nPrevTime;	// convert to delta time
			arrEvt[iEvent].dwEvent = evt.m_dwEvent;
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
	if (m_bIsSongMode)
		ChaseDubs(0, true);	// reset dub indices and update mutes
	// note that this method may throw CFileException (from CMidiFile)
	if (nDuration <= 0)	// if invalid duration (in seconds)
		return false;	// avoid divide by zero
	CIntArrayEx	arrUsedTrack;
	GetUsedTracks(arrUsedTrack, !m_bIsSongMode);	// in track mode, exclude muted tracks
	int	nUsedTracks = arrUsedTrack.GetSize();
	// max tracks is one less to allow for initialization track
	if (!nUsedTracks || nUsedTracks > USHRT_MAX - 1)	// if no tracks, or too many tracks
		return false;
	CMidiFile	fMidi(pszPath, CFile::modeCreate | CFile::modeWrite);	// create MIDI file
	USHORT	uTracks = static_cast<USHORT>(nUsedTracks + 1);	// account for initialization track
	USHORT	uTimeDiv = static_cast<USHORT>(m_nTimeDiv);
	fMidi.WriteHeader(uTracks, uTimeDiv, m_fTempo);	// write MIDI file header
	const int	nChunkDuration = 1000;	// desired chunk duration, in milliseconds
	int	nChunkLen = GetCallbackLength(nChunkDuration);	// convert chunk duration to ticks
	int	nChunks = (nDuration * 1000 - 1) / nChunkDuration + 1;	// number of chunks, rounded up
	m_nCBLen = nChunkLen;	// set callback length member; used in AddTrackEvents
	{	// output initialization track
		CMidiEventArray arrMidiEvent;
		int	nInitEvents = m_arrInitMidiEvent.GetSize();
		arrMidiEvent.SetSize(nInitEvents + 1);
		for (int iEvt = 0; iEvt < nInitEvents; iEvt++) {	// for each initial event
			MIDI_EVENT	evt = {0, m_arrInitMidiEvent[iEvt]};
			arrMidiEvent[iEvt] = evt;
		}
		int	nDurationTicks = static_cast<int>(ConvertSecondsToPosition(nDuration));
		MIDI_EVENT	evt = {nDurationTicks, KEY_AFT};	// dummy event to pad track out to duration
		arrMidiEvent[nInitEvents] = evt;
		fMidi.WriteTrack(arrMidiEvent);	// write initialization track
	}
	for (int iUsed = 0; iUsed < nUsedTracks; iUsed++) {	// for each non-empty track
		int	iTrack = arrUsedTrack[iUsed];	// get track index
		int	nCBTime = 0;
		int	nPadTime = 0;
		CMidiEventArray arrMidiEvent;
		for (int iChunk = 0; iChunk < nChunks; iChunk++) {	// for each time chunk
			m_arrEvent.FastRemoveAll();	// empty event array
			AddTrackEvents(iTrack, nCBTime);	// get track's events for this chunk
			AddNoteOffs(nCBTime, nCBTime + nChunkLen);	// add note offs for this chunk
			int	nEvents = m_arrEvent.GetSize();
			if (nEvents) {	// if any events to output
				// convert event times (relative to start of callback period) to delta times
				int	nPrevTime = -nPadTime;
				for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each event
					const CEvent&	evt = m_arrEvent[iEvent];
					MIDI_EVENT	midiEvt;
					midiEvt.DeltaT = evt.m_dwTime - nPrevTime;	// convert to delta time
					midiEvt.Msg = evt.m_dwEvent;
					arrMidiEvent.Add(midiEvt);
					nPrevTime = evt.m_dwTime;
				}
				nPadTime = nChunkLen - m_arrEvent[nEvents - 1].m_dwTime;
			} else	// no events to output
				nPadTime += nChunkLen;
			nCBTime += nChunkLen;
		}
		fMidi.WriteTrack(arrMidiEvent);	// write track to MIDI file
	}
	return true;
}

bool CSequencer::Export(LPCTSTR pszPath, int nDuration, bool bSongMode)
{
	CSequencerReader	seq(*this);	// give export its own sequencer instance
	seq.SetSongMode(bSongMode);
	return seq.ExportImpl(pszPath, nDuration);
}

CSequencerReader::CSequencerReader(CSequencer& seq)
{
	// attach our track array to sequencer's track array; risky but OK so long as
	// our instance is destroyed before sequencer, and never modifies track array
	Attach(seq.GetData(), seq.GetSize());
	// copy any variables used by export
	m_fTempo = seq.m_fTempo;
	m_nTimeDiv = seq.m_nTimeDiv;
	m_bIsSongMode = seq.m_bIsSongMode;
	m_arrInitMidiEvent = seq.m_arrInitMidiEvent;
}

CSequencerReader::~CSequencerReader()
{
	CTrack	*pTrack;
	W64INT	nSize;
	Detach(pTrack, nSize);	// detach our track array from sequencer's
}

bool CSequencer::OutputLiveEvent(DWORD dwEvent)
{
	ASSERT(IsOpen());	// device must be open
	if (!IsOpen())
		return false;
	return m_qLiveEvent.Push(dwEvent);	// fails if queue is full
}

void CSequencer::ConvertPositionToBeat(LONGLONG nPos, LONGLONG& nMeasure, LONGLONG& nBeat, LONGLONG& nTick) const
{
	nBeat = nPos / m_nTimeDiv;
	nTick = nPos % m_nTimeDiv;
	if (nTick < 0) {	// if negative tick
		nBeat--;	// compensate beat
		nTick += m_nTimeDiv;	// wrap tick to make it positive
	}
	if (m_nMeter > 1) {	// if valid meter
		nMeasure = nBeat / m_nMeter;
		nBeat = nBeat % m_nMeter;
		if (nBeat < 0) {	// if negative beat
			nMeasure--;	// compensate measure
			nBeat += m_nMeter;	// wrap beat to make it positive
		}
	} else	// measure doesn't apply
		nMeasure = -1;
}

void CSequencer::ConvertBeatToPosition(LONGLONG nMeasure, LONGLONG nBeat, LONGLONG nTick, LONGLONG& nPos) const
{
	if (m_nMeter > 1)	// if valid meter
		nPos = (nMeasure * m_nMeter + nBeat) * m_nTimeDiv + nTick;
	else	// measure doesn't apply
		nPos = nBeat * m_nTimeDiv + nTick;
}

void CSequencer::ConvertPositionToString(LONGLONG nPos, CString& sPos) const
{
	LONGLONG	nMeasure, nBeat, nTick;
	ConvertPositionToBeat(nPos, nMeasure, nBeat, nTick);
	// convert measure and beat from zero-origin to one-origin
	if (m_nMeter > 1)	// if valid meter
		sPos.Format(_T("%lld:%lld:%03lld"), nMeasure + 1, nBeat + 1, nTick);
	else	// measure doesn't apply
		sPos.Format(_T("%lld:%03lld"), nBeat + 1, nTick);
}

bool CSequencer::ConvertStringToPosition(const CString& sPosition, LONGLONG& nPos) const
{
	LONGLONG	nFld[3];
	int	nFields = _stscanf_s(sPosition, _T("%lld:%lld:%lld"), &nFld[0], &nFld[1], &nFld[2]);
	LONGLONG	nMeasure, nBeat, nTick;
	if (m_nMeter > 1) {	// if valid meter
		switch (nFields) {
		case 1:	// one field converted
			nMeasure = nFld[0];	// format is measure
			nBeat = 1;
			nTick = 0;
			break;
		case 2:	// two fields converted
			nMeasure = nFld[0];	// format is measure:beat
			nBeat = nFld[1];
			nTick = 0;
			break;
		case 3:	// three fields converted
			nMeasure = nFld[0];	// format is measure:beat:tick
			nBeat = nFld[1];
			nTick = nFld[2];
			break;
		default:
			return false;	// no fields converted
		}
	} else {	// invalid meter
		switch (nFields) {
		case 1:	// one field converted
			nMeasure = 1;
			nBeat = nFld[0];	// format is beat
			nTick = 0;
			break;
		case 2:	// two fields converted
			nMeasure = 1;
			nBeat = nFld[0];	// format is beat:tick
			nTick = nFld[1];
			break;
		case 3:	// three fields converted
			nMeasure = 1;	// could return error, but presume 4/4 instead
			nBeat = nFld[0] * 4 + nFld[1];	// format is measure:beat:tick
			nTick = nFld[2];
			break;
		default:
			return false;	// no fields converted
		}
	}
	// convert measure and beat from one-origin to zero-origin
	ConvertBeatToPosition(nMeasure - 1, nBeat - 1, nTick, nPos);
	return true;
}

LONGLONG CSequencer::ConvertPositionToSeconds(LONGLONG nPos) const
{
	return round64(static_cast<double>(nPos) / m_nTimeDiv / m_fTempo * 60);
}

LONGLONG CSequencer::ConvertSecondsToPosition(LONGLONG nSecs) const
{
	return round64(static_cast<double>(nSecs) / 60 * m_fTempo * m_nTimeDiv);
}

void CSequencer::ConvertPositionToTimeString(LONGLONG nPos, CString& sTime) const
{
	LONGLONG	nTime = ConvertPositionToSeconds(nPos);
	LPCTSTR	pszFormat;
	if (nTime < 0) {	// if negative time
		pszFormat = _T("-%lld:%02lld:%02lld");
		nTime = -nTime;
	} else
		pszFormat = _T("%lld:%02lld:%02lld");
	LONGLONG	nSecs = nTime % 60;
	nTime /= 60;
	LONGLONG	nMins = nTime % 60;
	nTime /= 60;
	LONGLONG	nHours = nTime;
	sTime.Format(pszFormat, nHours, nMins, nSecs);
}

int CSequencer::GetSongDurationSeconds() const
{
	int	nSongTicks = GetSongDuration();
	return static_cast<int>(ConvertPositionToSeconds(nSongTicks)) + 1;	// round up
}

int CSequencer::GetStepIndex(int iTrack, LONGLONG nPos) const
{
	const CTrack&	trk = GetAt(iTrack);
	int	nLength = trk.m_arrStep.GetSize();
	int	nQuant = trk.m_nQuant;
	int	nOffset = trk.m_nOffset;
	// similar to AddEvents, but divide by nQuant instead nQuant * 2
	int	nTrkStart = static_cast<int>(nPos) - nOffset;
	int	iQuant = nTrkStart / nQuant;
	int	nEvtTime = nTrkStart % nQuant;
	if (nEvtTime < 0) {
		iQuant--;
		nEvtTime += nQuant;
	}
	int	iStep = iQuant % nLength;
	if (iStep < 0)
		iStep += nLength;
	return iStep;
}

void CSequencer::RecordDub(int iTrack)
{
	LONGLONG	nPos;
	if (GetPosition(nPos))
		AddDub(iTrack, static_cast<int>(nPos));
}

void CSequencer::RecordDub(const CIntArrayEx& arrSelection)
{
	LONGLONG	nPos;
	if (GetPosition(nPos))
		AddDub(arrSelection, static_cast<int>(nPos));
}

void CSequencer::RecordDub()
{
	LONGLONG	nPos;
	if (GetPosition(nPos))
		AddDub(static_cast<int>(nPos));
}

void CSequencer::ChaseDubsFromCurPos()
{
	if (m_bIsPlaying && m_bIsSongMode) {
		LONGLONG	nPos;
		if (GetPosition(nPos))
			ChaseDubs(static_cast<int>(nPos), true);	// set mutes
	}
}

#if SEQ_DUMP_EVENTS

void CSequencer::AddDumpEvent(const CMidiEventStream& arrEvt, int nEvents)
{
	int	iEvent = m_arrDumpEvent.GetSize();
	m_arrDumpEvent.SetSize(iEvent + 1);
	m_arrDumpEvent[iEvent].SetSize(nEvents);
	memcpy(m_arrDumpEvent[iEvent].GetData(), arrEvt.GetData(), nEvents * sizeof(MIDI_STREAM_EVENT));
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
