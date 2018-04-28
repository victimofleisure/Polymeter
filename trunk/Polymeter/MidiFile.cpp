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
		
		MIDI file I/O
 
*/

#include "stdafx.h"
#include "MidiFile.h"
#include "Midi.h"
#include "mmsystem.h"	// for MAKEFOURCC
#include "atlconv.h"	// for Unicode conversion

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

void CMidiFile::Reverse(void *pDst, void *pSrc, UINT Len)
{
	BYTE	*pByteDst = static_cast<BYTE *>(pDst);
	const BYTE	*pByteSrc = static_cast<BYTE *>(pSrc);
	for (UINT i = 0; i < Len; i++)
		pByteDst[Len - 1 - i] = pByteSrc[i];
}

void CMidiFile::WriteVarLen(UINT Val)
{
	const int BUF_SIZE = 4;	// maximum number of output bytes
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

void CMidiFile::WriteMeta(BYTE Type, UINT Len)
{
	WriteByte(0);	// write zero delta time; required
	WriteByte(META_EVENT);	// write meta event pseudo-status
	WriteByte(Type);	// write meta event type
	WriteVarLen(Len);	// write meta event data size
}

CMidiFile::FILE_POS CMidiFile::BeginTrack()
{
	UINT	ChunkID = MAKEFOURCC('M', 'T', 'r', 'k');	// track chunk ID
	Write(&ChunkID, sizeof(ChunkID));	// write chunk ID
	WriteInt(0);	// write dummy chunk size
	return(GetPosition());	// return file position
}

void CMidiFile::EndTrack(FILE_POS StartPos)
{
	WriteMeta(ME_END_TRACK, 0);	// not optional
	FILE_POS	EndPos = GetPosition();	// get file position at end of chunk
	int	ChunkSize = static_cast<UINT>(EndPos - StartPos);	// compute chunk size
	int	ChunkSizeOffset = ChunkSize + sizeof(int);	// include size in offset
	Seek(-ChunkSizeOffset, CFile::current);	// rewind back to chunk size
	WriteInt(ChunkSize);	// overwrite dummy chunk size with actual size
	Seek(ChunkSize, CFile::current);	// restore file position
}

void CMidiFile::WriteHeader(USHORT nTracks, USHORT nPPQ, double fTempo, const TIME_SIGNATURE* pTimeSig, const KEY_SIGNATURE* pKeySig)
{
	// write file header
	UINT	ChunkID = MAKEFOURCC('M', 'T', 'h', 'd');	// header chunk ID
	Write(&ChunkID, sizeof(ChunkID));	// write chunk ID
	WriteInt(FILE_HEADER_LEN);	// write chunk size
	WriteShort(MF_MULTI);	// write MIDI file format
	WriteShort(nTracks + 1);	// write track count; one extra for song track
	WriteShort(nPPQ);	// write pulses per quarter note
	// write song track
	FILE_POS	StartPos = BeginTrack();	// write track header
	if (fTempo > 0) {	// if tempo specified
		WriteMeta(ME_SET_TEMPO, TEMPO_LEN);	// write meta header
		int	mspq = round(60000000 / fTempo);	 // microseconds per quarter note
		BYTE	TempoBuf[TEMPO_LEN];
		Reverse(TempoBuf, &mspq, TEMPO_LEN);	// convert tempo to big endian
		Write(TempoBuf, TEMPO_LEN);	// write tempo
	}
	if (pTimeSig != NULL) {	// if time signature specified
		WriteMeta(ME_TIME_SIGNATURE, TIME_SIG_LEN);	// write meta header
		Write(pTimeSig, TIME_SIG_LEN);	// write time signature
	}
	if (pKeySig != NULL) {	// if key signature specified
		WriteMeta(ME_KEY_SIGNATURE, KEY_SIG_LEN);	// write meta header
		Write(pKeySig, KEY_SIG_LEN);	// write key signature
	}
	EndTrack(StartPos);	// finish track and fix header
}

void CMidiFile::WriteTrack(const CMidiEventArray& arrEvent, LPCTSTR pszName)
{
	static bool	bHasP2[8] = {1, 1, 1, 1, 0, 0, 1, 1};
	FILE_POS	StartPos = BeginTrack();	// write track header
	USES_CONVERSION;
	if (pszName != NULL) {	// if track name specified
		int	len = UINT64TO32(_tcslen(pszName));
		if (len) {	// if track name has non-zero length
			WriteMeta(ME_TRACK_NAME, len);	// write meta header
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
		if (bHasP2[(nStatus >> 4) - 8])	// if message has 2nd parameter
			WriteByte(MIDI_P2(ev.Msg));	// write message 2nd parameter
	}
	EndTrack(StartPos);	// finish track and fix header
}
