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

// TrackDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Polymeter.h"
#include "TrackDlg.h"

#include "PolymeterDoc.h"
#include "PolymeterView.h"
#include "UndoCodes.h"

// CTrackDlg dialog

IMPLEMENT_DYNAMIC(CTrackDlg, CDialog)

const int CTrackDlg::m_arrPropCapId[PROPERTIES] = {
	#define TRACKDEF(type, prefix, name, defval, offset) IDS_TRK_##name,
	#include "TrackDef.h"		// generate track property caption resource IDs
};

CTrackDlg::CTrackDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
{
	m_iTrack = 0;
	m_iHotStep = -1;
	m_bIsSelected = false;
	const UINT	nNumEditFlags = CNumEdit::DF_INT | CNumEdit::DF_SPIN;
	#define TRACKDEF(type, prefix, name, defval, offset) m_##name.SetFormat(nNumEditFlags);
	#define TRACKDEF_INT		// for integer track properties only
	#include "TrackDef.h"		// generate code to configure numeric edit controls
	m_Length.SetRange(1, MAX_STEPS);
	m_Quant.SetRange(1, SHRT_MAX);
	m_Channel.SetRange(1, 16);
	m_Note.SetRange(0, 127);
	m_Velocity.SetRange(-127, 127);
}

CTrackDlg::~CTrackDlg()
{
}

inline CPolymeterView *CTrackDlg::GetView()
{
	return STATIC_DOWNCAST(CPolymeterView, GetParent());
}

inline CPolymeterDoc *CTrackDlg::GetDoc()
{
	CPolymeterView	*pView = GetView();
	ASSERT(pView != NULL);
	return pView->GetDocument();
}

inline CPolymeterDoc *CTrackDlg::GetDoc(CPolymeterView*& pView)
{
	pView = GetView();
	ASSERT(pView != NULL);
	return pView->GetDocument();
}

void CTrackDlg::UpdateDoc(int iProp)
{
	int	iTrack = m_iTrack;
	CPolymeterDoc	*pDoc = GetDoc();
	CSequencer&	seq = pDoc->m_Seq;
	switch (iProp) {
	case PROP_Name:
		{
			CString	sName;
			m_Name.GetWindowText(sName);
			seq.SetName(iTrack, sName);
		}
		break;
	#define TRACKDEF(type, prefix, name, defval, offset) \
		case PROP_##name:	\
			seq.Set##name(iTrack, m_##name.GetIntVal() - offset);	\
			break;
	#define TRACKDEF_INT	// for integer track properties only
	#include "TrackDef.h"	// generate code to retrieve values of numeric edit controls
	case PROP_Mute:
		seq.SetMute(iTrack, m_Mute.GetCheck() != 0);
		break;
	default:
		NODEFAULTCASE;
	}
}

void CTrackDlg::UpdateProp(int iProp, bool bGotoCtrl)
{
	int	iTrack = m_iTrack;
	CPolymeterDoc	*pDoc = GetDoc();
	const CSequencer&	seq = pDoc->m_Seq;
	CWnd	*pGotoCtrl = NULL;
	switch (iProp) {
	case PROP_Name:
		m_Name.SetWindowText(seq.GetName(iTrack));
		pGotoCtrl = &m_Name;
		break;
	#define TRACKDEF(type, prefix, name, defval, offset) \
		case PROP_##name:	\
			m_##name.SetVal(seq.Get##name(iTrack) + offset);	\
			pGotoCtrl = &m_##name;	\
			break;
	#define TRACKDEF_INT	// for integer track properties only
	#include "TrackDef.h"	// generate code to set values of numeric edit control
	case PROP_Mute:
		m_Mute.SetCheck(seq.GetMute(iTrack));
		pGotoCtrl = &m_Mute;
		break;
	default:
		NODEFAULTCASE;
	}
	if (iProp == PROP_Length)
		OnLengthChange();
	if (bGotoCtrl && pGotoCtrl != NULL)
		GotoDlgCtrl(pGotoCtrl);
}

void CTrackDlg::Update()
{
	int	iTrack = m_iTrack;
	CPolymeterDoc	*pDoc = GetDoc();
	CSequencer&	seq = pDoc->m_Seq;
	m_Name.SetWindowText(seq.GetName(iTrack));
	#define TRACKDEF(type, prefix, name, defval, offset) m_##name.SetVal(seq.Get##name(iTrack) + offset);
	#define TRACKDEF_INT		// for integer track properties only
	#include "TrackDef.h"		// generate code to update numeric edit controls
	m_Mute.SetCheck(seq.GetMute(iTrack));
	OnLengthChange();
}

void CTrackDlg::GetStepRect(int iStep, CRect& rStep)
{
	CPolymeterView	*pView = GetView();
	CSize	szStep(pView->GetStepSize());
	int	nFirstStepX = pView->GetFirstStepX();
	rStep = CRect(CPoint(nFirstStepX + iStep * szStep.cx, 0), szStep);
}

void CTrackDlg::GetStepsRect(CRect& rStep)
{
	CPolymeterView	*pView = GetView();
	CSize	szStep(pView->GetStepSize());
	int	nFirstStepX = pView->GetFirstStepX();
	rStep = CRect(CPoint(nFirstStepX, 0), CSize(szStep.cx * MAX_STEPS, szStep.cy));
}

int CTrackDlg::HitTest(CPoint point)
{
	CPolymeterView	*pView;
	CPolymeterDoc	*pDoc = GetDoc(pView);
	int	nFirstStepX = pView->GetFirstStepX();
	CSize	szStep(pView->GetStepSize());
	int	x = point.x - nFirstStepX;
	int	nStepsWidth = pDoc->m_Seq.GetLength(m_iTrack) * szStep.cx;
	if (x >= 0 && x < nStepsWidth)
		return x / szStep.cx;
	return -1;
}

void CTrackDlg::UpdateStep(int iStep)
{
	CRect	rStep;
	GetStepRect(iStep, rStep);
	InvalidateRect(rStep);
}

void CTrackDlg::OnLengthChange()
{
	CRect	rSteps;
	GetStepsRect(rSteps);
	InvalidateRect(rSteps);
}

void CTrackDlg::OnTrackPropEdit(int iProp)
{
	CPolymeterView	*pView;
	CPolymeterDoc	*pDoc = GetDoc(pView);
	int	nSels = pView->GetSelectedCount();
	if (nSels && pView->IsSelected(m_iTrack)) {	// if multi-track edit
		CIntArrayEx	arrSelection;
		pView->GetSelection(arrSelection);
		pDoc->NotifyUndoableEdit(iProp, UCODE_MULTI_TRACK_PROP);
		UpdateDoc(iProp);
		CComVariant	var;
		pDoc->m_Seq.GetTrackProperty(m_iTrack, iProp, var);
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iTrack = arrSelection[iSel];
			if (iTrack != m_iTrack)	// if not source track
				pDoc->m_Seq.SetTrackProperty(iTrack, iProp, var);
		}
		CPolymeterDoc::CMultiItemPropHint	hint(arrSelection, iProp);
		GetDoc()->UpdateAllViews(NULL, CPolymeterDoc::HINT_MULTI_TRACK_PROP, &hint);
	} else {	// single track edit
		UINT	nUndoFlags;
		if (iProp == PROP_Mute)	// if mute
			nUndoFlags = 0;
		else	// numeric edit
			nUndoFlags = CUndoable::UE_COALESCE;	// coalesce edits
		pDoc->NotifyUndoableEdit(MAKELONG(m_iTrack, iProp), UCODE_TRACK_PROP, nUndoFlags);
		pDoc->SetModifiedFlag();
		UpdateDoc(iProp);
		CPolymeterDoc::CPropHint	hint(m_iTrack, iProp);
		GetDoc()->UpdateAllViews(pView, CPolymeterDoc::HINT_TRACK_PROP, &hint);
		if (iProp == PROP_Length)
			OnLengthChange();
	}
}

void CTrackDlg::Select(bool bEnable)
{
	if (bEnable == m_bIsSelected)	// if already in requested state
		return;	// nothing to do
	Invalidate();
	m_Number.Invalidate();
	m_bIsSelected = bEnable;
}

COLORREF CTrackDlg::GetBkColor()
{
	COLORREF	nBkColor;
	if (m_bIsSelected) {
		nBkColor = COLOR_HIGHLIGHT; 
	} else {
		if (m_iTrack & 1)
			nBkColor = COLOR_3DLIGHT;
		else
			nBkColor = COLOR_BTNFACE;
	}
	COLORREF	clr = GetSysColor(nBkColor);
	return clr;
}

void CTrackDlg::SetHotStep(int iStep)
{
	if (iStep == m_iHotStep)	// if unchanged
		return;		// nothing to do
	if (m_iHotStep >= 0)	// if previous hot step valid
		UpdateStep(m_iHotStep);
	m_iHotStep = iStep;
	if (iStep >= 0)	// if new hot step valid
		UpdateStep(iStep);
}

void CTrackDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TRK_NUMBER, m_Number);
	#define TRACKDEF(type, prefix, name, defval, offset) DDX_Control(pDX, IDC_TRK_##name, m_##name);
	#include "TrackDef.h"		// generate data exchange code for track properties
}

// CTrackDlg message map

BEGIN_MESSAGE_MAP(CTrackDlg, CDialog)
	#define TRACKDEF(type, prefix, name, defval, offset) ON_NOTIFY(NEN_CHANGED, IDC_TRK_##name, OnChanged##name)
	#define TRACKDEF_INT		// for integer track properties only
	#include "TrackDef.h"		// generate message map entries for numeric edit controls
	ON_EN_KILLFOCUS(IDC_TRK_Name, OnKillfocusName)
	ON_CONTROL(BN_CLICKED, IDC_TRK_Mute, OnClickedMute)
	ON_WM_CTLCOLOR()
	ON_WM_LBUTTONDOWN()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

// CTrackDlg message handlers

BOOL CTrackDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	CString	s;
	s.Format(_T("%d"), m_iTrack + 1);
	m_Number.SetWindowText(s);	// set track number

	return TRUE;  // return TRUE unless you set the focus to a control
}

BOOL CTrackDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		switch (pMsg->wParam) {
		case VK_RETURN:
			{
				CWnd	*pWnd = GetFocus();
				if (pWnd == &m_Name) {
					OnKillfocusName();	// handle possible name change
					m_Name.SetSel(0, -1);	// select entire text
					return TRUE;	// handled, don't dispatch
				}
			}
			break;
		case VK_ESCAPE:
			{
				CWnd	*pWnd = GetFocus();
				if (pWnd != NULL && IsChild(pWnd)) {
					if (pWnd->IsKindOf(RUNTIME_CLASS(CNumEdit))) {
						CNumEdit	*pEdit = STATIC_DOWNCAST(CNumEdit, pWnd);
						pEdit->SetVal(pEdit->GetVal());
						pEdit->SetSel(0, -1);	// select entire text
					} else if (pWnd == &m_Name) {
						m_Name.SetWindowText(GetDoc()->m_Seq.GetName(m_iTrack));
						m_Name.SetSel(0, -1);	// select entire text
					}
				}
			}
			break;
		}
	}
	return GetParent()->PreTranslateMessage(pMsg);
}

HBRUSH CTrackDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH	hbr;
	if (nCtlColor == CTLCOLOR_STATIC) {
		COLORREF	clr = GetBkColor();
		pDC->SetBkColor(clr);
		pDC->SetDCBrushColor(clr);
		hbr = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
	} else {
		hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	}
	return hbr;
}

BOOL CTrackDlg::OnEraseBkgnd(CDC* pDC)
{
	UNREFERENCED_PARAMETER(pDC);
//	return __super::OnEraseBkgnd(pDC);
	return TRUE;	// OnPaint erases background
}

void CTrackDlg::OnPaint()
{
	enum {	// step flags
		BTF_ON		= 0x01,
		BTF_HOT		= 0x02,
		BTF_MUTE	= 0x04,
	};
	static const COLORREF clrStep[] = {
		RGB(255, 255, 255),		// off
		RGB(0, 0, 0),			// on
		RGB(160, 255, 160),		// off + hot
		RGB(0, 128, 0),			// on + hot
		RGB(255, 255, 255),		// off + mute
		RGB(0, 0, 0),			// on + mute
		RGB(255, 160, 160),		// off + hot + mute
		RGB(128, 0, 0),			// on + hot + mute
	};
	CPaintDC dc(this); // device context for painting
	CRect	rClip;
	dc.GetClipBox(rClip);
	CPolymeterView	*pView;
	CPolymeterDoc	*pDoc = GetDoc(pView);
	if (m_iTrack >= pDoc->m_Seq.GetTrackCount())	// if our sequencer track is missing
		return;	// degenerate case
	int	nLength = pDoc->m_Seq.GetLength(m_iTrack);
	bool	bMute = pDoc->m_Seq.GetMute(m_iTrack);
	int	nFirstStepX = pView->GetFirstStepX();
	CSize	szStep(pView->GetStepSize());
	CRect	rSteps(CPoint(nFirstStepX, 0), CSize(szStep.cx * nLength, szStep.cy));
	CRect	rClipSteps;
	if (rClipSteps.IntersectRect(rClip, rSteps)) {	// if clip box intersects steps
		int	iFirstStep = (rClipSteps.left - nFirstStepX) / szStep.cx;
		int	iLastStep = (rClipSteps.right - 1 - nFirstStepX) / szStep.cx;
		HGDIOBJ	hPrevBrush = dc.SelectObject(GetStockObject(DC_BRUSH));
		HGDIOBJ	hPrevPen = dc.SelectObject(GetStockObject(DC_PEN));
		COLORREF	clrOutline = GetSysColor(COLOR_BTNSHADOW);
		dc.SetDCPenColor(clrOutline);
		CBrush	brBkgnd(GetBkColor());
		int	x = nFirstStepX + iFirstStep * szStep.cx;
		for (int iStep = iFirstStep; iStep <= iLastStep; iStep++) {	// for each invalid step
			CRect	rStep(CPoint(x, 0), szStep);
			CRect	rTemp;
			dc.FrameRect(rStep, &brBkgnd);
			CRect	rBtn(rStep);
			rBtn.DeflateRect(1, 1);	// exclude frame
			int	nMask = pDoc->m_Seq.GetEvent(m_iTrack, iStep);
			if (theApp.m_Options.m_bViewShowCurPos) {
				if (iStep == m_iHotStep)
					nMask |= BTF_HOT;
				if (bMute)
					nMask |= BTF_MUTE;
			}
			dc.SetDCBrushColor(clrStep[nMask]);
			dc.Rectangle(rBtn);
			x += szStep.cx;
		}
		dc.SelectObject(hPrevBrush);
		dc.SelectObject(hPrevPen);
	}
	dc.ExcludeClipRect(rSteps);	// exclude steps from clipping region
	dc.FillSolidRect(rClip, GetBkColor());	// erase remaining background
}

void CTrackDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	int	iStep = HitTest(point);
	if (iStep >= 0) {	// if on a step
		UpdateStep(iStep);
		CPolymeterView	*pView;
		CPolymeterDoc	*pDoc = GetDoc(pView);
		pDoc->NotifyUndoableEdit(MAKELONG(m_iTrack, iStep), UCODE_TRACK_STEP);
		pDoc->SetModifiedFlag();
		pDoc->m_Seq.SetEvent(m_iTrack, iStep, pDoc->m_Seq.GetEvent(m_iTrack, iStep) ^ 1);
		CPolymeterDoc::CPropHint	hint(m_iTrack, iStep);
		GetDoc()->UpdateAllViews(pView, CPolymeterDoc::HINT_STEP, &hint);
	} else {	// not on a step
		CRect	rTrackNum;
		m_Number.GetWindowRect(rTrackNum);
		ScreenToClient(rTrackNum);
		if (point.x < rTrackNum.right)	// if clicked in track number area
			GotoDlgCtrl(&m_Number);	// end edit in progress if any
		GetParent()->SendMessage(UWM_SELECT, m_iTrack, nFlags);
	}
	__super::OnLButtonDown(nFlags, point);
}

void CTrackDlg::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	int	iStep = HitTest(point);
	if (iStep >= 0) {	// if on a step
		OnLButtonDown(nFlags, point);
		return;
	}
	__super::OnLButtonDblClk(nFlags, point);
}

void CTrackDlg::OnKillfocusName()
{
	CPolymeterView	*pView;
	CPolymeterDoc	*pDoc = GetDoc(pView);
	CString	sName;
	m_Name.GetWindowText(sName);
	CSequencer&	seq = pDoc->m_Seq;
	if (m_iTrack >= pDoc->m_Seq.GetTrackCount())	// if our sequencer track is missing
		return;	// degenerate case
	if (sName != seq.GetName(m_iTrack))	// if name changed
		OnTrackPropEdit(PROP_Name);
}

void CTrackDlg::OnChangedChannel(NMHDR *pNotifyStruct, LRESULT *pResult)
{
	UNREFERENCED_PARAMETER(pNotifyStruct);
	UNREFERENCED_PARAMETER(pResult);
	OnTrackPropEdit(PROP_Channel);
}

void CTrackDlg::OnChangedNote(NMHDR *pNotifyStruct, LRESULT *pResult)
{
	UNREFERENCED_PARAMETER(pNotifyStruct);
	UNREFERENCED_PARAMETER(pResult);
	OnTrackPropEdit(PROP_Note);
}

void CTrackDlg::OnChangedLength(NMHDR *pNotifyStruct, LRESULT *pResult)
{
	UNREFERENCED_PARAMETER(pNotifyStruct);
	UNREFERENCED_PARAMETER(pResult);
	OnTrackPropEdit(PROP_Length);
}

void CTrackDlg::OnChangedQuant(NMHDR *pNotifyStruct, LRESULT *pResult)
{
	UNREFERENCED_PARAMETER(pNotifyStruct);
	UNREFERENCED_PARAMETER(pResult);
	OnTrackPropEdit(PROP_Quant);
}

void CTrackDlg::OnChangedOffset(NMHDR *pNotifyStruct, LRESULT *pResult)
{
	UNREFERENCED_PARAMETER(pNotifyStruct);
	UNREFERENCED_PARAMETER(pResult);
	OnTrackPropEdit(PROP_Offset);
}

void CTrackDlg::OnChangedSwing(NMHDR *pNotifyStruct, LRESULT *pResult)
{
	UNREFERENCED_PARAMETER(pNotifyStruct);
	UNREFERENCED_PARAMETER(pResult);
	OnTrackPropEdit(PROP_Swing);
}

void CTrackDlg::OnChangedVelocity(NMHDR *pNotifyStruct, LRESULT *pResult)
{
	UNREFERENCED_PARAMETER(pNotifyStruct);
	UNREFERENCED_PARAMETER(pResult);
	OnTrackPropEdit(PROP_Velocity);
}

void CTrackDlg::OnChangedDuration(NMHDR *pNotifyStruct, LRESULT *pResult)
{
	UNREFERENCED_PARAMETER(pNotifyStruct);
	UNREFERENCED_PARAMETER(pResult);
	OnTrackPropEdit(PROP_Duration);
}

void CTrackDlg::OnClickedMute()
{
	OnTrackPropEdit(PROP_Mute);
}
