// Copyleft 2013 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      08oct13	initial version
        01      07may14	move generic functionality to base class
		02		09sep14	add CPatchObject
		03		02dec19	remove sort function, array now provides it
		04		19nov20	add randomized docking bar visibility

		automated undo test for patch editing
 
*/

#pragma once

#include "UndoTest.h"
#include "PolymeterDoc.h"
#include "MainFrm.h"

class CTrackUndoTest : public CUndoTest, public CTrackBase {
public:
// Construction
	CTrackUndoTest(bool InitRunning);
	virtual ~CTrackUndoTest();

protected:
// Types

// Constants
	static const EDIT_INFO	m_EditInfo[];	// array of edit properties

// Data members
	CPolymeterDoc	*m_pDoc;		// pointer to target document
	int		m_iNextTrack;			// next track number for renaming

// Overrides
	virtual	bool	Create();
	virtual	void	Destroy();
	virtual	int		ApplyEdit(int UndoCode);
	virtual	LONGLONG	GetSnapshot() const;

// Helpers
	int		GetRandomItem() const;
	int		GetRandomInsertPos() const;
	bool	MakeRandomSelection(int nItems, CIntArrayEx& arrSelection) const;
	void	MakeRandomTrackProperty(int iTrack, int iProp, CComVariant& var);
	bool	MakeRandomMasterProperty(int iProp, CComVariant& var);
	bool	GetRandomStep(CPoint& ptStep) const;
	bool	MakeRandomStepSelection(CRect& rSelection) const;
	int		MakeRandomMappingProperty(int iProp);
	void	RandomizeBarVisibility();
	CString	PrintSelection(CIntArrayEx& arrSelection) const;
	CString	PrintSelection(CRect& rSelection) const;
};
