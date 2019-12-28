// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      21may18	initial version
		01		15nov19	add option for signed velocity scaling

*/

#pragma once

#include "Range.h"

class CVelocitySheet;

class CVelocityOffsetPage : public CPropertyPage
{
	DECLARE_DYNAMIC(CVelocityOffsetPage)

public:
	CVelocityOffsetPage();
	virtual ~CVelocityOffsetPage();

// Dialog Data
	enum { IDD = IDD_VELOCITY_OFFSET };
	CVelocitySheet	*m_pSheet;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual	BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
};

class CVelocityScalePage : public CPropertyPage
{
	DECLARE_DYNAMIC(CVelocityScalePage)

public:
	CVelocityScalePage();
	virtual ~CVelocityScalePage();

// Dialog Data
	enum { IDD = IDD_VELOCITY_SCALE };
	CVelocitySheet	*m_pSheet;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
};

class CVelocitySheet : public CPropertySheet
{
	DECLARE_DYNAMIC(CVelocitySheet)

public:
	CVelocitySheet(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	virtual ~CVelocitySheet();

	CVelocityOffsetPage	m_pageOffset;
	CVelocityScalePage	m_pageScale;

	enum {	// define pages
		PAGE_OFFSET,
		PAGE_SCALE,
		PAGES
	};
	enum {	// define targets
		TARGET_TRACKS,
		TARGET_STEPS,
		TARGETS
	};
	int		m_nOffset;				// offset in MIDI steps
	int		m_nTarget;				// target entities; see enum above
	double	m_fScale;				// multiplier
	int		m_nSigned;				// if non-zero, scaling treats velocities as signed values
	bool	m_bHaveStepSelection;	// if true, disable target selection

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
};
