// Copyleft 2013 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
		chris korda
 
		revision history:
		rev		date	comments
		00		23aug13	initial version
		01		07may15	in DumpAccidentalsTable, fix unused file pointer arg
		02		12may15	refactor MakeAccidentalsTable to specify flattest key
		03		25apr18	use safe file open methods
		04		10jul19	in ScanNoteName, normalize note after applying accidentals
		05		08jun21	fix warning for CString as variadic argument

		diatonic framework

*/

#include "stdafx.h"
#include "Diatonic.h"

const NOTE CDiatonic::m_NoteToDegree[NOTES] = {
//	C	Db	D	Eb	E	F	Gb	G	Ab	A	Bb	B
	D1,	D2,	D2,	D3,	D3,	D4,	D5,	D5,	D6,	D6,	D7,	D7
};

const bool CDiatonic::m_IsDiatonic[NOTES] = {
//	C	Db	D	Eb	E	F	Gb	G	Ab	A	Bb	B
	1,	0,	1,	0,	1,	1,	0,	1,	0,	1,	0,	1
};

const CDiatonic::HEPTATONIC CDiatonic::m_NaturalScale = {C, D, E, F, G, A, B};

#define HEPTATONICDEF(d1, d2, d3, d4, d5, d6, d7, name) {d1, d2, d3, d4, d5, d6, d7},
const CDiatonic::HEPTATONIC CDiatonic::m_Scale[SCALES] = {
	#include "HeptatonicDef.h"
};

#define HEPTATONICDEF(d1, d2, d3, d4, d5, d6, d7, name) _T(#name),
const LPCTSTR CDiatonic::m_ScaleName[SCALES] = {
	#include "HeptatonicDef.h"
};

#define MODEDEF(name) _T(#name),
const LPCTSTR CDiatonic::m_ModeName[MODES] = {
	#include "ModeDef.h"
};

CDiatonic::HEPTATONIC CDiatonic::m_AccidentalTbl[SCALES][KEYS];
int	CDiatonic::m_ScaleTonality[SCALES];

int CDiatonic::ScanNoteName(LPCTSTR Name, CNote& Note)
{
	int	iNote = toupper(Name[0]) - 'C';	// convert note letter to index
	if (iNote < 0)	// if negative
		iNote += DEGREES;	// wrap around
	if (iNote < 0 || iNote >= DEGREES)	// if index out of range
		return(0);	// fail
	CNote	note(m_NaturalScale.Note[iNote]);	// convert index to note
	int	iCh = 1;
	while (Name[iCh] == '#' || Name[iCh] == 'b') {
		if (Name[iCh] == '#') {	// if sharp
			note++;	// increment note
			if (note >= NOTES)	// if normalized range exceeded
				note = 0;	// wrap around to keep note normalized
		} else {	// else flat
			note--;	// decrement note
			if (note < 0)	// if normalized range exceeded
				note = NOTES - 1;	// wrap around to keep note normalized
		}
		iCh++;
	}
	Note = note;
	return(iCh);	// return number of characters read
}

bool CDiatonic::ParseMidiNoteName(LPCTSTR Name, CNote& Note)
{
	int	iCh = ScanNoteName(Name, Note);
	if (!iCh)
		return(FALSE);
	if (isdigit(Name[iCh]) || (Name[iCh] == '-' && isdigit(Name[iCh + 1]))) {
		int	octave = _ttoi(&Name[iCh]) + 1;
		Note += octave * NOTES;
		return(TRUE);
	}
	return(FALSE);
}

CString	CDiatonic::PrettyScaleName(int Scale)
{
	CString	s(ScaleName(Scale));
	s.MakeLower();
	s.Replace('_', ' ');
	return(s);
}

int CDiatonic::FindScale(LPCTSTR Name)
{
	for (int iScale = 0; iScale < SCALES; iScale++) {
		if (!_tcsicmp(m_ScaleName[iScale], Name))
			return(iScale);
	}
	return(-1);
}

int CDiatonic::FindMode(LPCTSTR Name)
{
	for (int iMode = 0; iMode < MODES; iMode++) {
		if (!_tcsicmp(m_ModeName[iMode], Name))
			return(iMode);
	}
	return(-1);
}

CNote CDiatonic::GetRootKey(CNote Key, int Scale, int Mode)
{
	ASSERT(IsValidScale(Scale));
	ASSERT(IsValidMode(Mode));
	return(CNote(Key - m_Scale[Scale].Note[Mode]).Normal());
}

void CDiatonic::GetAccidentals(CNote Key, int Scale, CScale& Accidentals)
{
	ASSERT(Key.IsNormal());
	ASSERT(IsValidScale(Scale));
	Accidentals.SetSize(DEGREES);
	const HEPTATONIC&	acc = m_AccidentalTbl[Scale][Key];
	for (int iDegree = 0; iDegree < DEGREES; iDegree++)	// for each degree
		Accidentals[iDegree] = acc.Note[iDegree];
}

CNote CDiatonic::AlterNote(CNote Note, CNote Key, int Scale)
{
	ASSERT(Key.IsNormal());
	ASSERT(IsValidScale(Scale));
	int	iDegree = m_NoteToDegree[Note.Normal()];
	ASSERT(IsValidDegree(iDegree));
	return(Note + m_AccidentalTbl[Scale][Key].Note[iDegree]);
}

CNote CDiatonic::AlterNote(CNote Note, const CScale& Accidentals)
{
	int	iDegree = m_NoteToDegree[Note.Normal()];
	ASSERT(IsValidDegree(iDegree));
	return(Note + Accidentals[iDegree]);
}

void CDiatonic::MakeAccidentalsTable()
{
	enum {	// flattest keys
		// to reduce the fifteen possible keys to only twelve, we must specify
		// the key at which the cycle of fifths switches from sharps to flats
		FK_Cb,	// sharpest key is E
		FK_Gb,	// sharpest key is B
		FK_Db,	// sharpest key is F#
		FK_Ab,	// sharpest key is C#
	};
	int	FlattestKey = FK_Gb;
	int	FirstKey = FlattestKey - Gb - 1;
	int	LastKey = FirstKey + KEYS - 1;
	for (int iScale = 0; iScale < SCALES; iScale++) {	// for each scale
		for (int iKey = FirstKey; iKey <= LastKey; iKey++) {	// for each key
			int	key = CNote(iKey * 7).Normal();
			HEPTATONIC&	KeyAcc = m_AccidentalTbl[iScale][key];
			for (int iDegree = 0; iDegree < DEGREES; iDegree++) {	// for each degree
				int	degree = CNote::Mod(iKey * 3 + iDegree, DEGREES);
				CNote	altered(m_Scale[iScale].Note[degree] + key);
				CNote	natural(m_NaturalScale.Note[iDegree]);
				int	acc = altered.Normal().LeastInterval(natural);
				KeyAcc.Note[iDegree] = acc;
			}
		}
		if (m_Scale[iScale].Note[D3] == Eb)	// if third degree is minor
			m_ScaleTonality[iScale] = CNote::MINOR;
		else	// not minor
			m_ScaleTonality[iScale] = CNote::MAJOR;
	}
}

bool CDiatonic::DumpAccidentalsTable(LPCTSTR Path)
{
	FILE	*fp;
	if (_tfopen_s(&fp, Path, _T("w")))
		return(FALSE);
	for (int iScale = 0; iScale < SCALES; iScale++) {	// for each scale
		// write starting parentheses with scale name comment
		_ftprintf(fp, _T("\t{\t// %s\n\t\t//"), PrettyScaleName(iScale).GetString());
		// write natural note names in column header comment
		for (int iDegree = 0; iDegree < DEGREES; iDegree++)	// for each degree
			_ftprintf(fp, _T("\t%s"), CNote(m_NaturalScale.Note[iDegree]).Name());
		fputc('\n', fp);
		for (int iKey = 0; iKey < KEYS; iKey++) {	// for each key
			_fputts(_T("\t\t{ "), fp);
			for (int iDegree = 0; iDegree < DEGREES; iDegree++) {	// for each degree
				_ftprintf(fp, _T("%3d"), m_AccidentalTbl[iScale][iKey].Note[iDegree]);
				if (iDegree < DEGREES - 1)	// if not last item
					fputc(',', fp);	// write separating comma
			}
			// write key name in end of row comment
			_ftprintf(fp, _T("\t},\t// %s\n"), CNote(iKey).Name());
		}
		_fputts(_T("\t},\n"), fp);
	}
	fclose(fp);
	return(TRUE);
}

void CDiatonic::GetScaleTones(CNote Key, int Scale, int Mode, CScale& Tones)
{
	ASSERT(Key.IsNormal());
	ASSERT(IsValidScale(Scale));
	ASSERT(IsValidMode(Mode));
	const NOTE	*scale = m_Scale[Scale].Note;
	int	Offset = scale[Mode];
	int	iSrcDeg = Mode;
	Tones.SetSize(DEGREES);
	for (int iDegree = 0; iDegree < DEGREES; iDegree++) {	// for each degree
		Tones[iDegree] = CNote(scale[iSrcDeg] - Offset + Key).Normal();
		iSrcDeg++;
		if (iSrcDeg >= DEGREES)
			iSrcDeg = 0;
	}
}

void CDiatonic::NoteNameTest(FILE *fp)
{
	for (int iTonality = 0; iTonality < CNote::TONALITIES; iTonality++) {	// for each tonality
		_ftprintf(fp, _T("%s\n"), CNote::TonalityName(iTonality));
		for (int iKey = 0; iKey < KEYS; iKey++) {	// for each key
			CNote	key(iKey * 7);	// cycle of fifths
			_ftprintf(fp, _T("%-2s: "), key.Name(key, iTonality));
			for (int iNote = 0; iNote < NOTES; iNote++)
				_ftprintf(fp, _T(" %-2s"), CNote(iNote).Name(key, iTonality));
			fputc('\n', fp);
		}
		fputc('\n', fp);
	}
}

void CDiatonic::AlterTest(FILE *fp)
{
	for (int iScale = 0; iScale < SCALES; iScale++) {	// for each scale
		int	tonality = m_ScaleTonality[iScale];
		_ftprintf(fp, _T("%s\n"), PrettyScaleName(iScale).GetString());
		for (int iKey = 0; iKey < KEYS; iKey++) {			// for each key
			CNote	key(iKey * 7);	// cycle of fifths
			_ftprintf(fp, _T("%-3s: "), key.Name(key, tonality));
			for (int iDegree = 0; iDegree < DEGREES; iDegree++) {	// for each degree
				CNote	note(m_NaturalScale.Note[iDegree] + OCTAVE);
				note = AlterNote(note, key.Normal(), iScale);
				_ftprintf(fp, _T(" %-3s"), note.Name(key, tonality));
			}
			fputc('\n', fp);
		}
		fputc('\n', fp);
	}
}

void CDiatonic::ModeTest(FILE *fp)
{
	for (int iScale = 0; iScale < SCALES; iScale++) {	// for each scale
		int	tonality = m_ScaleTonality[iScale];
		for (int iKey = 0; iKey < KEYS; iKey++) {	// for each key
			_ftprintf(fp, _T("%s %s\n"), 
				CNote(iKey).Name(iKey, tonality), PrettyScaleName(iScale).GetString());
			for (int iMode = 0; iMode < MODES; iMode++) {		// for each mode
				_ftprintf(fp, _T("%d:"), iMode + 1);
				CScale	Tones;
				GetScaleTones(iKey, iScale, iMode, Tones);
				int	RootKey = CDiatonic::GetRootKey(iKey, iScale, iMode);
				for (int iDegree = 0; iDegree < DEGREES; iDegree++) {	// for each degree
					CNote	note(Tones[iDegree]);
					_ftprintf(fp, _T(" %-3s"), note.Name(RootKey, tonality));
				}
				fputc('\n', fp);
			}
			fputc('\n', fp);
		}
	}
}

void CDiatonic::TestAll()
{
	FILE	*fp;
	if (!fopen_s(&fp, "test.txt", "w")) {
		NoteNameTest(fp);
		AlterTest(fp);
		ModeTest(fp);
		fclose(fp);
	}
}
