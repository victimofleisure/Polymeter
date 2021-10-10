// Copyleft 2014 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      22apr14	initial version
		01		18may14	in Update, preserve key states
		02		24aug15	add per-key colors
		03		21dec15	use extended string array
		04		08jan19	fix unreferenced parameter warnings
		05		09jan19	lighten pressed black key color
		06		10jan19	add no internal style and refactor names
		07		01feb19	add key press notification
		08		07jun21	rename rounding functions
		09		08jun21	fix local name reuse warning

		piano control

*/

// PianoCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "Resource.h"
#include "PianoCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPianoCtrl

IMPLEMENT_DYNAMIC(CPianoCtrl, CWnd)

const double CPianoCtrl::m_fBlackHeightRatio = 2.0 / 3.0;

const CPianoCtrl::KEY_INFO CPianoCtrl::m_arrKeyInfo[NOTES] = {
	{0,	0},	// C
	{1,	C_SHARP_OFFSET},	// C#
	{1, 0},	// D
	{2,	C_SHARP_OFFSET + BLACK_WIDTH * 2},	// D#
	{2, 0},	// E
	{3, 0},	// F
	{4,	C_SHARP_OFFSET * 2 + F_SHARP_OFFSET + BLACK_WIDTH * 3},	// F#
	{4, 0},	// G
	{5,	C_SHARP_OFFSET * 2 + F_SHARP_OFFSET + BLACK_WIDTH * 5},	// G#
	{5, 0},	// A
	{6,	C_SHARP_OFFSET * 2 + F_SHARP_OFFSET + BLACK_WIDTH * 7},	// A#
	{6,	0},	// B
};

const COLORREF CPianoCtrl::m_arrKeyColor[KEY_TYPES][KEY_STATES] = {
	//	UP					DOWN
	{	RGB(255, 255, 255),	RGB(  0, 255, 255)	},	// WHITE
	{	RGB(  0,   0,   0),	RGB( 32,  32, 255)	},	// BLACK
};
const COLORREF CPianoCtrl::m_clrOutline = RGB(0, 0, 0);

// shortcut scan codes are grouped into four rows, as follows:
// 1st row, normally starting with '1': upper black keys
// 2nd row, normally starting with 'Q': upper white keys
// 3rd row, normally starting with 'A': lower black keys
// 4th row, normally starting with 'Z': lower white keys
// white and black keys are interleaved in scan code array, starting with
// black key, so even array elements are black and odd elements are white
const char CPianoCtrl::m_arrScanCode[] = {
//	3rd		4th
	0x1E,	0x2C,	// first two lower keys: black, white
	0x1F,	0x2D,
	0x20,	0x2E,
	0x21,	0x2F,
	0x22,	0x30,
	0x23,	0x31,
	0x24,	0x32,
	0x25,	0x33,
	0x26,	0x34,
	0x27,	0x35,
//	1st		2nd
	0x02,	0x10,	// first two upper keys: black, white
	0x03,	0x11,
	0x04,	0x12,
	0x05,	0x13,
	0x06,	0x14,
	0x07,	0x15,
	0x08,	0x16,
	0x09,	0x17,
	0x0A,	0x18,
	0x0B,	0x19,
	0x0C,	0x1A,
	0x0D,	0x1B,
};

CPianoCtrl::CPianoCtrl()
{
	m_fontWnd = NULL;
	m_nStartNote = 0;
	m_nPrevStartNote = 0;
	m_nKeys = 12;
	m_iCurKey = -1;
	m_szBlackKey = CSize(0, 0);
}

CPianoCtrl::~CPianoCtrl()
{
}

BOOL CPianoCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	LPCTSTR	lpszClassName = AfxRegisterWndClass(
		CS_HREDRAW | CS_VREDRAW, 
		LoadCursor(NULL, IDC_ARROW));
	if (!CWnd::Create(lpszClassName, NULL, dwStyle, rect, pParentWnd, nID))
		return FALSE;
	return TRUE;
}

int CPianoCtrl::FindKey(CPoint pt) const
{
	int	nKeys = GetKeyCount();
	for (int iKey = 0; iKey < nKeys; iKey++) {	// for each key
		if (m_arrKey[iKey].m_rgn.PtInRegion(pt))	// if point within key's region
			return(iKey);	// return key's index
	}
	return(-1);	// key not found
}

CPianoCtrl::CKey::CKey()
{
	m_bIsBlack = FALSE;
	m_bIsPressed = FALSE;
	m_bIsExternal = FALSE;
	m_nColor = -1;	// no per-key color
}

CPianoCtrl::CKey& CPianoCtrl::CKey::operator=(const CKey& key)
{
	if (&key != this) {	// if not self-assignment
		// no need to copy regions because update recreates them all anyway
		m_bIsBlack = key.m_bIsBlack;
		m_bIsPressed = key.m_bIsPressed;
		m_bIsExternal = key.m_bIsExternal;
		m_nColor = key.m_nColor;
	}
	return(*this);
}

void CPianoCtrl::Update(CSize szClient)
{
	// compute number of white keys
	int	nWhites = 0;
	int	nKeys = m_arrKey.GetSize();	// get existing key count
	for (int iKey = 0; iKey < nKeys; iKey++)	// for each key array element
		m_arrKey[iKey].m_rgn.DeleteObject();	// delete key's polygonal area
	int	nStartDelta = m_nStartNote - m_nPrevStartNote;
	if (nStartDelta) {	// if start note changed
		if (nStartDelta > 0)	// if start note increased
			m_arrKey.RemoveAt(0, min(nStartDelta, nKeys));
		else {	// start note decreased
			CKey	key;
			m_arrKey.InsertAt(0, key, min(-nStartDelta, nKeys));
		}
		m_nPrevStartNote = m_nStartNote;
	}
	nKeys = GetKeyCount();	// get new key count
	m_arrKey.SetSize(nKeys);	// resize key array, possibly reallocating it
	for (int iKey = 0; iKey < nKeys; iKey++) {	// for each key
		int	iKeyInfo = (m_nStartNote + iKey) % NOTES;	// account for start note
		if (!m_arrKeyInfo[iKeyInfo].nBlackOffset)	// if key is black
			nWhites++;
	}
	// build array of key regions, one per key
	double	fWhiteWidth;
	CSize	szBlack;
	DWORD	dwStyle = GetStyle();
	bool	bIsVertical = (dwStyle & PS_VERTICAL) != 0;
	if (bIsVertical)	// if vertical orientation
		fWhiteWidth = double(szClient.cy) / nWhites;
	else	// horizontal orientation
		fWhiteWidth = double(szClient.cx) / nWhites;
	double	fHScale = fWhiteWidth / WHITE_WIDTH;
	double	fBlackWidth = BLACK_WIDTH * fHScale;
	if (bIsVertical)	// if vertical orientation
		szBlack = CSize(Round(m_fBlackHeightRatio * szClient.cx), Round(fBlackWidth));
	else	// horizontal orientation
		szBlack = CSize(Round(fBlackWidth), Round(m_fBlackHeightRatio * szClient.cy));
	int	iStartWhite = m_arrKeyInfo[m_nStartNote % NOTES].nWhiteIndex;
	double	fStartOffset = iStartWhite * fWhiteWidth;
	int	iWhite = iStartWhite;
	int	nPrevWhiteX = 0;
	int	nPrevBlackOffset = 0;
	for (int iKey = 0; iKey < nKeys; iKey++) {	// for each key
		CKey&	key = m_arrKey[iKey];
		int	iKeyInfo = (m_nStartNote + iKey) % NOTES;	// account for start note
		int	nBlackOffset = m_arrKeyInfo[iKeyInfo].nBlackOffset;
		if (nBlackOffset) {	// if black key
			double	fOctaveOffset = (iWhite / WHITES) * fWhiteWidth * WHITES;
			int	x = Round(nBlackOffset * fHScale - fStartOffset + fOctaveOffset);
			CRect	rKey;
			if (bIsVertical)	// if vertical orientation
				rKey = CRect(CPoint(0, szClient.cy - x - szBlack.cy), szBlack);
			else	// horizontal orientation
				rKey = CRect(CPoint(x, 0), szBlack);
			key.m_rgn.CreateRectRgnIndirect(rKey);	// create black key region
			if (iKey) {	// if not first key
				// deduct this black key's region from previous white key's region
				CRgn&	rgnPrev = m_arrKey[iKey - 1].m_rgn;
				rgnPrev.CombineRgn(&rgnPrev, &key.m_rgn, RGN_DIFF);
			}
		} else {	// white key
			int	x = Round((iWhite + 1) * fWhiteWidth - fStartOffset);
			CRect	rKey;
			if (bIsVertical)	// if vertical orientation
				rKey = CRect(0, szClient.cy - nPrevWhiteX, szClient.cx, szClient.cy - x);
			else	// horizontal orientation
				rKey = CRect(nPrevWhiteX, 0, x, szClient.cy);
			key.m_rgn.CreateRectRgnIndirect(rKey);	// create white key region
			if (nPrevBlackOffset) {	// if previous key is black
				// deduct previous black key's region from this white key's region
				CRgn&	rgnPrev = m_arrKey[iKey - 1].m_rgn;
				key.m_rgn.CombineRgn(&key.m_rgn, &rgnPrev, RGN_DIFF);
			}
			nPrevWhiteX = x;
			iWhite++;
		}
		key.m_bIsBlack = nBlackOffset != 0;
		nPrevBlackOffset = nBlackOffset;
	}
	m_szBlackKey = szBlack;
	if (dwStyle & PS_SHOW_SHORTCUTS) {	// if showing shortcuts
		m_arrKeyLabel.RemoveAll();	// delete any existing labels
		m_arrKeyLabel.SetSize(nKeys);	// allocate labels
	}
	// if using or showing shortcuts
	if (dwStyle & (PS_SHOW_SHORTCUTS | PS_USE_SHORTCUTS)) {
		memset(m_arrScanCodeToKey, -1, sizeof(m_arrScanCodeToKey));
		int	iScanCode = 0;
		for (int iKey = 0; iKey < nKeys; iKey++) {	// for each key
			// if key is white, and first key or previous key is white
			if (!IsBlack(iKey) && (!iKey || !IsBlack(iKey - 1)))
				iScanCode++;	// advance one extra scan code
			if (iScanCode < _countof(m_arrScanCode)) {	// if within scan code array
				int	nScanCode = m_arrScanCode[iScanCode];
				ASSERT(nScanCode < _countof(m_arrScanCodeToKey));
				m_arrScanCodeToKey[nScanCode] = static_cast<char>(iKey);
				if (dwStyle & PS_SHOW_SHORTCUTS) {	// if showing shortcuts
					TCHAR	szKeyName[2];	// buffer for shortcut key name
					if (GetKeyNameText(nScanCode << 16, szKeyName, _countof(szKeyName)))
						m_arrKeyLabel[iKey] = szKeyName;
				}
			}
			iScanCode++;
		}
	}
	if (m_fontWnd == NULL)	// if window font not specified
		UpdateKeyLabelFont(szClient, dwStyle);	// update auto-scaled key label font
}

void CPianoCtrl::UpdateKeyLabelFont(CSize szClient, DWORD dwStyle)
{
	LOGFONT	lf;
	ZeroMemory(&lf, sizeof(lf));
	lf.lfQuality = ANTIALIASED_QUALITY;
	lf.lfPitchAndFamily = FF_SWISS;	// sans serif
	lf.lfWeight = FW_BOLD;
	CSize	szMaxLabel(0, 0);
	CClientDC	dc(this);
	CFont	fontExt;	// font for measuring label text extents
	lf.lfHeight = 50;	// sufficient height for reasonable precision
	if (!fontExt.CreateFontIndirect(&lf))	// create text extent font
		AfxThrowResourceException();
	HGDIOBJ	hPrevFont = dc.SelectObject(fontExt);	// select font
	int	nLabels = m_arrKeyLabel.GetSize();
	for (int iLabel = 0; iLabel < nLabels; iLabel++) {	// for each label
		CSize	szExt = dc.GetTextExtent(m_arrKeyLabel[iLabel]);
		if (szExt.cx > szMaxLabel.cx)	// if label width exceeds maximum
			szMaxLabel.cx = szExt.cx;	// update maximum
		if (szExt.cy > szMaxLabel.cy)	// if label height exceeds maximum
			szMaxLabel.cy = szExt.cy;	// update maximum
	}
	dc.SelectObject(hPrevFont);	// restore DC's default font
	double	fFontAspect;
	if (szMaxLabel.cx)	// if at least one label has non-zero width
		fFontAspect = double(szMaxLabel.cy) / szMaxLabel.cx;	// get font aspect ratio
	else	// all zero widths
		fFontAspect = 0;	// avoid divide by zero
	double	fWhiteAvail = 2.0 / 3.0;	// portion of white key available for label
	// compute font height; truncate instead of rounding to err on side of caution
	int	nFontHeight;	
	if (dwStyle & PS_VERTICAL) {	// if vertical orientation
		int	nWhiteMaxWidth = Trunc((szClient.cx - m_szBlackKey.cx) * fWhiteAvail);
		nFontHeight = Trunc(min(szMaxLabel.cx, nWhiteMaxWidth) * fFontAspect);
		nFontHeight = min(nFontHeight, m_szBlackKey.cy);
	} else {	// horizontal orientation
		if (dwStyle & PS_ROTATE_LABELS) {	// if rotated labels
			int	nWhiteMaxHeight = Trunc((szClient.cy - m_szBlackKey.cy) * fWhiteAvail);
			nFontHeight = Trunc(min(szMaxLabel.cx, nWhiteMaxHeight) * fFontAspect);
			nFontHeight = min(nFontHeight, m_szBlackKey.cx);
			lf.lfOrientation = 900;	// rotate font 90 degrees counter-clockwise
			lf.lfEscapement = 900;
		} else {	// normal labels
			nFontHeight = Trunc((m_szBlackKey.cx - OUTLINE_WIDTH * 2) * fFontAspect);
			int	nWhiteMaxHeight = Trunc((szClient.cy - m_szBlackKey.cy) * fWhiteAvail);
			nFontHeight = min(nFontHeight, nWhiteMaxHeight);
		}
	}
	nFontHeight = CLAMP(nFontHeight, MIN_FONT_HEIGHT, MAX_FONT_HEIGHT);
	lf.lfHeight = nFontHeight;	// font height, in logical pixels
	m_fontKeyLabel.DeleteObject();	// delete previous font if any
	if (!m_fontKeyLabel.CreateFontIndirect(&lf))	// create font
		AfxThrowResourceException();
}

inline void CPianoCtrl::UpdateKeyLabelFont()
{
	CRect	rc;
	GetClientRect(rc);
	UpdateKeyLabelFont(rc.Size(), GetStyle());
	Invalidate();
}

void CPianoCtrl::Update()
{
	CRect	rc;
	GetClientRect(rc);
	Update(rc.Size());
	Invalidate();
}

void CPianoCtrl::SetStyle(DWORD dwStyle, bool bEnable)
{
	BOOL	bRet;
	if (bEnable)
		bRet = ModifyStyle(0, dwStyle);
	else
		bRet = ModifyStyle(dwStyle, 0);
	if (bRet)	// if style actually changed
		Update();
}

void CPianoCtrl::SetRange(int nStartNote, int nKeys)
{
	ASSERT(nStartNote >= 0);
	ASSERT(nKeys >= 0);
	m_nStartNote = nStartNote;
	m_nKeys = nKeys;
	if (m_hWnd != NULL)	// if control exists
		Update();
}

void CPianoCtrl::SetPressed(int iKey, bool bEnable, bool bExternal)
{
	CKey&	key = m_arrKey[iKey];
	if (bEnable == key.m_bIsPressed)	// if key already in requested state
		return;	// nothing to do
	DWORD	dwStyle = GetStyle();
	if (dwStyle & PS_HIGHLIGHT_PRESS)	// if indicating pressed keys
		InvalidateRgn(&key.m_rgn);	// mark key for repainting
	key.m_bIsPressed = bEnable;
	key.m_bIsExternal = bExternal;
	if (!bExternal) {	// if key pressed by us
		CWnd	*pParentWnd = GetParent();	// notify parent window
		ASSERT(pParentWnd != NULL);
		pParentWnd->SendMessage(UWM_PIANOKEYCHANGE, iKey, LPARAM(m_hWnd));
	}
}

void CPianoCtrl::SetKeyLabel(int iKey, const CString& sLabel)
{
	m_arrKeyLabel[iKey] = sLabel;
	UpdateKeyLabelFont();
}

void CPianoCtrl::SetKeyLabels(const CStringArrayEx& arrLabel)
{
	m_arrKeyLabel = arrLabel;
	UpdateKeyLabelFont();
}

void CPianoCtrl::RemoveKeyLabels()
{
	m_arrKeyLabel.RemoveAll();
	Invalidate();
}

void CPianoCtrl::RemoveKeyColors()
{
	int	nKeys = GetKeyCount();
	for (int iKey = 0; iKey < nKeys; iKey++)	// for each key
		m_arrKey[iKey].m_nColor = -1;	// restore default color scheme
	Invalidate();
}

void CPianoCtrl::OnKeyChange(UINT nKeyFlags, bool bIsDown)
{
	if (GetStyle() & PS_USE_SHORTCUTS) {	// if using shortcuts
		int nScanCode = nKeyFlags & 0xff;
		int	iKey = m_arrScanCodeToKey[nScanCode];
		if (iKey >= 0 && iKey < _countof(m_arrScanCodeToKey))
			SetPressed(iKey, bIsDown);
	}
}

bool CPianoCtrl::IsShorcutScanCode(int nScanCode) const
{
	for (int iScanCode = 0; iScanCode < _countof(m_arrScanCode); iScanCode++) {
		if (m_arrScanCode[iScanCode] == nScanCode)
			return(TRUE);
	}
	return(FALSE);
}

void CPianoCtrl::ReleaseKeys(UINT nKeySourceMask)
{
	int	nKeys = GetKeyCount();
	for (int iKey = 0; iKey < nKeys; iKey++) {	// for each key
		const CKey&	key = m_arrKey[iKey];
		UINT	nKeyBit = key.m_bIsExternal ? KS_EXTERNAL : KS_INTERNAL;
		// if key pressed and its source matches caller's mask
		if (key.m_bIsPressed && (nKeyBit & nKeySourceMask))
			SetPressed(iKey, FALSE);	// reset key to avoid stuck notes
	}
}

/////////////////////////////////////////////////////////////////////////////
// CPianoCtrl drawing

void CPianoCtrl::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	CRect	cb;
	dc.GetClipBox(cb);
//printf("OnPaint cb %d %d %d %d\n", cb.left, cb.top, cb.right, cb.bottom);
	HFONT	hFont;
	if (m_fontWnd != NULL)	// if user specified font
		hFont = m_fontWnd;
	else	// font not specified
		hFont = m_fontKeyLabel;	// use auto-scaled font
	HGDIOBJ	hPrevFont = dc.SelectObject(hFont);
	DWORD	dwStyle = GetStyle();
	bool	bShowPressed = (dwStyle & PS_HIGHLIGHT_PRESS) != 0;
	UINT	nAlign;
	if (dwStyle & PS_VERTICAL) {	// if vertical orientation
		nAlign = TA_RIGHT | TA_TOP;
	} else {	// horizontal orientation
		if (dwStyle & PS_ROTATE_LABELS)	// if rotated labels
			nAlign = TA_LEFT | TA_TOP;
		else	// normal labels
			nAlign = TA_CENTER | TA_BOTTOM;
	}
	dc.SetTextAlign(nAlign);
	TEXTMETRIC	textMetric;
	dc.GetTextMetrics(&textMetric);
	dc.SetBkMode(TRANSPARENT);
	int	nKeys = GetKeyCount();
	for (int iKey = 0; iKey < nKeys; iKey++) {	// for each key
		CKey&	key = m_arrKey[iKey];
		if (key.m_rgn.RectInRegion(cb)) {	// if key's region intersects clip box
			// fill key's region with appropriate brush for key color and state
			bool	bIsPressed = key.m_bIsPressed & bShowPressed;
			if ((dwStyle & PS_PER_KEY_COLORS) && key.m_nColor >= 0) {
				SetDCBrushColor(dc, key.m_nColor);
				FillRgn(dc, key.m_rgn, (HBRUSH)GetStockObject(DC_BRUSH));
			} else	// default color scheme
				dc.FillRgn(&key.m_rgn, &m_arrKeyBrush[key.m_bIsBlack][bIsPressed]);
			// outline key's region; this flickers slightly because it overlaps fill
			dc.FrameRgn(&key.m_rgn, &m_brOutline, OUTLINE_WIDTH, OUTLINE_WIDTH);
			if (iKey < m_arrKeyLabel.GetSize() && !m_arrKeyLabel[iKey].IsEmpty()) {
				COLORREF	clrText;
				if (dwStyle & PS_PER_KEY_COLORS) {	// if per-key colors
					COLORREF	clrBk;
					int	nBkMode = TRANSPARENT;
					if (key.m_nColor >= 0) {	// if key has custom color
						// compute lightness, compensating for our sensitivity to green
						int	nLightness = GetRValue(key.m_nColor) 
							+ GetGValue(key.m_nColor) * 2 + GetBValue(key.m_nColor);
						if (nLightness < 255 * 2)	// if nLightness below threshold
							clrText = RGB(255, 255, 255);	// white text
						else	// key is light enough
							clrText = RGB(0, 0, 0);	// black text
						if (dwStyle & PS_INVERT_LABELS) {	// if inverting labels
							if (key.m_bIsPressed) {	// if key pressed
								clrBk = clrText;	// set background to text color
								clrText ^= 0xffffff;	// invert text color
								nBkMode = OPAQUE;
							} else	// key not pressed
								clrBk = key.m_nColor;	// make background same as key
						} else	// not highlighting pressed keys
							clrBk = key.m_nColor;	// make background same as key
					} else {	// default color scheme
						clrText = m_arrKeyColor[!key.m_bIsBlack][0];
						clrBk = m_arrKeyColor[key.m_bIsBlack][key.m_bIsPressed];
					}
					dc.SetBkColor(clrBk);	// set background color
					dc.SetBkMode(nBkMode);	// set background mode
				} else	// default color scheme
					clrText = m_arrKeyColor[!key.m_bIsBlack][0];
				dc.SetTextColor(clrText);	// set text color
				CRect	rKey;
				key.m_rgn.GetRgnBox(rKey);
				CPoint	pt;
				if (dwStyle & PS_VERTICAL) {	// if vertical orientation
					pt.x = rKey.right - textMetric.tmDescent * 2;
					pt.y = rKey.top + (rKey.Height() - textMetric.tmHeight) / 2;
				} else {	// horizontal orientation
					if (dwStyle & PS_ROTATE_LABELS) {	// if vertical labels
						pt.x = rKey.left + (rKey.Width() - textMetric.tmHeight) / 2;
						pt.y = rKey.bottom - textMetric.tmDescent * 2;
					} else {	// horizontal labels
						pt.x = rKey.left + rKey.Width() / 2;
						pt.y = rKey.bottom - textMetric.tmDescent;
					}
				}
				dc.TextOut(pt.x, pt.y, m_arrKeyLabel[iKey]);
			}
		}
	}
	dc.SelectObject(hPrevFont);
}

BOOL CPianoCtrl::OnEraseBkgnd(CDC* pDC) 
{
	UNREFERENCED_PARAMETER(pDC);
	return(TRUE);	// don't erase background
}

/////////////////////////////////////////////////////////////////////////////
// CPianoCtrl message map

BEGIN_MESSAGE_MAP(CPianoCtrl, CWnd)
	//{{AFX_MSG_MAP(CPianoCtrl)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_WM_ERASEBKGND()
	ON_MESSAGE(WM_SETFONT, OnSetFont)
	ON_MESSAGE(WM_GETFONT, OnGetFont)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPianoCtrl message handlers

int CPianoCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	// create key brushes	
	for (int iType = 0; iType < KEY_TYPES; iType++) {	// for each key type
		for (int iState = 0; iState < KEY_STATES; iState++) {	// for each key state
			if (!m_arrKeyBrush[iType][iState].CreateSolidBrush(m_arrKeyColor[iType][iState]))
				AfxThrowResourceException();
		}
	}
	if (!m_brOutline.CreateSolidBrush(m_clrOutline))	// create outline brush
		AfxThrowResourceException();
	return 0;
}

LRESULT CPianoCtrl::OnGetFont(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	return (LRESULT)m_fontWnd;
}

LRESULT CPianoCtrl::OnSetFont(WPARAM wParam, LPARAM lParam)
{
	m_fontWnd = (HFONT)wParam;
	if (lParam)
		Invalidate();
	return 0;
}

void CPianoCtrl::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	Update(CSize(cx, cy));
}

void CPianoCtrl::OnLButtonDown(UINT nFlags, CPoint point) 
{
	DWORD	dwStyle = GetStyle();
	// if internal key press enabled, or sending press notifications
	if (!(dwStyle & PS_NO_INTERNAL) || (dwStyle & PS_NOTIFY_PRESS)) {
		int	iKey = FindKey(point);	// find key containing cursor position
		if (iKey >= 0) {	// if key found
			ASSERT(m_iCurKey < 0);	// shouldn't be a current key, else logic error
			if (!(dwStyle & PS_NO_INTERNAL))	// if internal key press enabled
				SetPressed(iKey, TRUE);
			SetCapture();	// capture mouse input
			m_iCurKey = iKey;	// set current key
			if (dwStyle & PS_NOTIFY_PRESS) {	// if sending press notifications
				CWnd	*pParentWnd = GetParent();	// notify parent window
				ASSERT(pParentWnd != NULL);
				pParentWnd->SendMessage(UWM_PIANOKEYPRESS, MAKELONG(iKey, true), LPARAM(m_hWnd));
			}
		}
	}
	CWnd::OnLButtonDown(nFlags, point);
}

void CPianoCtrl::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if (m_iCurKey >= 0) {
		DWORD	dwStyle = GetStyle();
		if (!(dwStyle & PS_NO_INTERNAL))	// if internal key press enabled
			SetPressed(m_iCurKey, FALSE);
		if (dwStyle & PS_NOTIFY_PRESS) {	// if sending press notifications
			CWnd	*pParentWnd = GetParent();	// notify parent window
			ASSERT(pParentWnd != NULL);
			pParentWnd->SendMessage(UWM_PIANOKEYPRESS, MAKELONG(m_iCurKey, false), LPARAM(m_hWnd));
		}
		ReleaseCapture();	// release mouse input
		m_iCurKey = -1;	// reset current key
	}
	CWnd::OnLButtonUp(nFlags, point);
}

void CPianoCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	UNREFERENCED_PARAMETER(nChar);
	UNREFERENCED_PARAMETER(nRepCnt);
	UNREFERENCED_PARAMETER(nFlags);
	OnKeyChange(nFlags, TRUE);
}

void CPianoCtrl::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	UNREFERENCED_PARAMETER(nChar);
	UNREFERENCED_PARAMETER(nRepCnt);
	UNREFERENCED_PARAMETER(nFlags);
	OnKeyChange(nFlags, FALSE);
}
