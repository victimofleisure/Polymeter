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

// PolymeterDoc.cpp : implementation of the CPolymeterDoc class
//

#include "stdafx.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "Polymeter.h"
#endif

#include "PolymeterDoc.h"
#include "PolymeterView.h"

#include <propkey.h>
#include "MainFrm.h"
#include "IniFile.h"
#include "RegTempl.h"
#include "UndoCodes.h"
#include "FocusEdit.h"
#include "MidiWrap.h"
#include "PathStr.h"
#include "ExportDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CPolymeterDoc

IMPLEMENT_DYNCREATE(CPolymeterDoc, CDocument)

#define FILE_ID			_T("Polymeter")
#define	FILE_VERSION	0

#define RK_FILE_ID		_T("sFileID")
#define RK_FILE_VERSION	_T("nFileVersion")

#define RK_TRACK_COUNT	_T("nTracks")
#define RK_TRACK_EVENT_COUNT	_T("nEvents")
#define RK_TRACK_EVENT_USED		_T("nUsed")
#define RK_TRACK_EVENT_DATA		_T("arrEvent")

#define RK_EXPORT_DURATION	_T("nExportDuration")

// define null title IDs for undo codes that have dynamic titles
#define IDS_EDIT_TRACK_PROP			0
#define IDS_EDIT_MULTI_TRACK_PROP	0
#define IDS_EDIT_TRACK_BEAT			0
#define IDS_EDIT_MASTER_PROP		0

const int CPolymeterDoc::m_nUndoTitleId[UNDO_CODES] = {
	#define UCODE_DEF(name) IDS_EDIT_##name,
	#include "UndoCodeData.h"	
};

// CPolymeterDoc construction/destruction

CPolymeterDoc::CPolymeterDoc()
{
	m_nFileVersion = FILE_VERSION;
	m_UndoMgr.SetRoot(this);
	SetUndoManager(&m_UndoMgr);
}

CPolymeterDoc::~CPolymeterDoc()
{
}

void CPolymeterDoc::CMySequencer::OnMidiError(MMRESULT nResult)
{
	theApp.GetMainFrame()->PostMessage(UWM_MIDI_ERROR, nResult, LPARAM(this));
	Abort();	// abort playback and clean up
}

BOOL CPolymeterDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)
	m_Seq.SetTrackCount(TRACKS);

	return TRUE;
}

void CPolymeterDoc::PreCloseFrame(CFrameWnd* pFrame )
{
	m_Seq.Play(false);	// stop playback
	CDocument::PreCloseFrame(pFrame);
}

void CPolymeterDoc::ApplyOptions(const COptions *pPrevOptions)
{
	UNREFERENCED_PARAMETER(pPrevOptions);
	m_Seq.SetOutputDevice(theApp.m_Options.m_iMidiOutputDevice);
	m_Seq.SetLatency(theApp.m_Options.m_nMidiLatency);
	m_Seq.SetBufferSize(theApp.m_Options.m_nMidiBufferSize);
}

// CPolymeterDoc serialization

void CPolymeterDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

void CPolymeterDoc::ReadProperties(LPCTSTR szPath)
{
	CIniFile	f;
	f.Open(szPath, CFile::modeRead);
	CString	sFileID;
	RdReg(RK_FILE_ID, sFileID);
	if (sFileID != FILE_ID) {
		CString	msg;
		AfxFormatString1(msg, IDS_DOC_BAD_FORMAT, szPath);
		AfxMessageBox(msg);
		AfxThrowUserException();
	}
	RdReg(RK_FILE_VERSION, m_nFileVersion);
	if (m_nFileVersion > FILE_VERSION) {
		CString	msg;
		AfxFormatString1(msg, IDS_DOC_NEWER_VERSION, szPath);
		AfxMessageBox(msg);
	}
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		if (PT_##proptype == PT_ENUM) \
			ReadEnum(REG_SETTINGS, _T(#name), m_##name, itemname, items); \
		else \
			RdReg(_T(#name), m_##name);
	#include "MasterPropsDef.h"
	m_Seq.SetTempo(m_fTempo);
	m_Seq.SetTimeDivision(CMasterProps::GetTimeDivisionTicks(m_nTimeDiv));
	int	nTracks = 0;
	RdReg(RK_TRACK_COUNT, nTracks);
	m_Seq.SetTrackCount(nTracks);
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		CTrack	trk(true);	// initialize to defaults
		CString	sTrkID;
		sTrkID.Format(_T("Track%d"), iTrack);
		#define TRACKDEF(type, prefix, name, defval, offset) RdReg(sTrkID, _T(#prefix)_T(#name), trk.m_##prefix##name);
		#include "TrackDef.h"		// generate code to read track properties
		DWORD	nEvts = 0;
		RdReg(sTrkID, RK_TRACK_EVENT_COUNT, nEvts);
		DWORD	nUsed = 0;
		RdReg(sTrkID, RK_TRACK_EVENT_USED, nUsed);
		trk.m_arrEvent.SetSize(nEvts);
		ZeroMemory(trk.m_arrEvent.GetData(), nEvts);
		CPersist::GetBinary(sTrkID, RK_TRACK_EVENT_DATA, trk.m_arrEvent.GetData(), &nUsed); 
		m_Seq.SetTrack(iTrack, trk);
	}
}

void CPolymeterDoc::WriteProperties(LPCTSTR szPath) const
{
	CIniFile	f;
	f.Open(szPath, CFile::modeCreate | CFile::modeWrite);
	WrReg(RK_FILE_ID, CString(FILE_ID));
	WrReg(RK_FILE_VERSION, FILE_VERSION);
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		if (PT_##proptype == PT_ENUM) \
			WriteEnum(REG_SETTINGS, _T(#name), m_##name, itemname, items); \
		else \
			WrReg(_T(#name), m_##name);
	#include "MasterPropsDef.h"
	int	nTracks = m_Seq.GetTrackCount();
	WrReg(RK_TRACK_COUNT, nTracks);
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		const CTrack&	trk = m_Seq.GetTrack(iTrack);
		CString	sTrkID;
		sTrkID.Format(_T("Track%d"), iTrack);
		#define TRACKDEF(type, prefix, name, defval, offset) WrReg(sTrkID, _T(#prefix)_T(#name), trk.m_##prefix##name);
		#include "TrackDef.h"		// generate code to write track properties
		int	nEvts = trk.m_arrEvent.GetSize();
		WrReg(sTrkID, RK_TRACK_EVENT_COUNT, nEvts);
		int	nUsed = trk.GetUsedEventCount();
		WrReg(sTrkID, RK_TRACK_EVENT_USED, nUsed);
		CPersist::WriteBinary(sTrkID, RK_TRACK_EVENT_DATA, trk.m_arrEvent.GetData(), nUsed); 
	}
}

void CPolymeterDoc::SaveUndoState(CUndoState& State)
{
	switch (State.GetCode()) {
	case UCODE_TRACK_PROP:
		{
			int	iTrack = LOWORD(State.GetCtrlID());
			int	iProp = HIWORD(State.GetCtrlID());
			switch (iProp) {
			#define TRACKDEF(type, prefix, name, defval, offset) \
			case PROP_##name:	\
				{	\
					type val(m_Seq.Get##name(iTrack));	\
					State.SetVal(val);	\
				}	\
				break;
			#include "TrackDef.h"		// generate code to save track properties
			default:
				NODEFAULTCASE;
			}
		}
		break;
	case UCODE_MULTI_TRACK_PROP:
		{
			int	iProp = State.GetCtrlID();
			CIntArrayEx	arrSelection;
			if (State.IsEmpty()) {	// if initial state
				CPolymeterView	*pView = theApp.GetMainFrame()->GetActiveMDIView();
				pView->GetSelection(arrSelection);	// get selection from view
			} else {	// undoing or redoing; view selection may have changed, so don't rely on it
				const CUndoMultiTrackPropEdit	*pInfo = static_cast<CUndoMultiTrackPropEdit*>(State.GetObj());
				arrSelection = pInfo->m_arrSelection;	//  reuse our state's original selection
			}
			CRefPtr<CUndoMultiTrackPropEdit>	pInfo;
			pInfo.CreateObj();
			pInfo->m_arrSelection = arrSelection;
			int	nSels = arrSelection.GetSize();
			pInfo->m_arrVal.SetSize(nSels);
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	iTrack = pInfo->m_arrSelection[iSel];
				m_Seq.GetTrackProperty(iTrack, iProp, pInfo->m_arrVal[iSel]);
			}
			State.SetObj(pInfo);
		}
		break;
	case UCODE_TRACK_BEAT:
		{
			int	iTrack = LOWORD(State.GetCtrlID());
			int	iBeat = HIWORD(State.GetCtrlID());
			State.m_Val.p.x.c.al = m_Seq.GetEvent(iTrack, iBeat);
		}
		break;
	case UCODE_MASTER_PROP:
		{
			int	iProp = State.GetCtrlID();
			GetValue(iProp, &State.m_Val, sizeof(State.m_Val));
		}
		break;
	case UCODE_CUT:
	case UCODE_PASTE:
	case UCODE_INSERT:
	case UCODE_DELETE:
	case UCODE_MOVE:
		if (UndoMgrIsIdle()) {	// if initial state
			CRefPtr<CUndoClipboard>	pClipboard;
			pClipboard.CreateObj();
			CPolymeterView	*pView = theApp.GetMainFrame()->GetActiveMDIView();
			pView->GetSelection(pClipboard->m_arrSelection);
			m_Seq.CopyTracks(pClipboard->m_arrSelection, pClipboard->m_arrTrack); 
			pClipboard->m_nSelMark = pView->GetSelectionMark();
			State.SetObj(pClipboard);
			switch (State.GetCode()) {
			case UCODE_CUT:
			case UCODE_DELETE:
			case UCODE_MOVE:
				State.m_Val.p.x.i = CUndoManager::UA_UNDO;	// undo inserts, redo deletes
				break;
			default:
				State.m_Val.p.x.i = CUndoManager::UA_REDO;	// undo deletes, redo inserts
			}
		}
		break;
	}
}

void CPolymeterDoc::RestoreUndoState(const CUndoState& State)
{
	switch (State.GetCode()) {
	case UCODE_TRACK_PROP:
		{
			int	iTrack = LOWORD(State.GetCtrlID());
			int	iProp = HIWORD(State.GetCtrlID());
			switch (iProp) {
			#define TRACKDEF(type, prefix, name, defval, offset) \
			case PROP_##name:	\
				{	\
					type val;	\
					State.GetVal(val);	\
					m_Seq.Set##name(iTrack, val);	\
				}	\
				break;
			#include "TrackDef.h"		// generate code to restore track properties
			default:
				NODEFAULTCASE;
			}
			CPropHint	hint(iTrack, iProp);
			UpdateAllViews(NULL, HINT_TRACK_PROP, &hint);
		}
		break;
	case UCODE_MULTI_TRACK_PROP:
		{
			int	iProp = State.GetCtrlID();
			const CUndoMultiTrackPropEdit	*pInfo = static_cast<CUndoMultiTrackPropEdit*>(State.GetObj());
			int	nSels = pInfo->m_arrSelection.GetSize();
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	iTrack = pInfo->m_arrSelection[iSel];
				m_Seq.SetTrackProperty(iTrack, iProp, pInfo->m_arrVal[iSel]);
			}
			CMultiTrackPropHint	hint(pInfo->m_arrSelection, iProp);
			UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
		}
		break;
	case UCODE_TRACK_BEAT:
		{
			int	iTrack = LOWORD(State.GetCtrlID());
			int	iBeat = HIWORD(State.GetCtrlID());
			m_Seq.SetEvent(iTrack, iBeat, State.m_Val.p.x.c.al);
			CPropHint	hint(iTrack, iBeat);
			UpdateAllViews(NULL, HINT_BEAT, &hint);
		}
		break;
	case UCODE_MASTER_PROP:
		{
			int	iProp = State.GetCtrlID();
			SetValue(iProp, &State.m_Val, sizeof(State.m_Val));
			switch (iProp) {
			case CMasterProps::PROP_fTempo:
				m_Seq.SetTempo(m_fTempo);
				break;
			case CMasterProps::PROP_nTimeDiv:
				// convert time division preset index to time division value in ticks
				m_Seq.SetTimeDivision(GetTimeDivisionTicks(m_nTimeDiv));
				break;
			}
			CPropHint	hint(0, iProp);
			UpdateAllViews(NULL, HINT_MASTER_PROP, &hint);
		}
		break;
	case UCODE_CUT:
	case UCODE_PASTE:
	case UCODE_INSERT:
	case UCODE_DELETE:
	case UCODE_MOVE:
		{
			CUndoClipboard	*pClipboard = static_cast<CUndoClipboard *>(State.GetObj());
			if (GetUndoAction() == State.m_Val.p.x.i) {	// if inserting
				if (State.GetCode() == UCODE_MOVE)
					m_Seq.DeleteTracks(State.GetCtrlID(), pClipboard->m_arrTrack.GetSize());
				m_Seq.InsertTracks(pClipboard->m_arrSelection, pClipboard->m_arrTrack);
				UpdateAllViews(NULL);
				CPolymeterView	*pView = theApp.GetMainFrame()->GetActiveMDIView();
				pView->SetSelection(pClipboard->m_arrSelection);
				pView->SetSelectionMark(pClipboard->m_nSelMark);
				pView->EnsureSelectionVisible();
			} else {	// deleting
				m_Seq.DeleteTracks(pClipboard->m_arrSelection);
				if (State.GetCode() == UCODE_MOVE)
					m_Seq.InsertTracks(State.GetCtrlID(), pClipboard->m_arrTrack);
				UpdateAllViews(NULL);
				CPolymeterView	*pView = theApp.GetMainFrame()->GetActiveMDIView();
				if (State.GetCode() == UCODE_MOVE) {
					int	iTrack = State.GetCtrlID();
					pView->SelectRange(iTrack, pClipboard->m_arrTrack.GetSize());
					pView->SetSelectionMark(iTrack);
					pView->EnsureSelectionVisible();
				} else
					pView->Deselect();
			}
		}
		break;
	}
}

CString CPolymeterDoc::GetUndoTitle(const CUndoState& State)
{
	CString	sTitle;
	switch (State.GetCode()) {
	case UCODE_TRACK_PROP:
	case UCODE_MULTI_TRACK_PROP:
		{
			int	iProp = HIWORD(State.GetCtrlID());
			sTitle.LoadString(CTrackDlg::GetPropertyCaptionId(iProp));
		}
		break;
	case UCODE_TRACK_BEAT:
		sTitle.LoadString(IDS_TRK_BEAT);
		break;
	case UCODE_MASTER_PROP:
		sTitle = GetPropertyName(State.GetCtrlID());
		break;
	default:
		sTitle.LoadString(m_nUndoTitleId[State.GetCode()]);
	}
	return sTitle;
}

void CPolymeterDoc::SecsToTime(int nSecs, CString& sTime)
{
	sTime.Format(_T("%d:%02d:%02d"), nSecs / 3600, nSecs % 3600 / 60, nSecs % 60);
}

int CPolymeterDoc::TimeToSecs(LPCTSTR pszTime)
{
	static const int PLACES = 3;	// hours, minutes, seconds
	int	ip[PLACES], op[PLACES];	// input and output place arrays
	ZeroMemory(op, sizeof(op));
	int	nPlaces = _stscanf_s(pszTime, _T("%d%*[: ]%d%*[: ]%d"), &ip[0], &ip[1], &ip[2]);
	if (nPlaces >= 0)
		CopyMemory(&op[PLACES - nPlaces], ip, nPlaces * sizeof(int));
	return(op[0] * 3600 + op[1] * 60 + op[2]);
}

#ifdef SHARED_HANDLERS

// Support for thumbnails
void CPolymeterDoc::OnDrawThumbnail(CDC& dc, LPRECT lprcBounds)
{
	// Modify this code to draw the document's data
	dc.FillSolidRect(lprcBounds, RGB(255, 255, 255));

	CString strText = _T("TODO: implement thumbnail drawing here");
	LOGFONT lf;

	CFont* pDefaultGUIFont = CFont::FromHandle((HFONT) GetStockObject(DEFAULT_GUI_FONT));
	pDefaultGUIFont->GetLogFont(&lf);
	lf.lfHeight = 36;

	CFont fontDraw;
	fontDraw.CreateFontIndirect(&lf);

	CFont* pOldFont = dc.SelectObject(&fontDraw);
	dc.DrawText(strText, lprcBounds, DT_CENTER | DT_WORDBREAK);
	dc.SelectObject(pOldFont);
}

// Support for Search Handlers
void CPolymeterDoc::InitializeSearchContent()
{
	CString strSearchContent;
	// Set search contents from document's data. 
	// The content parts should be separated by ";"

	// For example:  strSearchContent = _T("point;rectangle;circle;ole object;");
	SetSearchContent(strSearchContent);
}

void CPolymeterDoc::SetSearchContent(const CString& value)
{
	if (value.IsEmpty())
	{
		RemoveChunk(PKEY_Search_Contents.fmtid, PKEY_Search_Contents.pid);
	}
	else
	{
		CMFCFilterChunkValueImpl *pChunk = NULL;
		ATLTRY(pChunk = new CMFCFilterChunkValueImpl);
		if (pChunk != NULL)
		{
			pChunk->SetTextValue(PKEY_Search_Contents, value, CHUNK_TEXT);
			SetChunkValue(pChunk);
		}
	}
}

#endif // SHARED_HANDLERS

// CPolymeterDoc diagnostics

#ifdef _DEBUG
void CPolymeterDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CPolymeterDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

// CPolymeterDoc message map

BEGIN_MESSAGE_MAP(CPolymeterDoc, CDocument)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_COMMAND(ID_EDIT_REDO, OnEditRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, OnUpdateEditRedo)
	ON_COMMAND(ID_TOOLS_STATISTICS, OnToolsStatistics)
	ON_COMMAND(ID_FILE_EXPORT, OnFileExport)
END_MESSAGE_MAP()

// CPolymeterDoc commands

BOOL CPolymeterDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	TRY {
		ReadProperties(lpszPathName);
	}
	CATCH(CFileException, e) {
		e->ReportError();
		return FALSE;
	}
	CATCH(CUserException, e) {
		// invalid format; already reported
		return FALSE;
	}
	END_CATCH
	if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;
	return TRUE;
}

BOOL CPolymeterDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
	if (!CDocument::OnSaveDocument(lpszPathName))
		return FALSE;
	TRY {
		WriteProperties(lpszPathName);
	}
	CATCH(CFileException, e) {
		e->ReportError();
		return FALSE;
	}
	END_CATCH
	return TRUE;
}

void CPolymeterDoc::OnFileExport()
{
	int	nUsedTracks = m_Seq.GetUsedTrackCount(true);	// exclude muted tracks
	if (!nUsedTracks) {	// if no tracks to export
		AfxMessageBox(IDS_EXPORT_EMPTY_FILE);
		return;
	}
	if (nUsedTracks > USHRT_MAX) {	// if too many tracks for MIDI file
		AfxMessageBox(IDS_EXPORT_TOO_MANY_TRACKS);
		return;
	}
	CString	sFilter(LPCTSTR(IDS_EXPORT_FILE_FILTER));
	CPathStr	sDefName(GetTitle());
	sDefName.RemoveExtension();
	CFileDialog	fd(FALSE, _T(".mid"), sDefName, OFN_OVERWRITEPROMPT, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_EXPORT));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		const int	nDefaultDuration = 60;	// seconds
		CExportDlg	dlg;
		dlg.m_nDuration = theApp.GetProfileInt(REG_SETTINGS, RK_EXPORT_DURATION, nDefaultDuration);
		if (dlg.DoModal() == IDOK) {
			theApp.WriteProfileInt(REG_SETTINGS, RK_EXPORT_DURATION, dlg.m_nDuration);
			CWaitCursor	wc;
			if (!m_Seq.Export(fd.GetPathName(), dlg.m_nDuration)) {
				AfxMessageBox(IDS_EXPORT_ERROR);
			}
		}
	}
}

void CPolymeterDoc::OnEditUndo()
{
	GetUndoManager()->Undo();
	SetModifiedFlag();	// undo counts as modification
}

void CPolymeterDoc::OnUpdateEditUndo(CCmdUI *pCmdUI)
{
	CString	Text;
	Text.Format(LDS(IDS_EDIT_UNDO_FMT), GetUndoManager()->GetUndoTitle());
	pCmdUI->SetText(Text);
	pCmdUI->Enable(m_UndoMgr.CanUndo());
}

void CPolymeterDoc::OnEditRedo()
{
	GetUndoManager()->Redo();
	SetModifiedFlag();	// redo counts as modification
}

void CPolymeterDoc::OnUpdateEditRedo(CCmdUI *pCmdUI)
{
	CString	Text;
	Text.Format(LDS(IDS_EDIT_REDO_FMT), GetUndoManager()->GetRedoTitle());
	pCmdUI->SetText(Text);
	pCmdUI->Enable(m_UndoMgr.CanRedo());
}

void CPolymeterDoc::OnToolsStatistics()
{
	CSequencer::STATS	stats;
	m_Seq.GetStatistics(stats);
	double	fCBTimeMin, fCBTimeMax, fCBTimeAvg;
	if (stats.nCallbacks) {
		fCBTimeMin = stats.fCBTimeMin;
		fCBTimeMax = stats.fCBTimeMax;
		fCBTimeAvg = stats.fCBTimeSum / stats.nCallbacks;
	} else {
		fCBTimeMin = 0;
		fCBTimeMax = 0;
		fCBTimeAvg = 0;
	}
	double	fLatency =  m_Seq.GetLatency() / 1000.0;
	CString	s;
	s.Format(_T("Total Callbacks:\t%d\nOverrun Errors:\t%d\nCB Latency:\t%d ms (%d ticks)\nMin CB Time:\t%f (%2.1f%%)\nMax CB Time:\t%f (%2.1f%%)\nAvg CB Time:\t%f (%2.1f%%)\n"),
		stats.nCallbacks, stats.nOverruns, m_Seq.GetLatency(), m_Seq.GetCallbackLength(), 
		fCBTimeMin, fCBTimeMin / fLatency * 100,
		fCBTimeMax, fCBTimeMax / fLatency * 100,
		fCBTimeAvg, fCBTimeAvg / fLatency * 100);
	AfxMessageBox(s, MB_ICONINFORMATION);
}
