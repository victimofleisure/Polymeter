// Copyleft 2014 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      22apr14	initial version
		01		18may14	add m_PrevStartNote
		02		24aug15	add per-key colors
		03		21dec15	use extended string array
		04		10jan19	add no internal style and refactor names
		05		01feb19	add optional key press notifications

		piano control

*/

#if !defined(AFX_PIANOCTRL_H__8DA580FF_35B7_464A_BC56_4AF82D60B1FE__INCLUDED_)
#define AFX_PIANOCTRL_H__8DA580FF_35B7_464A_BC56_4AF82D60B1FE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PianoCtrl.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPianoCtrl window

#include "ArrayEx.h"

#define UWM_PIANOKEYCHANGE (WM_USER + 1917)	// wParam: key index, lParam: HWND
#define UWM_PIANOKEYPRESS (WM_USER + 1918)	// wParam: LOWORD key index, HIWORD bool enable, lParam: HWND

class CPianoCtrl : public CWnd
{
	DECLARE_DYNAMIC(CPianoCtrl)
// Construction
public:
	CPianoCtrl();
	BOOL	Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);

// Constants
	enum {	// piano styles
		PS_VERTICAL			= 0x0001,	// orient keyboard vertically
		PS_HIGHLIGHT_PRESS	= 0x0002,	// highlight pressed keys
		PS_USE_SHORTCUTS	= 0x0004,	// enable use of shortcut keys
		PS_SHOW_SHORTCUTS	= 0x0008,	// display shortcut assignments
		PS_ROTATE_LABELS	= 0x0010,	// rotate labels 90 degrees counter-clockwise;
										// only supported in horizontal orientation
		PS_PER_KEY_COLORS	= 0x0020,	// enable per-key color customization
		PS_INVERT_LABELS	= 0x0040,	// invert labels on pressed keys;
										// only supported on custom-colored keys
		PS_NO_INTERNAL		= 0x0080,	// disable internal key press behavior
		PS_NOTIFY_PRESS		= 0x0100,	// send key press notifications
	};
	enum {	// key sources
		KS_INTERNAL			= 0x01,		// key triggered internally
		KS_EXTERNAL			= 0x02,		// key triggered externally 
		KS_ALL				= KS_INTERNAL | KS_EXTERNAL,
	};

// Types

// Attributes
public:
	void	SetStyle(DWORD dwStyle, bool bEnable);
	void	SetRange(int nStartNote, int nKeys);
	int		GetStartNote() const;
	int		GetKeyCount() const;
	void	GetKeyRect(int iKey, CRect& rKey) const;
	bool	IsBlack(int iKey) const;
	bool	IsPressed(int iKey) const;
	void	SetPressed(int iKey, bool bEnable, bool bExternal = FALSE);
	CSize	GetBlackKeySize() const;
	int		GetKeyLabelCount() const;
	CString	GetKeyLabel(int iKey) const;
	void	SetKeyLabel(int iKey, const CString& sLabel);
	void	GetKeyLabels(CStringArrayEx& arrLabel) const;
	void	SetKeyLabels(const CStringArrayEx& arrLabel);
	bool	IsShorcutScanCode(int nScanCode) const;
	int		GetKeyColor(int iKey) const;
	void	SetKeyColor(int iKey, int nColor, bool bRepaint = TRUE);

// Operations
public:
	void	InvalidateKey(int iKey);
	int		FindKey(CPoint pt) const;
	void	Update(CSize szClient);
	void	Update();
	void	ReleaseKeys(UINT nKeySourceMask);
	void	RemoveKeyLabels();
	void	RemoveKeyColors();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPianoCtrl)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CPianoCtrl();

	// Generated message map functions
protected:
	//{{AFX_MSG(CPianoCtrl)
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg LRESULT OnSetFont(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnGetFont(WPARAM wParam, LPARAM lParam);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

// Types
	struct KEY_INFO {	// information about each key within the octave
		int		nWhiteIndex;	// index of nearest white key; range is [0..6]
		int		nBlackOffset;	// if black key, its offset from C in Savard units
	};
	class CKey : public WObject {
	public:
		CKey();
		CKey&	CKey::operator=(const CKey& key);
		CRgn	m_rgn;			// key's polygonal area
		bool	m_bIsBlack;		// true if key is black
		bool	m_bIsPressed;	// true if key is pressed
		bool	m_bIsExternal;	// true if external key press
		int		m_nColor;		// per-key color as RGB, or -1 if none
	};
	typedef CArrayEx<CKey, CKey&> CKeyArray;

// Constants
	enum {	// dimensions from "The Size of the Piano Keyboard" by John J. G. Savard
		WHITE_WIDTH = 24,		// width of white key, in Savard units
		BLACK_WIDTH = 14,		// width of black key, in Savard units
		C_SHARP_OFFSET = 15,	// offset of C# key from C key, in Savard units
		F_SHARP_OFFSET = 13,	// offset of F# key from F key, in Savard units
	};
	enum {
		NOTES = 12,				// semitones per octave
		WHITES = 7,				// number of white keys per octave
		MIN_FONT_HEIGHT = 1,	// minimum key label font height, in pixels
		MAX_FONT_HEIGHT = 100,	// maximum key label font height, in pixels
		OUTLINE_WIDTH = 1,		// width of key outline, in pixels
	};
	enum {	// key types
		WHITE,
		BLACK,
		KEY_TYPES
	};
	enum {	// key states
		UP,
		DOWN,
		KEY_STATES
	};
	static const double m_fBlackHeightRatio;	// ratio of black height to white height
	static const KEY_INFO	m_arrKeyInfo[NOTES];	// static key information
	static const COLORREF	m_arrKeyColor[KEY_TYPES][KEY_STATES];	// key color matrix
	static const COLORREF	m_clrOutline;	// color of outlines
	static const char m_arrScanCode[];	// array of shortcut scan codes

// Member data
	HFONT	m_fontWnd;			// window font handle, or NULL for key label font
	CFont	m_fontKeyLabel;		// auto-scaled key label font; used if m_Font is NULL
	int		m_nStartNote;		// MIDI note number of keyboard's first note
	int		m_nPrevStartNote;	// previous start note, for detecting changes
	int		m_nKeys;			// total number of keys on keyboard
	int		m_iCurKey;			// index of current key, or -1 if none
	CSize	m_szBlackKey;		// black key dimensions, in pixels
	CKeyArray	m_arrKey;		// dynamic information about each key
	CBrush	m_arrKeyBrush[KEY_TYPES][KEY_STATES];	// brush for each key type and state
	CBrush	m_brOutline;		// brush for outlining keys
	CStringArrayEx	m_arrKeyLabel;	// labels to be displayed on keys
	char	m_arrScanCodeToKey[0x80];	// map scan codes to keys

// Helpers
	void	OnKeyChange(UINT nKeyFlags, bool bIsDown);
	void	UpdateKeyLabelFont(CSize szClient, DWORD dwStyle);
	void	UpdateKeyLabelFont();
};

inline int CPianoCtrl::GetStartNote() const
{
	return(m_nStartNote);
}

inline int CPianoCtrl::GetKeyCount() const
{
	return(m_nKeys);
}

inline void CPianoCtrl::GetKeyRect(int iKey, CRect& rKey) const
{
	m_arrKey[iKey].m_rgn.GetRgnBox(rKey);
}

inline bool CPianoCtrl::IsBlack(int iKey) const
{
	return(m_arrKey[iKey].m_bIsBlack);
}

inline bool CPianoCtrl::IsPressed(int iKey) const
{
	return(m_arrKey[iKey].m_bIsPressed);
}

inline CSize CPianoCtrl::GetBlackKeySize() const
{
	return(m_szBlackKey);
}

inline int CPianoCtrl::GetKeyLabelCount() const
{
	return(m_arrKeyLabel.GetSize());
}

inline void CPianoCtrl::GetKeyLabels(CStringArrayEx& arrLabel) const
{
	arrLabel = m_arrKeyLabel;
}

inline CString CPianoCtrl::GetKeyLabel(int iKey) const
{
	return(m_arrKeyLabel[iKey]);
}

inline int CPianoCtrl::GetKeyColor(int iKey) const
{
	return(m_arrKey[iKey].m_nColor);
}

inline void CPianoCtrl::SetKeyColor(int iKey, int nColor, bool bRepaint)
{
	m_arrKey[iKey].m_nColor = nColor;
	if (bRepaint)
		InvalidateKey(iKey);
}

inline void CPianoCtrl::InvalidateKey(int iKey)
{
	InvalidateRgn(&m_arrKey[iKey].m_rgn);
}

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PIANOCTRL_H__8DA580FF_35B7_464A_BC56_4AF82D60B1FE__INCLUDED_)
