// Copyleft 2014 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		30aug99 initial version
		01		25apr02 set key signature
        02      07mar14	convert to MFC
        03      25apr18	standardize names
        04      10oct18 add read
		05		11feb19	add tempo and key and time signatures to read
		06		09sep19	add duration and tempo map to write
		07		07jun21	rename rounding functions
		08		08jun21	fix local name reuse warning
		09		15nov21	in read, merge track name case with preceding switch
		10		15nov21	in read, set master tempo from first tempo event only
		11		15nov21	add tempo map to read

		MIDI file I/O
 
*/

#include "stdafx.h"
#include "MidiFile.h"
#include "Midi.h"
#include "mmsystem.h"	// for MAKEFOURCC
#include "atlconv.h"	// for Unicode conversion

const UINT CMidiFile::m_nHeaderChunkID = MAKEFOURCC('M', 'T', 'h', 'd');
const UINT CMidiFile::m_nTrackChunkID = MAKEFOURCC('M', 'T', 'r', 'k');
const bool CMidiFile::m_bHasP2[8] = {1, 1, 1, 1, 0, 0, 1, 1};

void CMidiFile::WriteByte(BYTE Val)
{
	Write(&Val, sizeof(Val));
}

void CMidiFile::WriteShort(short Val)
{
	Reverse(Val);
	Write(&Val, sizeof(Val));
}

void CMidiFile::WriteInt(int Val)
{
	Reverse(Val);
	Write(&Val, sizeof(Val));
}

void CMidiFile::ReadCheck(void* lpBuf, UINT nCount)
{
	if (Read(lpBuf, nCount) != nCount)	// if requested bytes weren't obtained
		AfxThrowFileException(CFileException::endOfFile);
}

void CMidiFile::ReadByte(BYTE& Val)
{
	ReadCheck(&Val, sizeof(Val));
}

void CMidiFile::ReadShort(short& Val)
{
	ReadCheck(&Val, sizeof(Val));
	Reverse(Val);
}

void CMidiFile::ReadShort(USHORT& Val)
{
	ReadCheck(&Val, sizeof(Val));
	Reverse(Val);
}

void CMidiFile::ReadInt(int& Val)
{
	ReadCheck(&Val, sizeof(Val));
	Reverse(Val);
}

void CMidiFile::ReadInt(UINT& Val)
{
	ReadCheck(&Val, sizeof(Val));
	Reverse(Val);
}

void CMidiFile::Reverse(void *pDst, void *pSrc, UINT Len)
{
	BYTE	*pByteDst = static_cast<BYTE *>(pDst);
	const BYTE	*pByteSrc = static_cast<BYTE *>(pSrc);
	for (UINT i = 0; i < Len; i++)
		pByteDst[Len - 1 - i] = pByteSrc[i];
}

void CMidiFile::WriteVarLen(UINT Val)
{
	const int BUF_SIZE = sizeof(UINT);	// maximum number of output bytes
	ASSERT(Val < (1 << (BUF_SIZE * 7)));	// avoid buffer overrun
	BYTE	buf[BUF_SIZE];
	buf[BUF_SIZE - 1] = Val & 0x7f;
	int	len = 1;
	while (Val >>= 7) {	// while more significant input bits
		len++;
		buf[BUF_SIZE - len] = (Val & 0x7f) | 0x80;
	}
	Write(&buf[BUF_SIZE - len], len);	// output is big endian
}

void CMidiFile::ReadVarLen(UINT& Val)
{
	const int BUF_SIZE = sizeof(UINT);	// maximum number of output bytes
	Val = 0;
	BYTE	b;
	int	nBytes = 0;
	while (nBytes < BUF_SIZE) {
		ReadCheck(&b, sizeof(b));
		Val |= b & 0x7f;
		if (!(b & 0x80))
			break;
		Val <<= 7;
	}
}

void CMidiFile::WriteMeta(UINT DeltaT, BYTE Type, UINT Len)
{
	WriteVarLen(DeltaT);	// write delta time
	WriteByte(META_EVENT);	// write meta event pseudo-status
	WriteByte(Type);	// write meta event type
	WriteVarLen(Len);	// write meta event data size
}

void CMidiFile::WriteTempo(UINT DeltaT, UINT Tempo)
{
	WriteMeta(DeltaT, ME_SET_TEMPO, TEMPO_LEN);	// write meta header
	BYTE	TempoBuf[TEMPO_LEN];
	Reverse(TempoBuf, &Tempo, TEMPO_LEN);	// convert tempo to big endian
	Write(TempoBuf, TEMPO_LEN);	// write tempo
}

CMidiFile::FILE_POS CMidiFile::BeginTrack()
{
	Write(&m_nTrackChunkID, sizeof(m_nTrackChunkID));	// write chunk ID
	WriteInt(0);	// write dummy chunk size
	return(GetPosition());	// return file position
}

void CMidiFile::EndTrack(FILE_POS StartPos)
{
	WriteMeta(0, ME_END_TRACK, 0);	// not optional
	FILE_POS	EndPos = GetPosition();	// get file position at end of chunk
	int	ChunkSize = static_cast<UINT>(EndPos - StartPos);	// compute chunk size
	int	ChunkSizeOffset = ChunkSize + sizeof(int);	// include size in offset
	Seek(-ChunkSizeOffset, CFile::current);	// rewind back to chunk size
	WriteInt(ChunkSize);	// overwrite dummy chunk size with actual size
	Seek(ChunkSize, CFile::current);	// restore file position
}

void CMidiFile::WriteHeader(USHORT nTracks, USHORT nPPQ, double fTempo, UINT nDurationTicks, const TIME_SIGNATURE* pTimeSig, const KEY_SIGNATURE* pKeySig, const CMidiEventArray* parrTempoMap)
{
	// write file header
	Write(&m_nHeaderChunkID, sizeof(m_nHeaderChunkID));	// write chunk ID
	WriteInt(FILE_HEADER_LEN);	// write chunk size
	WriteShort(MF_MULTI);	// write MIDI file format
	WriteShort(nTracks + 1);	// write track count; one extra for song track
	WriteShort(nPPQ);	// write pulses per quarter note
	// write song track
	FILE_POS	StartPos = BeginTrack();	// write track header
	int	uspqn = 0;
	if (fTempo > 0) {	// if tempo specified
		uspqn = Round(MICROS_PER_MINUTE / fTempo);	 // microseconds per quarter note
		WriteTempo(0, uspqn);	// write tempo
	}
	if (pTimeSig != NULL) {	// if time signature specified
		WriteMeta(0, ME_TIME_SIGNATURE, TIME_SIG_LEN);	// write meta header
		Write(pTimeSig, TIME_SIG_LEN);	// write time signature
	}
	if (pKeySig != NULL) {	// if key signature specified
		WriteMeta(0, ME_KEY_SIGNATURE, KEY_SIG_LEN);	// write meta header
		Write(pKeySig, KEY_SIG_LEN);	// write key signature
	}
	UINT	nLastTempoEvtTime = 0;
	if (parrTempoMap != NULL && parrTempoMap->GetSize()) {	// if tempo map specified
		int	nEvents = parrTempoMap->GetSize();
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each tempo map event
			const MIDI_EVENT&	midiEvt = (*parrTempoMap)[iEvent];
			WriteTempo(midiEvt.DeltaT, midiEvt.Msg);
			nLastTempoEvtTime += midiEvt.DeltaT;	// accumulate duration
		}
		uspqn = (*parrTempoMap)[nEvents - 1].Msg;	// store last tempo
	}
	if (nDurationTicks > nLastTempoEvtTime && uspqn)
		WriteTempo(nDurationTicks - nLastTempoEvtTime, uspqn);	// pad track to duration
	EndTrack(StartPos);	// finish track and fix header
}

void CMidiFile::WriteTrack(const CMidiEventArray& arrEvent, LPCTSTR pszName)
{
	FILE_POS	StartPos = BeginTrack();	// write track header
	USES_CONVERSION;
	if (pszName != NULL) {	// if track name specified
		int	len = UINT64TO32(_tcslen(pszName));
		if (len) {	// if track name has non-zero length
			WriteMeta(0, ME_TRACK_NAME, len);	// write meta header
			Write(T2CA(pszName), len);	// write track name
		}
	}
	BYTE	nRunningStatus = 0;	// init running status
	int	nEvents = arrEvent.GetSize();
	for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each event
		const MIDI_EVENT&	ev = arrEvent[iEvent];
		WriteVarLen(ev.DeltaT);	// write variable-length delta time
		BYTE	nStatus = MIDI_STAT(ev.Msg);	// get message status
		if (nStatus != nRunningStatus) {	// if status changed
			WriteByte(nStatus);
			nRunningStatus = nStatus;
		}
		WriteByte(MIDI_P1(ev.Msg));	// write message 1st parameter
		if (m_bHasP2[(nStatus >> 4) - 8])	// if message has 2nd parameter
			WriteByte(MIDI_P2(ev.Msg));	// write message 2nd parameter
	}
	EndTrack(StartPos);	// finish track and fix header
}

void CMidiFile::ReadTracks(CMidiTrackArray& arrTrack, CStringArrayEx& arrTrackName, USHORT& nPPQ, UINT *pnTempo, TIME_SIGNATURE* pTimeSig, KEY_SIGNATURE* pKeySig, CMidiEventArray* parrTempoMap)
{
	bool	bGotTempo = false;
	bool	bGotTimeSig = false;
	bool	bGotKeySig = false;
	if (pnTempo != NULL)	// if tempo requested
		*pnTempo = 0;	// init destination before anything throws
	if (pTimeSig != NULL)	// if time signature requested
		ZeroMemory(pTimeSig, sizeof(TIME_SIGNATURE));
	if (pKeySig != NULL)	// if key signature requested
		ZeroMemory(pKeySig, sizeof(KEY_SIGNATURE));
	if (parrTempoMap != NULL)
		parrTempoMap->RemoveAll();
	UINT	nChunkID;
	ReadCheck(&nChunkID, sizeof(nChunkID));	// read chunk ID
	if (nChunkID != m_nHeaderChunkID)	// if not header chunk
		AfxThrowFileException(CFileException::genericException);
	UINT	nChunkSize;
	ReadInt(nChunkSize);	// read chunk size
	USHORT	nFormat;
	ReadShort(nFormat);	// read MIDI file format
	if (nFormat > MF_ASYNC)	// if unknown MIDI file format
		AfxThrowFileException(CFileException::genericException);
	USHORT	nTracks;
	ReadShort(nTracks);	// read track count
	ReadShort(nPPQ);	// read pulses per quarter note
	CByteArrayEx	arrByte;
	if (nChunkSize > FILE_HEADER_LEN) {	// if header longer than expected
		UINT	nExtraLen = nChunkSize - FILE_HEADER_LEN;
		arrByte.SetSize(nExtraLen);
		ReadCheck(arrByte.GetData(), nExtraLen);	// ignore extra header bytes
	}
	arrTrack.SetSize(nTracks);	// allocate destination arrays
	arrTrackName.SetSize(nTracks);
	int	iTrack = 0;
	while (Read(&nChunkID, sizeof(nChunkID)) == sizeof(nChunkID)) {	// read chunk ID
		ReadInt(nChunkSize);	// read chunk size
		if (nChunkID == m_nTrackChunkID) {	// if track chunk
			BYTE	nRunningStatus = 0;	// init running status
			BYTE	p1 = 0, p2 = 0;
			while (1) {	// track processing loop, terminated by end track meta event
				BYTE	nStatus;
				UINT	nDeltaT;
				ReadVarLen(nDeltaT);	// read delta time
				ReadByte(nStatus);	// read status or data byte
				if (nStatus == META_EVENT) {	// if meta event
					nRunningStatus = 0;	// cancel running status
					BYTE	nEventType;
					UINT	nEventLen;
					ReadByte(nEventType);	// read meta event type
					ReadVarLen(nEventLen);	// read meta event data length
					arrByte.SetSize(nEventLen);	// allocate buffer for meta event
					ReadCheck(arrByte.GetData(), nEventLen);	// read meta event
					if (nEventType == ME_END_TRACK) {	// if end of track
						iTrack++;	// increment track index
						break;	// exit track processing loop
					}
					switch (nEventType) {
					case ME_SET_TEMPO:
						if (!bGotTempo && pnTempo != NULL) {	// if tempo unread and requested
							Reverse(pnTempo, arrByte.GetData(), TEMPO_LEN);	// convert tempo to little endian
							bGotTempo = true;	// mark tempo read
						}
						if (parrTempoMap != NULL) {	// if tempo map requested
							// tempo conversion only sets three bytes, so we must zero high byte
							MIDI_EVENT	evt = {nDeltaT};	// also initializes Msg member to zero
							Reverse(&evt.Msg, arrByte.GetData(), TEMPO_LEN);	// convert tempo to little endian
							parrTempoMap->Add(evt);	// add event to caller's tempo map
						}
						break;
					case ME_TIME_SIGNATURE:
						if (!bGotTimeSig && pTimeSig != NULL) {	// if time signature unread and requested
							*pTimeSig = *reinterpret_cast<TIME_SIGNATURE*>(arrByte.GetData());
							bGotTimeSig = true;	// mark time signature read
						}
						break;
					case ME_KEY_SIGNATURE:
						if (!bGotKeySig && pKeySig != NULL) {	// if key signature unread and requested
							*pKeySig = *reinterpret_cast<KEY_SIGNATURE*>(arrByte.GetData());
							bGotKeySig = true;	// mark key signature read
						}
						break;
					case ME_TRACK_NAME:
						arrTrackName[iTrack] = CString(reinterpret_cast<char *>(arrByte.GetData()), nEventLen);
						break;
					}
				} else {	// normal event
					if (nStatus & 0x80) {	// if status byte
						nRunningStatus = nStatus;	// update running status
						ReadByte(p1);	// read first parameter
					} else {	// data byte
						p1 = nStatus;	// nStatus actually contains first parameter
						nStatus = nRunningStatus;	// use running status
					}
					if (m_bHasP2[(nStatus >> 4) - 8])	// if message has second parameter
						ReadByte(p2);	// read message second parameter
					else	// no second parameter
						p2 = 0;	// zero second parameter
					if (nStatus >= 0xf0) {	// if system message
						if (nStatus == 0xf0 || nStatus == 0xf7) {	// if system exclusive message
							nRunningStatus = 0;	// cancel running status
							UINT	nSysexLen;
							ReadVarLen(nSysexLen);	// read sysex data length
							arrByte.SetSize(nSysexLen);	// allocate buffer for sysex data
							ReadCheck(arrByte.GetData(), nSysexLen);	// read sysex data and discard it
						} else	// other system status
							AfxThrowFileException(CFileException::genericException);	// not supported
					}
					MIDI_EVENT	evt;
					evt.DeltaT = nDeltaT;
					evt.Msg = nStatus | (p1 << 8) | (p2 << 16);
					arrTrack[iTrack].Add(evt);	// add event to track's event array
				}
			}
		} else {	// not track chunk; unexpected chunk ID
			arrByte.SetSize(nChunkSize);	// allocate buffer for chunk
			ReadCheck(arrByte.GetData(), nChunkSize);	// read chunk and discard it
		}
	}
}
