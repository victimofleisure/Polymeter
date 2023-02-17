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
		02		06nov20	add replace page; add load/store state
		03		17feb23	add replace range to velocity transform

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

class CVelocityReplacePage : public CPropertyPage
{
	DECLARE_DYNAMIC(CVelocityReplacePage)

public:
	CVelocityReplacePage();
	virtual ~CVelocityReplacePage();

// Dialog Data
	enum { IDD = IDD_VELOCITY_REPLACE };
	CVelocitySheet	*m_pSheet;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual	BOOL OnInitDialog();
	void	SetRangeMode(bool bEnable);

	bool	m_bIsRangeMode;		// true if controls are in range mode
	void	SwapCtrlPositions(int nID1, int nID2);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnReplaceRange();
};

class CVelocityTransform {
public:
	CVelocityTransform();
	bool	IsIdentity() const;
	enum {	// define types
		TYPE_OFFSET,
		TYPE_SCALE,
		TYPE_REPLACE,
		TYPES
	};
	enum {	// define targets
		TARGET_TRACKS,
		TARGET_STEPS,
		TARGETS
	};
	int		m_iType;				// transformation type; see enum above
	int		m_nOffset;				// velocity offset; applicable to tracks or their steps
	int		m_nTarget;				// target entities; see enum above
	double	m_fScale;				// velocity multiplier 
	int		m_bSigned;				// if non-zero, treat velocities as signed values
	int		m_nFindWhat;			// velocity value to find, or starting value if range enabled
	int		m_nReplaceWith;			// replacement velocity value
	int		m_nFindEnd;				// if range enabled, ending velocity value to find
	int		m_nMatches;				// match count for find/replace
	bool	m_bHaveStepSelection;	// if true, disable target selection
	bool	m_bIsFindRange;			// if true, find range of velocities
};

class CVelocitySheet : public CPropertySheet, public CVelocityTransform
{
	DECLARE_DYNAMIC(CVelocitySheet)

public:
	CVelocitySheet(UINT nIDCaption = IDS_VELOCITY, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	virtual ~CVelocitySheet();
	void	LoadState();
	void	StoreState();

	CVelocityOffsetPage	m_pageOffset;
	CVelocityScalePage	m_pageScale;
	CVelocityReplacePage	m_pageReplace;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
};
