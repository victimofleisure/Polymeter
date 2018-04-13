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

// PolymeterDoc.h : interface of the CPolymeterDoc class
//


#pragma once

#include "Sequencer.h"
#include "TrackDlg.h"
#include "MasterProps.h"
#include "Undoable.h"

class CPolymeterDoc : public CDocument, public CMasterProps, public CUndoable, public CTrackBase
{
protected: // create from serialization only
	CPolymeterDoc();
	DECLARE_DYNCREATE(CPolymeterDoc)

// Attributes
public:
// Constants
	enum {
		TRACKS = 32,
	};
	enum {	// update hints
		HINT_NONE,				// no hint
		HINT_TRACK_PROP,		// track property edit; pHint is CPropHint
		HINT_MULTI_TRACK_PROP,	// multiple tracks property edit; pHint is CMultiTrackPropHint
		HINT_BEAT,				// beat edit; pHint is CPropHint
		HINT_MASTER_PROP,		// master property edit
		HINT_PLAY,				// start or stop playback
		HINTS
	};

// Types
	class CMySequencer : public CSequencer {
	public:
		virtual void OnMidiError(MMRESULT nResult);
	};
	class CPropHint : public CObject {
	public:
		CPropHint() {}
		CPropHint(int iTrack, int iProp) : m_iTrack(iTrack), m_iProp(iProp) {}
		int		m_iTrack;	// track index
		int		m_iProp;	// property index, or -1 for all
	};
	class CMultiTrackPropHint : public CObject {
	public:
		CMultiTrackPropHint() {}
		CMultiTrackPropHint(const CIntArrayEx& arrSelection, int iProp) 
			: m_arrSelection(arrSelection), m_iProp(iProp) {}
		CIntArrayEx	m_arrSelection;	// indices of selected tracks
		int		m_iProp;	// property index
	};

// Public data
	CMySequencer	m_Seq;		// sequencer instance
	int		m_nFileVersion;		// file version number
	CUndoManager	m_UndoMgr;	// undo manager
	CString	m_sGoToPosition;	// previous go to position string

// Operations
public:
	void	ReadProperties(LPCTSTR szPath);
	void	WriteProperties(LPCTSTR szPath) const;
	void	ApplyOptions();

// Overrides
public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	virtual void PreCloseFrame(CFrameWnd* pFrame );
#ifdef SHARED_HANDLERS
	virtual void InitializeSearchContent();
	virtual void OnDrawThumbnail(CDC& dc, LPRECT lprcBounds);
#endif // SHARED_HANDLERS
	virtual	CString	GetUndoTitle(const CUndoState& State);

// Implementation
public:
	virtual ~CPolymeterDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
// Types
	class CUndoClipboard : public CRefObj {
	public:
		CTrackArray	m_arrTrack;		// array of tracks
		CIntArrayEx	m_arrSelection;	// selection array
		int		m_nSelMark;			// selection mark
	};
	class CUndoMultiTrackPropEdit : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected tracks
		CVariantArray	m_arrVal;	// property values for selected tracks
	};

// Constants
	static const int	m_nUndoTitleId[];

// Overrides
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
	virtual	void	SaveUndoState(CUndoState& State);
	virtual	void	RestoreUndoState(const CUndoState& State);

// Helpers

#ifdef SHARED_HANDLERS
	// Helper function that sets search content for a Search Handler
	void SetSearchContent(const CString& value);
#endif // SHARED_HANDLERS

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnEditUndo();
	afx_msg void OnUpdateEditUndo(CCmdUI *pCmdUI);
	afx_msg void OnEditRedo();
	afx_msg void OnUpdateEditRedo(CCmdUI *pCmdUI);
	afx_msg void OnToolsStatistics();
	afx_msg void OnFileExport();
};
