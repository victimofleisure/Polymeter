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
		03		07dec18	set initial dub time to smallest int instead of zero
		04		07dec18	in track export, don't export modulator tracks
		05		07dec18	in export implementation, reset cached parameters
		06		11dec18	add note range and range modulation
		07		14dec18	add position modulation
		08		17dec18	move MIDI file types into class scope
		09		03jan19	add MIDI output capture
		10		12jan19	add recursive position modulation
		11		17jan19	handle recursive mute with continue instead of abort
		12		06feb19	in export, chase dubs to start position instead of minimum value
		13		14feb19	in export, handle track and song modes similarly
		14		20feb19	optionally prevent note overlaps
		15		26feb19	use fast insert for event arrays
		16		09sep19	add tempo track type and tempo modulation
		17		17feb20	move MIDI event class into track base
		18		29feb20	output live events directly to stream handle
		19		29feb20	add support for recording live events
		20		16mar20	add scale, index and voicing modulation
		21		20mar20	add mapping
		22		30mar20	fix index modulation; sort scale ascending
		23		03apr20	add milliseconds to time format
		24		04apr20	add chord modulation
		25		06apr20	add callback too long error
		26		11apr20	replace some array calls with fast versions
		27		14apr20	add send MIDI clock option
		28		16apr20	apply range to scale after picking chord tones
		29		16apr20	in recursion methods, compute step index only if needed
		30		18apr20	optimize add track events
		31		19may20	refactor record dub methods to include conditional
		32		23may20	negative voicing modulation raises instead of dropping
		33		09jun20	add offset modulation
		34		17jul20	set song mode now optionally chases dubs
		35		16dec20	add looping of playback
		36		07jun21	rename rounding functions
		37		08jun21	fix local name reuse warning
		38		30oct21	remove song duration method
		39		03jan22	for zero-velocity notes, suppress duplicate note off
		40		21jan22	add per-channel note overlap methods
		41		21jan22	fix tempo modulation causing position slippage
		42		05feb22 in SumModulations, step value was missing velocity mask
		43		13feb22	support offset modulation of modulator tracks
		44		19may22	set position offset in SetPosition stopped case
		45		20oct22	support offset modulation of controller tracks
		46		12nov22	flip sign in recursive offset modulation case
		47		27nov23	include time signature and key signature in Export
		48		19dec23	add internal track type and controllers
		49		09jan24	add base class to streamline reader init
		50		24jan24	add warning error attribute
		51		10feb24	export now turns off all notes at end of file
		52		02may24	optimize swing; replace boolean with sign flip
		53		02may24	replace redundant track index with reference
		54		01sep24	add per-channel duplicate note methods

*/

#include "stdafx.h"
#include "Sequencer.h"
#include "float.h"
#include "VariantHelper.h"
#include "MidiFile.h"
#include "Midi.h"
#include <math.h>

#define CHECK(x) { MMRESULT nResult = x; if (MIDI_FAILED(nResult)) { OnMidiError(nResult); return false; } }
#define MAKE_CHANNEL_MASK(iType) (static_cast<USHORT>(1) << iType)

bool CSequencer::m_bExportTimeKeySigs = true;
bool CSequencer::m_bExportAllNotesOff = true;

CSequencer::CSequencer()
{
	m_fTempo = SEQ_INIT_TEMPO;
	m_fTempoScaling = 1;
	m_fLatencySecs = 0;
	m_nAltTempo = 0;
	m_iOutputDevice = 0;
	m_nTimeDiv = SEQ_INIT_TIME_DIV;
	m_nMeter = 1;
	m_nCBTime = 0;
	m_nCBLen = 0;
	m_nLatency = 50;
	m_nBufferSize = DEF_BUFFER_SIZE;
	m_iBuffer = 0;
	m_nStartPos = 0;
	m_nPosOffset = 0;
	m_nRecursions = 0;
	m_iRecordEvent = 0;
	m_nRecordOffset = 0;
	m_nMidiClockPeriod = 0;
	m_nMidiClockTimer = 0;
	m_bIsPlaying = false;
	m_bIsPaused = false;
	m_bIsStopping = false;
	m_bIsRecording = false;
	m_bIsTempoChange = false;
	m_bIsPositionChange = false;
	m_bIsSongMode = false;
	m_bIsOutputCapture = false;
	m_bPreventNoteOverlap = false;
	m_bIsSendingMidiClock = false;
	m_bIsLooping = false;
	m_nNoteOverlapMethods = 0;
	m_nDuplicateNoteMethods = 0;
	m_nSustainMask = 0;
	m_nSostenutoMask = 0;
	m_hStrm = 0;
	ZeroMemory(&m_arrMsgHdr, sizeof(m_arrMsgHdr));
	ZeroMemory(&m_stats, sizeof(m_stats));
	m_rngLoop = CLoopRange(0, 0);
	memset(m_arrPrevNote, 0xff, sizeof(m_arrPrevNote));
}

CSequencer::~CSequencer()
{
	Abort();
}

void CSequencer::OnMidiError(MMRESULT nResult)
{
	UNREFERENCED_PARAMETER(nResult);
}

bool CSequencer::IsErrorWarning(MMRESULT nSeqError)
{
	static const MMRESULT arrWarning[] = {	// non-fatal sequencer errors
		SEQERR_CALLBACK_TOO_LONG,
	};
	return ARRAY_FIND(arrWarning, nSeqError) >= 0;	// return true if error is a warning
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
	MIDIPROPTIMEDIV	mpTimeDiv = {sizeof(mpTimeDiv), static_cast<DWORD>(nTimeDiv)};
	CHECK(midiStreamProperty(m_hStrm, (BYTE *)&mpTimeDiv, MIDIPROP_SET | MIDIPROP_TIMEDIV));
	return true;
}

bool CSequencer::WriteTempo(double fTempo)
{
	int	nMicrosPerQtrNote = Round(CMidiFile::MICROS_PER_MINUTE / fTempo);
	MIDIPROPTEMPO	mpTempo = {sizeof(mpTempo), static_cast<DWORD>(nMicrosPerQtrNote)};
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
		m_nPosOffset = nTicks;
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

bool CSequencer::GetCurrentTempo(DWORD& dwTempo)
{
	MIDIPROPTEMPO	propTempo;
	propTempo.cbStruct = sizeof(propTempo);
	CHECK(midiStreamProperty(m_hStrm, reinterpret_cast<LPBYTE>(&propTempo), MIDIPROP_GET | MIDIPROP_TEMPO));
	dwTempo = propTempo.dwTempo;
	return true;
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
	int	nTicks = Trunc(nLatency / 1000.0 * m_fTempo / 60.0 * m_nTimeDiv) + 1;	// round up
	return max(nTicks, MIN_CALLBACK_LENGTH);	// helps to avoid glitches
}

void CSequencer::UpdateCallbackLength()
{
	m_nCBLen = GetCallbackLength(m_nLatency);
}

void CSequencer::SetNoteOverlapMode(bool bPrevent)
{
	if (m_bIsPlaying) {	// if playing
		WCritSec::Lock	lock(m_csTrack);	// serialize access to note reference counts
		if (bPrevent)	// if preventing note overlaps
			ZeroMemory(m_arrNoteRef, sizeof(m_arrNoteRef));	// zero note reference counts
		m_bPreventNoteOverlap = bPrevent;
	} else	// stopped
		m_bPreventNoteOverlap = bPrevent;	// serialization not needed; play resets reference counts
}

inline void CSequencer::ResetCachedParameters()
{
	memset(&m_MidiCache, 0xff, sizeof(m_MidiCache));	// MIDI parameters are 7-bit so 0xff is unused
	if (m_bPreventNoteOverlap)	// if preventing note overlaps
		ZeroMemory(m_arrNoteRef, sizeof(m_arrNoteRef));	// zero note reference counts
	ResetChannelStates();
	memset(m_arrPrevNote, 0xff, sizeof(m_arrPrevNote));	// init to invalid note
}

inline void CSequencer::ResetChannelStates()
{
	m_nSustainMask = 0;
	m_nSostenutoMask = 0;
	for (int iChan = 0; iChan < MIDI_CHANNELS; iChan++) {	// for each channel
		CChannelState&	cs = m_arrChannelState[iChan];	// reference channel's state
		cs.m_arrSustainNote.FastRemoveAll();	// empty sustain note array
		cs.m_arrSostenutoNote.FastRemoveAll();	// empty sostenuto note array
	}
}

bool CSequencer::IsValidMidiSongPosition(int nSongPos) const
{
	int	nMidiBeat = m_nTimeDiv / 4;	// sixteenth note in ticks
	return nSongPos >= 0	// MIDI song position limitations: must be positive
		&& (nSongPos % nMidiBeat) == 0	// and evenly divisible by a MIDI beat
		&& (nSongPos / nMidiBeat) < 0x4000;	// and less than 16K in MIDI beats
}

void CSequencer::QuantizeStartPositionForSync(int& nMidiSongPos)
{
	if (m_nStartPos > 0) {	// if start position is greater than zero
		int	nMidiBeat = m_nTimeDiv / 4;	// sixteenth note in ticks
		nMidiSongPos = m_nStartPos / nMidiBeat;
		if (m_nStartPos % nMidiBeat >= nMidiBeat / 2)	// if remainder exceeds halfway
			nMidiSongPos++;	// round up
		nMidiSongPos &= 0x3fff;	// limit to 16K in MIDI beats
		m_nStartPos = nMidiSongPos * nMidiBeat;	// quantize start position to nearest MIDI beat
	} else {	// start position is zero or less
		nMidiSongPos = 0;	// MIDI song position can't be negative
		m_nStartPos = 0;	// start song from position zero
	}
}

DWORD CSequencer::GetMidiSongPositionMsg(int nMidiSongPos) const
{
	// byte 0: command, byte 1: position's low 7 bits, byte 2: position's high 7 bits
	return SONG_POSITION | ((nMidiSongPos & 0x7f) << 8) | ((nMidiSongPos >> 7) << 16);
}

bool CSequencer::Play(bool bEnable, bool bRecord)
{
	if (bEnable == m_bIsPlaying)	// if already in requested state
		return true;	// nothing to do
	if (bEnable) {	// if playing
		int	nMidiSongPos = 0;
		if (m_bIsSendingMidiClock)	// if sending MIDI clock
			QuantizeStartPositionForSync(nMidiSongPos);	// ensure start position is sync-compatible
		if (bRecord) {	// if recording
			OnRecordStart(m_nStartPos);
		}
		ZeroMemory(&m_stats, sizeof(m_stats));
		m_stats.fCBTimeMin = DBL_MAX;
		m_fLatencySecs = m_nLatency / 1000.0;	// invariant while playing
		UpdateCallbackLength();
		ResetCachedParameters();
		m_nAltTempo = 0;
		m_fTempoScaling = 1;
		m_nCBTime = m_nStartPos;
		m_nPosOffset = m_nStartPos;
		m_iBuffer = 0;
		m_nMidiClockPeriod = m_nTimeDiv / 24;
		m_nMidiClockTimer = 0;
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
		for (int iEvent = 0; iEvent < nInitEvents; iEvent++) {	// for each initial MIDI event
			arrEvt[iEvent] = CMidiEvent(0, m_arrInitMidiEvent[iEvent]);	// copy event to output buffer
		}
		int	nEvents = nInitEvents;
		if (m_bIsSendingMidiClock) {	// if sending MIDI clock
			int	nStartCmd;
			if (nMidiSongPos) {	// if starting after MIDI beat zero
				arrEvt[nEvents] = CMidiEvent(0, GetMidiSongPositionMsg(nMidiSongPos));	// add song position message
				nEvents++;
				nStartCmd = CONTINUE;	// use continue message with song position, per MIDI specification
			} else	// starting at MIDI beat zero
				nStartCmd = START;	// use start message
			arrEvt[nEvents] = CMidiEvent(0, nStartCmd);	// add start or continue message to output buffer
			nEvents++;
		}
		arrEvt[nEvents] = CMidiEvent(m_nCBLen, SEVT_NOP);	// pad time to start of next callback
		nEvents++;
#if SEQ_DUMP_EVENTS
		m_arrDumpEvent.RemoveAll();
		AddDumpEvent(m_arrMidiEvent[0], nEvents);
#endif	// SEQ_DUMP_EVENTS
		WriteTimeDivision(m_nTimeDiv);	// output time division
		WriteTempo(m_fTempo);	// output tempo
		m_arrMsgHdr[0].dwBytesRecorded = nEvents * sizeof(MIDI_STREAM_EVENT);
		CHECK(midiStreamOut(m_hStrm, &m_arrMsgHdr[0], sizeof(MIDIHDR)));	// output lead-in
		if (m_bIsOutputCapture)	// if capturing output MIDI events
			QueueOutputEvents(nEvents);
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
		if (m_bIsSendingMidiClock)	// if sending MIDI clock
			OutputLiveEvent(STOP);	// output stop message
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

__forceinline int CSequencer::ApplyNoteRange(int nNote, int nRangeStart, int iRangeType)
{
	nNote = (nNote - nRangeStart) % OCTAVE;
	if (iRangeType == RT_DUAL) {	// if dual octave range
		nNote += nRangeStart;	// add start offset first
		if (nNote < 0)	// then wrap negative values
			nNote += OCTAVE;
	} else {	// default is single octave range
		if (nNote < 0)	// wrap negative values first
			nNote += OCTAVE;
		nNote += nRangeStart;	// then add start offset
	}
	return nNote;
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

__forceinline bool CSequencer::MakeControlEvent(const CTrack& trk, int nTime, int nVal, CMidiEvent& evt)
{
	int	nChannel = trk.m_nChannel;
	int	nNote = trk.m_nNote;
	char	cVal = static_cast<char>(nVal);
	switch (trk.m_iType) {
	case TT_KEY_AFT:
		if (cVal == m_MidiCache.arrKeyAft[nChannel][nNote])	// if value unchanged
			return false;	// no operation
		m_MidiCache.arrKeyAft[nChannel][nNote] = cVal;	// update cache
		evt.m_dwEvent = MakeMidiMsg(KEY_AFT, nChannel, nNote, nVal);
		break;
	case TT_CONTROL:
		if (cVal == m_MidiCache.arrControl[nChannel][nNote])	// if value unchanged
			return false;	// no operation
		m_MidiCache.arrControl[nChannel][nNote] = cVal;	// update cache
		evt.m_dwEvent = MakeMidiMsg(CONTROL, nChannel, nNote, nVal);
		break;
	case TT_PATCH:
		if (cVal == m_MidiCache.arrPatch[nChannel])	// if value unchanged
			return false;	// no operation
		m_MidiCache.arrPatch[nChannel] = cVal;	// update cache
		evt.m_dwEvent = MakeMidiMsg(PATCH, nChannel, nVal, 0);
		break;
	case TT_CHAN_AFT:
		if (cVal == m_MidiCache.arrChanAft[nChannel])	// if value unchanged
			return false;	// no operation
		m_MidiCache.arrChanAft[nChannel] = cVal;	// update cache
		evt.m_dwEvent = MakeMidiMsg(CHAN_AFT, nChannel, nVal, 0);
		break;
	case TT_WHEEL:
		if (cVal == m_MidiCache.arrWheel[nChannel])	// if value unchanged
			return false;	// no operation
		m_MidiCache.arrWheel[nChannel] = cVal;	// update cache
		// if above midpoint (zero in signed 7-bit), set LSB so bend upper limit is exact
		evt.m_dwEvent = MakeMidiMsg(WHEEL, nChannel, nVal > 0x40 ? 0x7f : 0, nVal);	// LSB, MSB
		break;
	default:
		NODEFAULTCASE;
	}
	evt.m_nTime = nTime;
	return true;
}

bool CSequencer::RecurseModulations(const CTrack& trk, int& nAbsEvtTime, int& nPosMod)
{
	int	nTracks = GetTrackCount();
	int	nMods = trk.m_arrModulator.GetSize();
	for (int iMod = 0; iMod < nMods; iMod++) {	// for each of track's modulators
		const CModulation&	mod = trk.m_arrModulator[iMod];
		int	iModSource = mod.m_iSource;
		int	iModType = mod.m_iType;
		// if modulation source index is valid and modulator is unmuted
		if (iModSource >= 0 && iModSource < nTracks && !GetMute(iModSource)) {
			if (mod.IsRecursiveType()) {	// if modulation type supports recursion
				const CTrack&	trkModSource = GetTrack(iModSource);
				int	nAbsEvtTime2 = nAbsEvtTime;	// copy event time; offset modulation may change it
				int	nPosMod2 = 0;
				if (trkModSource.IsModulated() && trkModSource.IsModulator()) {	// if modulator could be modulated
					if (m_nRecursions >= MOD_MAX_RECURSIONS) {	// if maximum recursion depth reached
						OnMidiError(SEQERR_TOO_MANY_RECURSIONS);
						return true;	// abort recursion
					}
					m_nRecursions++;	// increment recursion depth
					bool	bMute = RecurseModulations(trkModSource, nAbsEvtTime2, nPosMod2);	// traverse sub-modulations
					m_nRecursions--;	// decrement recursion depth
					if (bMute)	// if recursion returned mute
						continue;	// skip this modulator
				}
				int	iModStep = trkModSource.GetStepIndex(nAbsEvtTime2);	// use potentially offset event time
				if (nPosMod2)	// if recursion returned a non-zero position modulation
					iModStep = ModWrap(iModStep - nPosMod2, trkModSource.GetLength());	// modulate position
				int	nStepVal = trkModSource.m_arrStep[iModStep] & SB_VELOCITY;
				if (iModType == MT_Mute) {	// if mute modulation
					if (nStepVal)	// if non-zero step
						return true;	// abandon this branch and return mute
				} else {	// not mute modulation
					nStepVal -= MIDI_NOTES / 2;	// convert step value to signed offset
					if (iModType == MT_Position)	// if position modulation
						nPosMod += nStepVal;	// add step value to caller's position
					else	// assume offset modulation
						nAbsEvtTime -= nStepVal;	// subtract step value from caller's event time
				}
			}
		}
	}
	return false;	// return unmute
}

int CSequencer::SumModulations(const CTrack& trk, int iModType, int nAbsEvtTime)
{
	int	nSum = 0;
	int	nMods = trk.m_arrModulator.GetSize();
	for (int iMod = 0; iMod < nMods; iMod++) {	// for each of track's modulators
		const CModulation&	mod = trk.m_arrModulator[iMod];
		if (mod.m_iType == iModType) {	// if modulation type matches caller's
			int	iModSource = mod.m_iSource;
			if (iModSource >= 0 && iModSource < GetTrackCount() && !GetMute(iModSource)) {	// if modulator is valid and unmuted
				const CTrack&	trkModSource = GetTrack(iModSource);
				int	nPosMod = 0;
				if (trkModSource.IsModulated() && trkModSource.IsModulator()) {	// if modulator could be modulated
					m_nRecursions = 0;
					if (RecurseModulations(trkModSource, nAbsEvtTime, nPosMod))	// recurse into modulator's modulations
						continue;	// recursion returned mute, so skip this modulator
				}
				int	iModStep = trkModSource.GetStepIndex(nAbsEvtTime);
				if (nPosMod)	// if recursion returned a non-zero position modulation
					iModStep = ModWrap(iModStep - nPosMod, trkModSource.GetLength());	// modulate position
				int	nStepVal = trkModSource.m_arrStep[iModStep] & SB_VELOCITY;
				nStepVal -= MIDI_NOTES / 2;	// convert step value to signed offset
				nSum += nStepVal;
			}
		}
	}
	return nSum;
}

__forceinline void CSequencer::AddTrackEvents(CTrack& trk, int nCBStart)
{
	int	nOffset = trk.m_nOffset;	// cache these values for thread safety
	int	nLength = trk.GetLength();
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
	int	nTracks = GetTrackCount();
	while (nEvtTime < m_nCBLen) {	// while event time within callback period
		if (nEvtTime >= 0) {	// discard already played events
			int	nAbsEvtTime = nCBStart + nEvtTime;	// convert event time from callback-relative to absolute
			if (m_bIsSongMode) {	// if applying track dubs
				int	nDubs = trk.m_arrDub.GetSize();
				while (trk.m_iDub < nDubs && nAbsEvtTime >= trk.m_arrDub[trk.m_iDub].m_nTime) {
					trk.m_bMute = trk.m_arrDub[trk.m_iDub].m_bMute;
					trk.m_iDub++;
				}
			}
			if (!trk.m_bMute && trk.m_iType != TT_MODULATOR) {	// if track isn't muted and isn't a modulator
				double	fTempoModulation = 1;
				int	arrMod[MODULATION_TYPES] = {0};	// initialize modulations to zero
				m_arrScale.FastRemoveAll();
				m_arrChord.FastRemoveAll();
				m_arrVoicing.FastRemoveAll();
				int	nMods = trk.m_arrModulator.GetSize();
				for (int iMod = 0; iMod < nMods; iMod++) {	// for each of track's modulators
					const CModulation&	mod = trk.m_arrModulator[iMod];
					int	iModSource = mod.m_iSource;
					int	iModType = mod.m_iType;
					if (iModSource >= 0 && iModSource < nTracks && !GetMute(iModSource)) {	// if modulator is valid and unmuted
						const CTrack&	trkModSource = GetTrack(iModSource);
						int	iModStep;
						if (trkModSource.IsModulated() && trkModSource.IsModulator()) {	// if modulator could be modulated
							int	nAbsEvtTime2 = nAbsEvtTime;	// copy event time; offset modulation may change it
							int	nPosMod = 0;
							m_nRecursions = 0;
							if (RecurseModulations(trkModSource, nAbsEvtTime2, nPosMod))	// recurse into modulator's modulations
								continue;	// recursion returned mute, so skip this modulator
							iModStep = trkModSource.GetStepIndex(nAbsEvtTime2);	// use potentially offset event time
							if (nPosMod)	// if recursion returned a non-zero position modulation
								iModStep = ModWrap(iModStep - nPosMod, trkModSource.GetLength());	// modulate position
						} else {	// modulator isn't modulated
							iModStep = trkModSource.GetStepIndex(nAbsEvtTime);
						}
						int	nStepVal = trkModSource.m_arrStep[iModStep] & SB_VELOCITY;
						switch (iModType) {
						case MT_Mute:
							if (nStepVal)	// if track is muted by mute modulator
								goto lblNextStep;	// skip this step
							break;
						case MT_Tempo:
							{
								// duration is repurposed as tempo change percentage
								double	fValNorm = double(nStepVal - MIDI_NOTES / 2) / (MIDI_NOTES / 2);
								fTempoModulation *= pow(2, fValNorm * trkModSource.m_nDuration / 100.0);
							}
							break;
						case MT_Scale:
							{
								int	nTone = nStepVal - MIDI_NOTES / 2;
								if (trkModSource.IsModulated())	// if modulation source is modulated
									nTone += SumModulations(trkModSource, MT_Note, nAbsEvtTime);
								m_arrScale.Add(nTone);	// add tone to scale array
							}
							break;
						case MT_Chord:
							{
								int	nTone = nStepVal - MIDI_NOTES / 2;
								if (trkModSource.IsModulated())	// if modulation source is modulated
									nTone += SumModulations(trkModSource, MT_Note, nAbsEvtTime);
								m_arrChord.Add(nTone);	// add tone to chord array
							}
							break;
						case MT_Voicing:
							m_arrVoicing.Add(nStepVal - MIDI_NOTES / 2);	// add step to voicing array
							break;
						case MT_Offset:
							arrMod[MT_Offset] += nStepVal;	// offset modulation is unsigned (delay only)
							break;
						default:
							nStepVal -= MIDI_NOTES / 2;	// convert step value to signed offset
							arrMod[iModType] += nStepVal;	// accumulate modulation value
						}
					}
				}
				int	iModStep;
				if (arrMod[MT_Position]) {	// if modulating position
					iModStep = (iStep - arrMod[MT_Position]) % nLength;
					if (iModStep < 0)
						iModStep += nLength;
				} else	// not modulating position
					iModStep = iStep;
				if (trk.m_iType == TT_NOTE) {	// if track type is note
					if (trk.m_arrStep[iModStep]) {	// if step isn't empty
						int	nDurSteps = GetNoteDuration(trk.m_arrStep, nLength, iModStep);
						if (nDurSteps) {	// if at start of note
							int	nVel = (trk.m_arrStep[iModStep] & SB_VELOCITY) + trk.m_nVelocity + arrMod[MT_Velocity];
							nVel = CLAMP(nVel, 0, MIDI_NOTE_MAX);
							int	nNote = trk.m_nNote + arrMod[MT_Note];
							int	nScaleTones = m_arrScale.GetSize();
							if (nScaleTones) {	// if scale defined
								int	nChordTones = m_arrChord.GetSize();
								if (nChordTones) {	// if chord defined
									for (int iChordTone = 0; iChordTone < nChordTones; iChordTone++) {	// for each chord tone
										int		iScaleTone = m_arrChord[iChordTone];
										iScaleTone = ModWrap(iScaleTone, nScaleTones);
										m_arrChord[iChordTone] = m_arrScale[iScaleTone];	// map chord tone to scale tone
									}
									m_arrScale = m_arrChord;	// replace scale with chord
									nScaleTones = nChordTones;
								}
								int	nRangeStart = trk.m_nRangeStart + arrMod[MT_Range];
								for (int iTone = 0; iTone < nScaleTones; iTone++) {	// for each scale tone
									m_arrScale[iTone] += nNote;	// transpose
									if (trk.m_iRangeType != RT_NONE)	// if applying range to note
										m_arrScale[iTone] = ApplyNoteRange(m_arrScale[iTone], nRangeStart, trk.m_iRangeType);
								}
								int	nDrops = m_arrVoicing.GetSize();
								if (nDrops) {	// if dropping at least one voice
									m_arrScale.Sort();	// sort scale tones into ascending order
									bool	bDropped = false;
									for (int iDrop = 0; iDrop < nDrops; iDrop++) {	// for each voice drop
										int	iTone = nScaleTones - m_arrVoicing[iDrop];	// drop voicings are one-origin
										if (iTone >= 0 && iTone < nScaleTones) {	// if index within scale
											m_arrScale[iTone] -= OCTAVE;	// drop voice an octave
											bDropped = true;
										} else {	// negative value means raise instead of drop
											iTone = nScaleTones * 2 - iTone;	// still one-origin from top note
											if (iTone >= 0 && iTone < nScaleTones) {	// if index within scale
												m_arrScale[iTone] += OCTAVE;	// raise voice an octave
												bDropped = true;
											}
										}
									}
									if (bDropped)	// if any voices were dropped
										m_arrScale.Sort();	// re-sort scale tones
								}
								int	iTone = ModWrap(arrMod[MT_Index], nScaleTones);
								nNote = m_arrScale[iTone];
								if (m_nDuplicateNoteMethods & (1 << trk.m_nChannel)) {	// if channel prevents duplicate notes
									if (nNote == m_arrPrevNote[trk.m_nChannel]) {	// if note matches channel's previous note
										iTone++;	// try scale or chord's next tone
										if (iTone >= nScaleTones)	// if out of range
											iTone = 0;	// wrap around to first tone
										nNote = m_arrScale[iTone];
									}
									m_arrPrevNote[trk.m_nChannel] = static_cast<BYTE>(nNote);	// update previous note
								}
							} else {	// scale not defined
								if (trk.m_iRangeType != RT_NONE)	// if applying range to note
									nNote = ApplyNoteRange(nNote, trk.m_nRangeStart + arrMod[MT_Range], trk.m_iRangeType);
							}
							nNote = CLAMP(nNote, 0, MIDI_NOTE_MAX);
							CMidiEvent	evt;
							evt.m_nTime = nEvtTime;
							evt.m_dwEvent = MakeMidiMsg(NOTE_ON, trk.m_nChannel, nNote, nVel);
							if (arrMod[MT_Offset] > 0) {	// if offset modulation is active, delay note
								evt.m_nTime += arrMod[MT_Offset];	// add offset modulation to event time
								nAbsEvtTime += arrMod[MT_Offset];	// add offset modulation to absolute time
								if (evt.m_nTime >= m_nCBLen) {	// if delayed note starts after this callback
									evt.m_nTime = nAbsEvtTime;	// make event time absolute instead of callback-relative
									m_arrNoteOff.FastInsertSorted(evt);	// schedule delayed note via note off array
									goto lblNoteScheduled;
								}
							}
							m_arrEvent.FastInsertSorted(evt);	// add note to sorted array for output
lblNoteScheduled:;
							if (nVel) {	// if non-zero velocity, queue note off
								int	nDuration = nDurSteps * nQuant + trk.m_nDuration + arrMod[MT_Duration];	// add duration offset
								if (nDurSteps & 1)	// if odd duration
									nDuration += nSwing;	// add swing to duration
								nDuration = max(nDuration, 1);	// keep duration above zero
								evt.m_nTime = nAbsEvtTime + nDuration;	// absolute time
								evt.m_dwEvent &= ~MIDI_P2_MASK;	// zero note's velocity
								m_arrNoteOff.FastInsertSorted(evt);	// add pending note off to array
							}
						}
					}
				} else if (trk.m_iType == TT_TEMPO) {	// if track type is tempo
					int	nVal = trk.m_arrStep[iModStep] & SB_VELOCITY;
					double	fValNorm = double(nVal - MIDI_NOTES / 2) / (MIDI_NOTES / 2);
					double	fTempoScaling = pow(2, fValNorm * trk.m_nDuration / 100.0) * fTempoModulation;
					double	fNewTempo = m_fTempo * fTempoScaling;
					int	nTempo = Round(CMidiFile::MICROS_PER_MINUTE / fNewTempo);
					if (nTempo != m_nAltTempo) {	// if altered tempo actually changed
						m_nAltTempo = nTempo;	// update shadow
						m_fTempoScaling = fTempoScaling;	// save tempo scaling factor
						CMidiEvent	evtTempo(nEvtTime, SEVT_TEMPO | nTempo);
						m_arrTempoEvent.FastAdd(evtTempo);	// add tempo event
					}
				} else if (trk.m_iType == TT_INTERNAL) {	// if track type is internal
					int	nVal = (trk.m_arrStep[iModStep] & SB_VELOCITY) + trk.m_nVelocity + arrMod[MT_Velocity];
					nVal = CLAMP(nVal, 0, MIDI_NOTE_MAX);
					char	cVal = static_cast<char>(nVal);	// avoids compiler warning
					if (cVal != m_MidiCache.arrInternal[trk.m_nChannel][trk.m_nNote]) {	// if value changed
						m_MidiCache.arrInternal[trk.m_nChannel][trk.m_nNote] = cVal;	// update cache
						switch (trk.m_nNote) {
						case ICTL_SUSTAIN:
						case ICTL_SOSTENUTO:
						case ICTL_ALL_NOTES_OFF:
							{
								CMidiEvent	evt;
								evt.m_nTime = nEvtTime + arrMod[MT_Offset] + nCBStart;
								evt.m_dwEvent = MakeMidiMsg(CONTROL, trk.m_nChannel, trk.m_nNote, nVal);
								evt.m_dwEvent |= SEVT_INTERNAL;	// set bit to indicate event is internal
								m_arrNoteOff.FastInsertSorted(evt);	// controller is handled by AddNoteOffs
							}
							break;
						case ICTL_DUPLICATE_NOTES:
							SetDuplicateNoteMethod(trk.m_nChannel, nVal != 0);
							break;
						case ICTL_NOTE_OVERLAP:
							if (m_bPreventNoteOverlap) {	// if preventing note overlap
								CMidiEvent	evt;
								evt.m_nTime = nEvtTime + arrMod[MT_Offset] + nCBStart;
								evt.m_dwEvent = MakeMidiMsg(CONTROL, trk.m_nChannel, trk.m_nNote, nVal);
								evt.m_dwEvent |= SEVT_INTERNAL;	// set bit to indicate event is internal
								m_arrEvent.FastInsertSorted(evt);	// controller is handled by FixNoteOverlaps
							}
							break;
						}
					}
				} else {	// track type is control event
					int	nVal = (trk.m_arrStep[iModStep] & SB_VELOCITY) + trk.m_nVelocity + arrMod[MT_Velocity];
					nVal = CLAMP(nVal, 0, MIDI_NOTE_MAX);
					CMidiEvent	evt;
					if (MakeControlEvent(trk, nEvtTime, nVal, evt)) {	// if event state changed
						if (arrMod[MT_Offset] > 0) {	// if offset modulation is active, delay event
							evt.m_nTime += arrMod[MT_Offset];	// add offset modulation to event time
							if (evt.m_nTime >= m_nCBLen) {	// if delayed event starts after this callback
								evt.m_nTime += nCBStart;	// make event time absolute instead of callback-relative
								m_arrNoteOff.FastInsertSorted(evt);	// schedule delayed event via note off array
								goto lblControlScheduled;
							}
						}
						m_arrEvent.FastInsertSorted(evt);	// add event to sorted array for output
					}
lblControlScheduled:;
				}
			}
		}
lblNextStep:
		iStep++;	// advance to next track step
		if (iStep >= nLength)	// if track length reached
			iStep -= nLength;	// wrap to start of track
		nEvtTime += nQuant + nSwing;	// advance event time by track's swung time quant
		nSwing = -nSwing;	// invert swing
	}
}

__forceinline void CSequencer::AddNoteOffs(int nCBStart, int nCBEnd)
{
	int	nOffs = m_arrNoteOff.GetSize();	// cache note off count
	while (nOffs > 0 && m_arrNoteOff[0].m_nTime < nCBEnd) {	// while more note offs within this callback period
		CMidiEvent&	noteOff = m_arrNoteOff[0];	// reference the first note off in the array
		BYTE	iChan = MIDI_CHAN(noteOff.m_dwEvent);	// get note off's channel index
		if (noteOff.m_dwEvent & SEVT_INTERNAL) {	// if note off is an internal event
			OnInternalControl(noteOff, iChan, nCBStart);	// handle internal event
			nOffs = m_arrNoteOff.GetSize();	// assume note off array changed and update cached count
		} else {	// ordinary note off
			USHORT	nChannelBit = MAKE_CHANNEL_MASK(iChan);	// convert channel index to bitmask
			if (m_nSustainMask & nChannelBit) {	// if channel's sustain is on
				m_arrChannelState[iChan].m_arrSustainNote.FastAdd(noteOff);	// add note off to channel's sustain array
			} else {	// channel's sustain is off
				noteOff.m_nTime -= nCBStart;	// make time relative to this callback
				ASSERT(noteOff.m_nTime >= 0);	// time must be positive
				m_arrEvent.FastInsertSorted(noteOff);	// add note off to event array for output
			}
		}
		m_arrNoteOff.FastRemoveAt(0);	// remove note off that was processed above
		nOffs--;	// one less note off
	}
}

void CSequencer::OnInternalControl(const CMidiEvent& noteOff, BYTE iChan, int nCBStart)
{
	USHORT	nChannelBit = MAKE_CHANNEL_MASK(iChan);	// convert channel index to bitmask
	CChannelState&	cs = m_arrChannelState[iChan];	// reference channel's state for brevity
	switch (MIDI_P1(noteOff.m_dwEvent)) {	// internal controller number determines action if any
	case ICTL_SUSTAIN:	// if internal sustain controller
		if (MIDI_P2(noteOff.m_dwEvent)) {	// if event data is non-zero
			m_nSustainMask |= nChannelBit;	// enable channel's sustain
		} else {	// event data is zero
			if (m_nSustainMask & nChannelBit) {	// if channel's sustain is on
				m_nSustainMask &= ~nChannelBit;	// disable channel's sustain
				ReleaseHeldNotes(cs.m_arrSustainNote, noteOff.m_nTime, nCBStart);	// release sustain notes
			}
		}
		break;
	case ICTL_SOSTENUTO:	// if internal sostenuto controller
		if (MIDI_P2(noteOff.m_dwEvent)) {	// if event data is non-zero
			if (!(m_nSostenutoMask & nChannelBit)) {	// if channel's sostenuto is off
				m_nSostenutoMask |= nChannelBit;	// enable channel's sostenuto
				CullNoteOffs(cs.m_arrSostenutoNote, iChan, noteOff.m_nTime, nCBStart);	// cull sostenuto notes
			}
		} else {	// event data is zero
			if (m_nSostenutoMask & nChannelBit) {	// if channel's sostenuto is on
				m_nSostenutoMask &= ~nChannelBit;	// disable channel's sostenuto
				if (m_nSustainMask & nChannelBit) {	// if channel's sustain is on
					cs.m_arrSustainNote.Append(cs.m_arrSostenutoNote);	// add sostenuto notes to sustain array
					cs.m_arrSostenutoNote.FastRemoveAll();	// empty sostenuto array
				} else {	// channel's sustain is off
					ReleaseHeldNotes(cs.m_arrSostenutoNote, noteOff.m_nTime, nCBStart);	// release sostenuto notes
				}
			}
		}
		break;
	case ICTL_ALL_NOTES_OFF:	// if internal all notes off controller
		if (MIDI_P2(noteOff.m_dwEvent)) {	// if event data is non-zero
			AllNotesOff(iChan, noteOff.m_nTime, nCBStart);
		}
		break;
	}
}

void CSequencer::ReleaseHeldNotes(CMidiEventArray& arrHeldNoteOff, int nTime, int nCBStart, bool bForceExpire)
{
	int	nHolds = arrHeldNoteOff.GetSize();
	for (int iHeld = 0; iHeld < nHolds; iHeld++) {	// for each held note
		CMidiEvent&	noteOff = arrHeldNoteOff[iHeld];
		if (noteOff.m_nTime > nTime && !bForceExpire) {	// if note off is pending
			m_arrNoteOff.FastInsertSorted(noteOff);	// restore to note off array
		} else {	// note off is expired, or caller is forcing expiration
			noteOff.m_nTime = nTime;	// set event time to caller's time
			noteOff.m_nTime -= nCBStart;	// make time relative to this callback
			ASSERT(noteOff.m_nTime >= 0);	// time must be positive
			m_arrEvent.FastInsertSorted(noteOff);	// insert in output event array
		}
	}
	arrHeldNoteOff.FastRemoveAll();	// empty held note array
}

void CSequencer::CullNoteOffs(CMidiEventArray& arrCulledNoteOff, BYTE iChan, int nTime, int nCBStart)
{
	arrCulledNoteOff.FastRemoveAll();
	int	nOffs = m_arrNoteOff.GetSize();
	CMidiEvent	evtFindStart(nTime - nCBStart, 0);	// start event search at caller's time
	int	iEventStart = static_cast<int>(m_arrEvent.BinarySearchAbove(evtFindStart));
	for (int iOff = nOffs - 1; iOff >= 0; iOff--) {	// reverse iterate for deletion stability
		CMidiEvent&	noteOff = m_arrNoteOff[iOff];
		if (MIDI_CHAN(noteOff.m_dwEvent) == iChan	// if event channel matches
		&& !(noteOff.m_dwEvent & SEVT_INTERNAL)) {	// and not internal event
			bool	bIsFutureNote = false;	// assume note was triggered before caller's time
			if (iEventStart >= 0) {	// if event search starting index is valid
				int	nEvents = m_arrEvent.GetSize();
				int	nRelOffTime = noteOff.m_nTime - nCBStart;	// make note off's time callback-relative
				for (int iEvent = iEventStart; iEvent < nEvents; iEvent++) {	// for each event
					const CMidiEvent&	evt = m_arrEvent[iEvent];
					if (evt.m_nTime >= nRelOffTime) {	// if event time equals or exceeds note off's
						break;	// any subsequent events are irrelevant
					}
					// if event is a note on message, and its channel and note number match note off's
					if (MIDI_SHORT_MSG_CMD(evt.m_dwEvent) == NOTE_ON && (evt.m_dwEvent & MIDI_P2_MASK)
					&& (evt.m_dwEvent & ~MIDI_P2_MASK) == noteOff.m_dwEvent) {
						bIsFutureNote = true;	// note was triggered after caller's time
						break;	// note off belongs to a future note and must remain untouched
					}
				}
			}
			if (!bIsFutureNote) {	// if note was triggered before caller's time, it's fair game
				arrCulledNoteOff.FastAdd(noteOff);	// first add note off to destination array
				m_arrNoteOff.FastRemoveAt(iOff);	// then remove it; it's a reference, so order matters
			}
		}
	}
}

void CSequencer::AllNotesOff(BYTE iChan, int nTime, int nCBStart)
{
	CChannelState&	cs = m_arrChannelState[iChan];	// reference channel's state
	ReleaseHeldNotes(cs.m_arrSustainNote, nTime, nCBStart, true);	// force expiration
	ReleaseHeldNotes(cs.m_arrSostenutoNote, nTime, nCBStart, true);	// force expiration
	CMidiEventArray	arrNoteOff;
	CullNoteOffs(arrNoteOff, iChan, nTime, nCBStart);
	ReleaseHeldNotes(arrNoteOff, nTime, nCBStart, true);	// force expiration
}

__forceinline bool CSequencer::IsRecordedEventPlayback() const
{
	// if recorded events exist and we're in song mode and not recording
	return m_arrRecordEvent.GetSize() && m_bIsSongMode && !m_bIsRecording;
}

void CSequencer::AddRecordedEvents(int nCBStart, int nCBEnd)
{
	int	nRecordEvents = m_arrRecordEvent.GetSize();
	while (m_iRecordEvent < nRecordEvents) {	// while more recorded events
		CMidiEvent	evtOut(m_arrRecordEvent[m_iRecordEvent]);
		evtOut.m_nTime += m_nRecordOffset;	// compensate for recording latency
		if (evtOut.TimeInRange(nCBStart, nCBEnd)) {	// if event is due
			evtOut.m_nTime -= nCBStart;	// make time relative to start of callback
			if (m_mapping.GetCount()) {
				WCritSec::Lock	lock(m_mapping.GetCritSec());	// serialize access to mappings
				if (m_mapping.GetArray().MapMidiEvent(evtOut.m_dwEvent, m_arrMappedEvent)) {
					int	nMaps = m_arrMappedEvent.GetSize();
					for (int iMap = 0; iMap < nMaps; iMap++) {	// for each translated event
						evtOut.m_dwEvent = m_arrMappedEvent[iMap];
						m_arrEvent.FastInsertSorted(evtOut);	// insert translated event
					}
					goto lblEventWasMapped;	// input event was mapped so don't output it
				}
			}
			m_arrEvent.FastInsertSorted(evtOut);	// insert event
lblEventWasMapped:;
			m_iRecordEvent++;	// increment recorded event index
		} else
			break;
	}
}

bool CSequencer::OutputMidiBuffer()
{
	CBenchmark b;	// for timing statistics only
	int	nCBStart, nCBEnd;
	{
		WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
		if (m_bIsLooping && m_nCBTime + m_nCBLen >= m_rngLoop.m_nTo)	// if looping and callback reaches end of loop
			SetPosition(m_rngLoop.m_nFrom);	// set position to start of loop
		m_arrEvent.FastRemoveAll();
		m_arrTempoEvent.FastRemoveAll();
		// handle tempo change first to improve responsiveness
		if (m_bIsTempoChange) {	// if tempo changed
			m_bIsTempoChange = false;	// reset signal
			int	nTempo = Round(CMidiFile::MICROS_PER_MINUTE / (m_fTempo * m_fTempoScaling));
			CMidiEvent	evtTempo(0, SEVT_TEMPO | nTempo);	// at callback start time
			m_arrTempoEvent.FastAdd(evtTempo);	// add tempo event
		}
		// if position changed, turn off any active notes before adding new events
		if (m_bIsPositionChange) {	// if position changed
			m_bIsPositionChange = false;	// reset signal
			int	nEvents = m_arrNoteOff.GetSize();
			for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each active note
				m_arrNoteOff[iEvent].m_nTime = 0;	// at callback start time
				m_arrEvent.FastAdd(m_arrNoteOff[iEvent]);	// add note off event
			}
			m_arrNoteOff.FastRemoveAll();	// empty note off array
		}
		nCBStart = m_nCBTime;
		nCBEnd = nCBStart + m_nCBLen;
		m_nCBTime = nCBEnd;	// advance callback time by one period
		int	nTracks = GetSize();
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			CTrack&	trk = GetAt(iTrack);
			bool	bIsDubbing = false;
			if (m_bIsSongMode) {	// if applying track dubs
				if (trk.m_iDub < trk.m_arrDub.GetSize()) {	// if unplayed dubs remain
					if (trk.m_arrDub[trk.m_iDub].m_nTime < nCBEnd)	// if dubs during this callback
						bIsDubbing = true;	// call AddTrackEvents even if track is muted
				}
			}
			if (!trk.m_bMute || bIsDubbing)	// if track isn't muted, or track is dubbing
				AddTrackEvents(trk, nCBStart);	// add events for this callback period
		}
		if (IsRecordedEventPlayback())
			AddRecordedEvents(nCBStart, nCBEnd);	// add recorded events for this callback period
	}	// leave callback critical section, releasing lock on callback state
	AddNoteOffs(nCBStart, nCBEnd);	// add note off events for this callback period
	if (m_bIsSendingMidiClock) {	// if sending MIDI clock
		while (m_nMidiClockTimer < m_nCBLen) {	// while MIDI clock timer less than callback period
			CMidiEvent	evtClock(m_nMidiClockTimer, CLOCK);
			m_arrEvent.FastInsertSorted(evtClock);	// output MIDI clock
			m_nMidiClockTimer += m_nMidiClockPeriod;	// increment MIDI clock timer by clock period
		}
		m_nMidiClockTimer -= m_nCBLen;	// decrement MIDI clock timer by callback period
	}
	m_iBuffer ^= 1;	// swap playback buffers
	// fill MIDI output buffer with events created above
	CMidiEventStream&	arrEvt = m_arrMidiEvent[m_iBuffer];
	int	nEvents;
	int	nTempoEvts = m_arrTempoEvent.GetSize();
	if (m_arrEvent.GetSize() || nTempoEvts) {	// if any events to output
		if (m_bPreventNoteOverlap) {	// if preventing note overlap
			FixNoteOverlaps();
		}
		// add tempo events after note processing, which can't handle meta-events
		for (int iTempoEvt = 0; iTempoEvt < nTempoEvts; iTempoEvt++) {	// for each tempo event
			m_arrEvent.FastInsertSorted(m_arrTempoEvent[iTempoEvt]);	// insert into event array
		}
		CMidiEvent	evtNOP(m_nCBLen, SEVT_NOP);
		m_arrEvent.FastAdd(evtNOP);	// pad time to start of next callback
		nEvents = m_arrEvent.GetSize();	// final event count, after all insertions and deletions
		if (nEvents >= arrEvt.GetSize()) {	// if events exceed output buffer size
			OnMidiError(SEQERR_BUFFER_OVERRUN);
			return false;
		}
		// convert event times (relative to start of callback period) to delta times
		int	nPrevTime = 0;
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each event
			const CMidiEvent&	evt = m_arrEvent[iEvent];
			arrEvt[iEvent].dwDeltaTime = evt.m_nTime - nPrevTime;	// convert to delta time
			arrEvt[iEvent].dwEvent = evt.m_dwEvent;
			nPrevTime = evt.m_nTime;
		}
	} else {	// no events to output, so output NOP
		nEvents = 1;
		if (!arrEvt.GetSize()) {	// if events exceed buffer size
			OnMidiError(SEQERR_BUFFER_OVERRUN);
			return false;
		}
		arrEvt[0].dwDeltaTime = m_nCBLen;
		arrEvt[0].dwEvent = SEVT_NOP;
	}
#if SEQ_DUMP_EVENTS
	AddDumpEvent(arrEvt, nEvents);
#endif	// SEQ_DUMP_EVENTS
	MIDIHDR&	hdr = m_arrMsgHdr[m_iBuffer];
	hdr.dwBytesRecorded = nEvents * sizeof(MIDI_STREAM_EVENT);
	CHECK(midiStreamOut(m_hStrm, &hdr, sizeof(MIDIHDR)));	// output MIDI buffer
	if (m_bIsOutputCapture)	// if capturing output MIDI events
		QueueOutputEvents(nEvents);
	// update statistics only after this point
	double t = b.Elapsed();
	m_stats.nCallbacks++;
	if (t < m_stats.fCBTimeMin)
		m_stats.fCBTimeMin = t;
	if (t > m_stats.fCBTimeMax)
		m_stats.fCBTimeMax = t;
	m_stats.fCBTimeSum += t;
	if (t > m_fLatencySecs) {	// if callback took too long
		m_stats.nUnderruns++;
		if (m_stats.nUnderruns == 1)	// if first instance of this error
			OnMidiError(SEQERR_CALLBACK_TOO_LONG);	// report error
	}
	return true;
}

void CSequencer::FixNoteOverlaps()
{
	int	nEvents = m_arrEvent.GetSize();
	int	iEvent = 0;
	USHORT	nNoteOverlapMethods = m_nNoteOverlapMethods;	// cache per-channel bitmask
	while (iEvent < nEvents) {	// for each event
		DWORD	dwEvent = m_arrEvent[iEvent].m_dwEvent;
		if (dwEvent & SEVT_INTERNAL) {	// if internal event
			if (MIDI_P1(dwEvent) == ICTL_NOTE_OVERLAP) {	// if internal note overlap controller
				SetNoteOverlapMethod(MIDI_CHAN(dwEvent), MIDI_P2(dwEvent) != 0);
				nNoteOverlapMethods = m_nNoteOverlapMethods;	// update cached bitmask
			}
			m_arrEvent.FastRemoveAt(iEvent);	// delete internal event
			nEvents--;	// one less event
			continue;	// proceed to next event if any
		}
		ASSERT(MIDI_IS_SHORT_MSG(dwEvent));	// short MIDI messages only
		if (MIDI_CMD(dwEvent) == NOTE_ON) {	// if note on event
			int	iChan = MIDI_CHAN(dwEvent);
			int	iNote = MIDI_P1(dwEvent);
			if (MIDI_P2(dwEvent)) {	// if event is note on (non-zero velocity)
				if (m_arrNoteRef[iChan][iNote]++) {	// if note is already active; post-increment reference count
					int	nTime = m_arrEvent[iEvent].m_nTime;
					for (int iPrevEvt = iEvent - 1; iPrevEvt >= 0; iPrevEvt--) {	// for each previous event
						if (m_arrEvent[iPrevEvt].m_nTime != nTime)	// if previous event has different time
							break;	// stop iterating
						// if previous event is note on for same channel and note number
						if (USHORT(m_arrEvent[iPrevEvt].m_dwEvent) == USHORT(dwEvent)) {
							m_arrEvent.FastRemoveAt(iPrevEvt);	// delete previous note on event
							nEvents--;	// one less event
							goto lblFixNoteOverlaps;	// skip incrementing index to account for deletion
						}
					}
					USHORT	nEvtChanMask = MAKE_CHANNEL_MASK(iChan);
					if (nNoteOverlapMethods & nEvtChanMask) {	// if this channel wants note overlaps merged
						m_arrEvent.FastRemoveAt(iEvent);	// delete note on event
						nEvents--;	// one less event
						goto lblFixNoteOverlaps;	// skip incrementing index to account for deletion
					}
					CMidiEvent	evt(m_arrEvent[iEvent]);
					evt.m_dwEvent &= ~MIDI_P2_MASK;	// convert to note off (zero note's velocity)
					m_arrEvent.FastInsertAt(iEvent, evt);	// insert note off event
					iEvent++;	// extra index increment to account for insertion
					nEvents++;	// one more event
				}
			} else {	// event is note off (zero velocity)
				if (m_arrNoteRef[iChan][iNote] > 0) {	// if note is active
					m_arrNoteRef[iChan][iNote]--;	// decrement note's reference count
					if (m_arrNoteRef[iChan][iNote]) {	// if note is still active
						m_arrEvent.FastRemoveAt(iEvent);	// delete note off event
						nEvents--;	// one less event
						continue;	// skip incrementing index to account for deletion
					}
				}
			}
		}
		iEvent++;	// increment index to access next event
lblFixNoteOverlaps:;
	}
}

bool CSequencer::ExportImpl(LPCTSTR pszPath, int nDuration, int nKeySig)
{
	// note that this method may throw CFileException (from CMidiFile)
	if (nDuration <= 0)	// if invalid duration (in seconds)
		return false;	// avoid divide by zero
	ResetCachedParameters();	// crucial to avoid undefined controller behavior
	CIntArrayEx	arrUsedTrack;
	int	arrFirstTrack[MIDI_CHANNELS];
	int	nUsedTracks = GetChannelUsage(arrFirstTrack, !m_bIsSongMode);	// if track mode, exclude muted tracks
	int	nRecEvts = m_arrRecordEvent.GetSize();
	for (int iRecEvt = 0; iRecEvt < nRecEvts; iRecEvt++) {	// for each record event
		int	iChan = MIDI_CHAN(m_arrRecordEvent[iRecEvt].m_dwEvent);
		if (arrFirstTrack[iChan] < 0) {	// if channel unused by tracks
			arrFirstTrack[iChan] = INT_MAX;	// mark channel used
			nUsedTracks++;
		}
	}
	ChaseDubs(m_nStartPos, true);	// reset dub indices and update mutes
	int	nChunkDuration = m_nLatency;	// same latency as playback to ensure identical dubbing
	CMidiFile	fMidi(pszPath, CFile::modeCreate | CFile::modeWrite);	// create MIDI file
	USHORT	uTracks = static_cast<USHORT>(nUsedTracks);
	USHORT	uTimeDiv = static_cast<USHORT>(m_nTimeDiv);
	int	nChunkLen = GetCallbackLength(nChunkDuration);	// convert chunk duration to ticks
	int nDurationTicks = static_cast<int>(ConvertSecondsToPosition(nDuration));
	int	nChunks = (nDurationTicks - 1) / nChunkLen + 1;	// number of chunks, rounded up
	m_nCBLen = nChunkLen;	// set callback length member; used in AddTrackEvents
	int	nCBTime = m_nStartPos;
	CMidiFile::CMidiEventArray arrMidiEvent[MIDI_CHANNELS];
	int	nInitEvents = m_arrInitMidiEvent.GetSize();
	for (int iEvt = 0; iEvt < nInitEvents; iEvt++) {	// for each initial event
		int	iChan = MIDI_CHAN(m_arrInitMidiEvent[iEvt]);
		CMidiFile::MIDI_EVENT	evt = {static_cast<DWORD>(m_nStartPos), m_arrInitMidiEvent[iEvt]};
		arrMidiEvent[iChan].FastAdd(evt);	// add initial event to per-channel array
	}
	CMidiEventArray arrSongTempoEvent;
	int	nTracks = GetSize();
	for (int iChunk = 0; iChunk < nChunks; iChunk++) {	// for each time chunk
		m_arrEvent.FastRemoveAll();	// empty event array
		m_arrTempoEvent.FastRemoveAll();	// empty tempo event array
		int	nCBEnd = nCBTime + nChunkLen;
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			AddTrackEvents(GetAt(iTrack), nCBTime);	// get track's events for this chunk
		}
		if (IsRecordedEventPlayback())
			AddRecordedEvents(nCBTime, nCBEnd);	// add recorded events for this chunk
		AddNoteOffs(nCBTime, nCBEnd);	// add note offs for this chunk
		if (m_bPreventNoteOverlap)	// if preventing note overlap
			FixNoteOverlaps();
		int	nEvents = m_arrEvent.GetSize();
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each event
			const CMidiEvent&	evt = m_arrEvent[iEvent];
			CMidiFile::MIDI_EVENT	midiEvt;
			midiEvt.DeltaT = evt.m_nTime + nCBTime;	// convert to song time
			midiEvt.Msg = evt.m_dwEvent;
			int	iChan = MIDI_CHAN(evt.m_dwEvent);
			arrMidiEvent[iChan].FastAdd(midiEvt);	// add event to per-channel array
		}
		int	nTempoEvts = m_arrTempoEvent.GetSize();
		for (int iTempoEvt = 0; iTempoEvt < nTempoEvts; iTempoEvt++) {	// for each tempo event
			m_arrTempoEvent[iTempoEvt].m_nTime += nCBTime;	// convert to song time
			arrSongTempoEvent.FastInsertSorted(m_arrTempoEvent[iTempoEvt]);	// add event to song tempo event array
		}
		nCBTime += nChunkLen;
	}
	if (m_bExportAllNotesOff) {	// if turning off all notes
		m_arrEvent.FastRemoveAll();	// empty event array
		for (int iChan = 0; iChan < MIDI_CHANNELS; iChan++) {	// for each MIDI channel
			AllNotesOff(static_cast<BYTE>(iChan), nCBTime, nCBTime);	// turn off all notes
		}
		int	nEvents = m_arrEvent.GetSize();
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each event
			const CMidiEvent&	evt = m_arrEvent[iEvent];
			CMidiFile::MIDI_EVENT	midiEvt;
			midiEvt.DeltaT = evt.m_nTime + nCBTime;	// convert to song time
			midiEvt.Msg = evt.m_dwEvent;
			int	iChan = MIDI_CHAN(evt.m_dwEvent);
			arrMidiEvent[iChan].FastAdd(midiEvt);	// add event to per-channel array
		}
	}
	CMidiFile::CMidiEventArray arrTempoMap;
	CMidiFile::CMidiEventArray *parrTempoMap = NULL;
	int	nTempoEvts = arrSongTempoEvent.GetSize();
	if (nTempoEvts) {
		arrTempoMap.SetSize(nTempoEvts);
		int	nPrevTime = m_nStartPos;
		for (int iTempoEvt = 0; iTempoEvt < nTempoEvts; iTempoEvt++) {	// for each tempo event
			const CMidiEvent&	evt = arrSongTempoEvent[iTempoEvt];
			int	nTime = evt.m_nTime - nPrevTime;
			nPrevTime = evt.m_nTime;
			CMidiFile::MIDI_EVENT	midiEvt = {static_cast<DWORD>(nTime), MEVT_EVENTPARM(evt.m_dwEvent)};
			arrTempoMap[iTempoEvt] = midiEvt;
		}
		parrTempoMap = &arrTempoMap;
	}
	CMidiFile::TIME_SIGNATURE	sigTime = {static_cast<BYTE>(m_nMeter), 2, 0, 8};	// denominator is quarter note
	CMidiFile::KEY_SIGNATURE	sigKey = {static_cast<signed char>(GetMidiKeySignature(nKeySig)), 0};
	CMidiFile::TIME_SIGNATURE	*pTimeSig;
	CMidiFile::KEY_SIGNATURE	*pKeySig;
	if (m_bExportTimeKeySigs) {	// if exporting time and key signatures
		pTimeSig = &sigTime;
		pKeySig = &sigKey;
	} else {	// exclude time and key signatures
		pTimeSig = NULL;
		pKeySig = NULL;
	}
	fMidi.WriteHeader(uTracks, uTimeDiv, m_fTempo, nDurationTicks, pTimeSig, pKeySig, parrTempoMap);	// write MIDI file header
	for (int iChan = 0; iChan < MIDI_CHANNELS; iChan++) {	// for each MIDI channel
		int	iTrack = arrFirstTrack[iChan];
		if (iTrack >= 0) {	// if channel is used
			int	nEvents = arrMidiEvent[iChan].GetSize();
			int	nPrevTime = m_nStartPos;
			for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each channel event
				CMidiFile::MIDI_EVENT&	midiEvt = arrMidiEvent[iChan][iEvent];
				int	nTime = midiEvt.DeltaT;
				midiEvt.DeltaT -= nPrevTime;	// convert to delta time
				nPrevTime = nTime;
			}
			CString	sName;
			if (iTrack != INT_MAX)	// if channel used by tracks
				sName = GetName(iTrack);
			fMidi.WriteTrack(arrMidiEvent[iChan], sName);	// write track to MIDI file
		}
	}
	return true;
}

bool CSequencer::Export(LPCTSTR pszPath, int nDuration, bool bSongMode, int nStartPos, int nKeySig)
{
	CSequencerReader	seq(*this);	// give export its own sequencer instance
	seq.SetSongMode(bSongMode);
	seq.m_nStartPos = nStartPos;
	return seq.ExportImpl(pszPath, nDuration, nKeySig);
}

inline int CSequencer::GetMidiKeySignature(int nKey)
{
	// convert key [0..11] to number of sharps (positive) or flats (negative)
	ASSERT(nKey >= 0 && nKey < 12);
	if (nKey & 1)
		return nKey - 6;
	if (nKey >= 6)
		return nKey - 12;
	return nKey;
}

CSequencerReader::CSequencerReader(CSequencer& seq)
{
	// attach our track array to sequencer's track array; risky but OK so long as
	// our instance is destroyed before sequencer, and never modifies track array
	Attach(seq.GetData(), seq.GetSize());
	GetMutes(m_arrTrackMute);	// export does modify track mutes, so save them
	CSequencerBase&	seqBaseData = *this;	// downcast to base class
	seqBaseData = seq;	// copy simple data members all at once
	// copy any other members used by export
	m_arrInitMidiEvent = seq.m_arrInitMidiEvent;
	m_arrRecordEvent.Attach(seq.m_arrRecordEvent.GetData(), seq.m_arrRecordEvent.GetSize());
	m_mapping.SetArray(seq.m_mapping.GetArray());
}

CSequencerReader::~CSequencerReader()
{
	SetMutes(m_arrTrackMute);	// restore track mutes
	CTrack	*pTrack;
	W64INT	nSize;
	Detach(pTrack, nSize);	// detach our track array from sequencer's
	CMidiEvent	*pEvent;
	m_arrRecordEvent.Detach(pEvent, nSize);
}

bool CSequencer::OutputLiveEvent(DWORD dwEvent)
{
	ASSERT(IsOpen());	// device must be open
	if (!IsOpen())
		return false;
	return MIDI_SUCCEEDED(midiOutShortMsg(HMIDIOUT(m_hStrm), dwEvent));
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

bool CSequencer::ConvertStringToPosition(const CString& sPos, LONGLONG& nPos) const
{
	LONGLONG	nFld[3];
	int	nFields = _stscanf_s(sPos, _T("%lld:%lld:%lld"), &nFld[0], &nFld[1], &nFld[2]);
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

void CSequencer::ConvertPositionToTimeString(LONGLONG nPos, CString& sTime) const
{
	LONGLONG	nTime = ConvertPositionToMilliseconds(nPos);
	static const LPCTSTR pszFormatNegative = _T("-%d:%02d:%02d.%03d");
	LPCTSTR	pszFormat;
	if (nTime < 0) {	// if negative time
		pszFormat = pszFormatNegative;
		nTime = -nTime;
	} else	// positive time
		pszFormat = pszFormatNegative + 1;	// skip minus sign
	int	nMillis = nTime % 1000;
	nTime /= 1000;
	int	nSecs = nTime % 60;
	nTime /= 60;
	int	nMins = nTime % 60;
	nTime /= 60;
	int	nHours = static_cast<int>(nTime);
	sTime.Format(pszFormat, nHours, nMins, nSecs, nMillis);
}

bool CSequencer::ConvertTimeStringToPosition(const CString& sPos, LONGLONG& nPos) const
{
	LONGLONG	nFld[4];	// hours, minutes, seconds, milliseconds
	int	nFields = _stscanf_s(sPos, _T("%lld:%lld:%lld.%lld"), &nFld[0], &nFld[1], &nFld[2], &nFld[3]);
	if (nFields <= 0)
		return false;
	bool	bHasLeadingSign = false;
	int	iChar = static_cast<int>(_tcsspn(sPos, _T(" ")));	// skip any leading spaces
	if (sPos[iChar] == '-') {	// if first non-space is sign 
		nFld[0] = -nFld[0];	// negate first field, as result will be negated
		bHasLeadingSign = true;
	} else	// leading sign not found
		bHasLeadingSign = false;
	static const int	nScale[4] = {60 * 60 * 1000, 60 * 1000, 1000, 1};
	LONGLONG	nMillis = 0;
	for (int iField = 0; iField < nFields; iField++)	// for each field
		nMillis += nFld[iField] * nScale[iField];	// scale field and accumulate
	nPos = ConvertMillisecondsToPosition(nMillis);
	if (bHasLeadingSign)	// if leading sign present
		nPos = -nPos;	// negate result
	return true;
}

void CSequencer::AddDubNow(int iTrack)
{
	LONGLONG	nPos;
	if (GetPosition(nPos))
		AddDub(iTrack, static_cast<int>(nPos));
}

void CSequencer::AddDubNow(const CIntArrayEx& arrSelection)
{
	LONGLONG	nPos;
	if (GetPosition(nPos))
		AddDub(arrSelection, static_cast<int>(nPos));
}

void CSequencer::AddDubNow()
{
	LONGLONG	nPos;
	if (GetPosition(nPos))
		AddDub(static_cast<int>(nPos));
}

void CSequencer::ChaseDubs(int nTime, bool bUpdateMutes)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks and record events
	CSeqTrackArray::ChaseDubs(nTime, bUpdateMutes);	// call base class
	m_iRecordEvent = m_arrRecordEvent.Chase(nTime - m_nRecordOffset);	// chase record events
}

void CSequencer::ChaseDubsFromCurPos()
{
	if (m_bIsSongMode) {
		WCritSec::Lock	lock(m_csTrack);	// serialize access to callback time
		ChaseDubs(m_nCBTime, true);	// chase to current callback time; set mutes
	}
}

void CSequencer::SetSongMode(bool bEnable, bool bChaseDubs)
{
	if (bChaseDubs && bEnable) {
		WCritSec::Lock	lock(m_csTrack);	// serialize access to song mode and callback time
		m_bIsSongMode = true;	// enable song mode
		ChaseDubs(m_nCBTime, true);	// chase to current callback time; set mutes
	} else
		m_bIsSongMode = bEnable;
}

void CSequencer::QueueOutputEvents(int nEvents)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to output event buffer
	CMidiEventStream&	arrEvt = m_arrMidiEvent[m_iBuffer];
	int	nOldSize = m_arrOutputEvent.GetSize();
	m_arrOutputEvent.FastSetSize(nOldSize + nEvents, m_nBufferSize);	// grow buffer
	for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each event
		CMidiEvent	evt(arrEvt[iEvent].dwDeltaTime, arrEvt[iEvent].dwEvent);
		m_arrOutputEvent[nOldSize + iEvent] = evt;
	}
}

void CSequencer::GetMidiOutputEvents(CMidiEventArray& arrEvent)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to output event buffer
	m_arrOutputEvent.Swap(arrEvent);	// swap event buffers with caller; swaps pointers only
	m_arrOutputEvent.FastRemoveAll();	// empty our new buffer but don't deallocate it
}

void CSequencer::FlushMidiOutputEvents()
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to output event buffer
	m_arrOutputEvent.FastRemoveAll();
}

void CSequencer::SetMidiOutputCapture(bool bEnable)
{
	if (bEnable)
		FlushMidiOutputEvents();
	m_bIsOutputCapture = bEnable;
}

void CSequencer::SetRecordedEvents(const CMidiEventArray& arrRecordEvent)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to record event buffer
	m_arrRecordEvent = arrRecordEvent;
}

void CSequencer::AttachRecordedEvents(CMidiEventArray& arrRecordEvent)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to record event buffer
	m_arrRecordEvent.Swap(arrRecordEvent);
}

void CSequencer::RemoveAllRecordedEvents()
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to record event buffer
	m_arrRecordEvent.RemoveAll();
}

void CSequencer::TruncateRecordedEvents(int nStartTime)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to record event buffer
	m_arrRecordEvent.TruncateEvents(nStartTime);
}

void CSequencer::DeleteRecordedEvents(int nStartTime, int nEndTime)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to record event buffer
	m_arrRecordEvent.DeleteEvents(nStartTime, nEndTime);
}

void CSequencer::AppendRecordedEvents(const CMidiEventArray& arrEvent)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to record event buffer
	m_arrRecordEvent.Append(arrEvent);
}

void CSequencer::InsertRecordedEvents(int nInsertTime, int nDuration, CMidiEventArray& arrEvent)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to record event buffer
	m_arrRecordEvent.InsertEvents(nInsertTime, nDuration, arrEvent);
}

void CSequencer::SetLoopRange(CLoopRange rngTicks)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to loop range
	m_rngLoop = rngTicks;
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
			if (MIDI_SHORT_MSG_CMD(evt.dwEvent) == NOTE_ON) {
				int	iChan = MIDI_CHAN(evt.dwEvent);
				int	iNote = MIDI_P1(evt.dwEvent);
				BYTE	nVel = MIDI_P2(evt.dwEvent);
				bool	bIsNoteOn = nVel != 0;
				bool	bIsPrevNoteOn = bNote[iChan][iNote] != 0;
				if (!(bIsNoteOn ^ bIsPrevNoteOn)) {
					sLine.Format(_T("ERROR collision %d %d\n"), iChan, iNote);
					fOut.WriteString(sLine);
					nErrors++;
				}
				bNote[iChan][iNote] = nVel;
			}
			nSum += evt.dwDeltaTime;
		}
		if (nSum != m_nCBLen) {
			sLine.Format(_T("ERROR duration %d != %d\n"), nSum, m_nCBLen);
			fOut.WriteString(sLine);
			nErrors++;
		}
		nTime += m_nCBLen;
	}
	if (nErrors)
		printf("ERRORS!\n");
}
#endif
