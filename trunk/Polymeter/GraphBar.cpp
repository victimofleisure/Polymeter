// Copyleft 2019 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		25jan19	initial version
		01		30jan19	fix graph too small when system text size above 100%
		02		31jan19	if unable to write graph, show blank page
		03		10feb19	move temp file path wrapper to app
		04		09sep19	add tempo modulation color
		05		16mar20	add colors for new modulation types
		06		01apr20	standardize context menu handling
		07		04apr20	add color for chord modulation
		08		19apr20	don't set browser window name; fixes OLE exception
		09		08jun20	add color for offset modulation
		10		18jun20	add hint for layout menu items
		11		16oct20	specify layout via command line parameter
		12		18oct20	search both 32-bit and 64-bit program folders
		13		19oct20	after calling dot, verify that output file exists
		14		07jun21	rename rounding functions
		15		08jun21	fix warning for CString as variadic argument
		16		26jun21	add filtering by modulation type
		17		23jul21	eliminate spurious background tooltip
		18		03nov21	make save as filename default to document title
        19		11nov21	add graph depth attribute
		20		12nov21	add modulation type dialog for multiple filters
		21		13nov21	add optional legend to graph
		22		03jan22	add full screen mode
		23		05jul22	add parent window to modulation type dialog ctor
		24		23jul22	add option to exclude muted tracks
		25		13dec22	add export of formats other than SVG
		26		16feb23	add special handling for non-ASCII characters
		27		24feb23	add channel selection
		28		26feb23	move Graphviz path to app options
		29		27feb23	optimize channel selection
		30		20sep23	move use Cairo flag to app options
		31		19dec23	replace track type member usage with accessor
		32		18dec24	allow all unchecked in modulation type dialog
		33		18dec24	make filter and channels available to customize

*/

#include "stdafx.h"
#include "Polymeter.h"
#include "GraphBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "PathStr.h"
#include <math.h>
#include "RegTempl.h"
#include "FolderDialog.h"
#include "ProgressDlg.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CGraphBar

IMPLEMENT_DYNAMIC(CGraphBar, CMyDockablePane)

// X11 color name for each modulation type; see Graphviz documentation
#define MOD_TYPE_COLOR_Mute		"black"
#define MOD_TYPE_COLOR_Note		"red"
#define MOD_TYPE_COLOR_Velocity	"green"
#define MOD_TYPE_COLOR_Duration	"blue"
#define MOD_TYPE_COLOR_Range	"darkcyan"
#define MOD_TYPE_COLOR_Position	"magenta"
#define MOD_TYPE_COLOR_Tempo	"brown"
#define MOD_TYPE_COLOR_Scale	"purple"
#define MOD_TYPE_COLOR_Chord	"darkgreen"
#define MOD_TYPE_COLOR_Index	"olive"
#define MOD_TYPE_COLOR_Voicing	"orange"
#define MOD_TYPE_COLOR_Offset	"turquoise"

// adding a new modulation type requires adding a new color name above
const LPCTSTR CGraphBar::m_arrModTypeColor[CTrack::MODULATION_TYPES] = {
	#define MODTYPEDEF(name) _T(MOD_TYPE_COLOR_##name),
	#include "TrackDef.h"
};

const LPCTSTR CGraphBar::m_arrGraphScopeInternalName[GRAPH_SCOPES] = {
	#define GRAPHSCOPEDEF(name) _T(#name),
	#include "GraphTypeDef.h"
};

const int CGraphBar::m_arrGraphScopeID[GRAPH_SCOPES] = {
	#define GRAPHSCOPEDEF(name) IDS_GRAPH_SCOPE_##name,
	#include "GraphTypeDef.h"
};

const int CGraphBar::m_arrHintGraphScopeID[GRAPH_SCOPES] = {
	#define GRAPHSCOPEDEF(name) IDS_HINT_GRAPH_SCOPE_##name,
	#include "GraphTypeDef.h"
};

const LPCTSTR CGraphBar::m_arrGraphLayout[GRAPH_LAYOUTS] = {
	#define GRAPHLAYOUTDEF(name) _T(#name),
	#include "GraphTypeDef.h"
};

const LPCTSTR CGraphBar::m_arrGraphExeName = _T("dot.exe");
const LPCTSTR CGraphBar::m_arrGraphFindExeName[] = {
	// Graphviz folder is identified by the existence of these executables
	_T("dot.exe"),
	_T("gvgen.exe"),
	_T("gvpack.exe"),
};

#define RK_GRAPH_SCOPE _T("Scope")
#define RK_GRAPH_DEPTH _T("Depth")
#define RK_GRAPH_LAYOUT _T("Layout")
#define RK_EDGE_LABELS _T("EdgeLabels")
#define RK_SHOW_LEGEND _T("Legend")
#define RK_HIGHLIGHT_SELECT _T("HighlightSelect")
#define RK_SHOW_MUTED _T("ShowMuted")
#define RK_EXPORT_SIZE_MODE _T("ExportSizeMode")
#define RK_EXPORT_WIDTH _T("ExportWidth")
#define RK_EXPORT_HEIGHT _T("ExportHeight")

#define MAKE_MOD_TYPE_MASK(iType) (static_cast<MOD_TYPE_MASK>(1) << iType)
#define MAKE_CHANNEL_MASK(iType) (static_cast<USHORT>(1) << iType)

CGraphBar::CGraphBar()
{
	m_iGraphState = GTS_IDLE;
	m_iGraphScope = GS_SOURCES;
	m_nGraphDepth = 1;
	m_iGraphLayout = GL_dot;
	m_iGraphFilter = -1;
	m_nGraphFilterMask = 0;
	m_iGraphChannel = -1;
	m_nGraphChannelMask = 0;
	m_iZoomLevel = 0;
	m_fZoomStep = 1.25;
	m_bUpdatePending = false;
	m_bHighlightSelect = true;
	m_bEdgeLabels = false;
	m_bShowLegend = false;
	m_bGraphvizFound = false;
	m_bShowMuted = true;
	// read graph scope string from registry and try to map it to index
	CString	sScope(theApp.GetProfileString(RK_GraphBar, RK_GRAPH_SCOPE));
	int iScope = ARRAY_FIND(m_arrGraphScopeInternalName, sScope);
	if (iScope >= 0)	// if string was found
		m_iGraphScope = iScope;
	// read graph layout string from registry and try to map it to index
	CString	sLayout(theApp.GetProfileString(RK_GraphBar, RK_GRAPH_LAYOUT));
	int iLayout = ARRAY_FIND(m_arrGraphLayout, sLayout);
	if (iLayout >= 0)	// if string was found
		m_iGraphLayout = iLayout;
	RdReg(RK_GraphBar, RK_GRAPH_DEPTH, m_nGraphDepth);
	RdReg(RK_GraphBar, RK_HIGHLIGHT_SELECT, m_bHighlightSelect);
	RdReg(RK_GraphBar, RK_EDGE_LABELS, m_bEdgeLabels);
	RdReg(RK_GraphBar, RK_SHOW_LEGEND, m_bShowLegend);
	RdReg(RK_GraphBar, RK_SHOW_MUTED, m_bShowMuted);
}

CGraphBar::~CGraphBar()
{
	theApp.WriteProfileString(RK_GraphBar, RK_GRAPH_SCOPE, m_arrGraphScopeInternalName[m_iGraphScope]);
	theApp.WriteProfileString(RK_GraphBar, RK_GRAPH_LAYOUT, m_arrGraphLayout[m_iGraphLayout]);
	WrReg(RK_GraphBar, RK_GRAPH_DEPTH, m_nGraphDepth);
	WrReg(RK_GraphBar, RK_HIGHLIGHT_SELECT, m_bHighlightSelect);
	WrReg(RK_GraphBar, RK_EDGE_LABELS, m_bEdgeLabels);
	WrReg(RK_GraphBar, RK_SHOW_LEGEND, m_bShowLegend);
	WrReg(RK_GraphBar, RK_SHOW_MUTED, m_bShowMuted);
}

bool CGraphBar::CreateBrowser()
{
	ASSERT(m_pBrowser == NULL);
	LPUNKNOWN lpUnk = m_wndBrowser.GetControlUnknown();
	HRESULT hr = lpUnk->QueryInterface(IID_IWebBrowser2, (void**)&m_pBrowser);
	if (!SUCCEEDED(hr)) {
		m_pBrowser = NULL;
		return false;
	}
	return true;
}

bool CGraphBar::Navigate(LPCTSTR pszURL)
{
	CString	sURL(pszURL);
	ASSERT(!sURL.IsEmpty());
	CComBSTR	bstrURL;
	bstrURL.Attach(sURL.AllocSysString());
	long	nFlags = navNoHistory;
	HRESULT	hr = m_pBrowser->Navigate(bstrURL,
		COleVariant(nFlags, VT_I4), NULL, NULL, NULL);
	return SUCCEEDED(hr);
}

void CGraphBar::SetZoomLevel(int iZoom)
{
	double	fZoom = pow(m_fZoomStep, iZoom);
	int	nZoomPct = Round(fZoom * BROWSER_ZOOM_PCT_INIT);
	if (nZoomPct >= BROWSER_ZOOM_PCT_MIN && nZoomPct <= BROWSER_ZOOM_PCT_MAX) {
		m_iZoomLevel = iZoom;
		VERIFY(SetBrowserZoom(nZoomPct));
	}
}

bool CGraphBar::SetBrowserZoom(int nZoomPct)
{
	if (m_pBrowser == NULL)
		return false;
	VARIANT vZoom;
	vZoom.vt = VT_I4;
	vZoom.lVal = nZoomPct;
	HRESULT	hr = m_pBrowser->ExecWB(OLECMDID_OPTICAL_ZOOM, OLECMDEXECOPT_DONTPROMPTUSER, &vZoom, NULL);
	return SUCCEEDED(hr);
}

bool CGraphBar::GetBrowserZoom(int& nZoomPct)
{
	if (m_pBrowser == NULL)
		return false;
	VARIANT vZoom;
	vZoom.vt = VT_I4;
	HRESULT	hr = m_pBrowser->ExecWB(OLECMDID_OPTICAL_ZOOM, OLECMDEXECOPT_DONTPROMPTUSER, NULL, &vZoom);
	if (FAILED(hr))
		return false;
	nZoomPct = vZoom.lVal;
	return true;
}

bool CGraphBar::GetBrowserZoomRange(int& nZoomMin, int& nZoomMax)
{
	if (m_pBrowser == NULL)
		return false;
	VARIANT vZoomRange;
	vZoomRange.vt = VT_I4;
	HRESULT	hr = m_pBrowser->ExecWB(OLECMDID_OPTICAL_GETZOOMRANGE, OLECMDEXECOPT_DONTPROMPTUSER, NULL, &vZoomRange);
	if (FAILED(hr))
		return false;
	nZoomMin = LOWORD(vZoomRange.lVal);
	nZoomMax = HIWORD(vZoomRange.lVal);
	return true;
}

bool CGraphBar::UpdateGraph()
{
	if (!m_bGraphvizFound) {	// if graphviz not found yet
		if (FindGraphviz()) {	// find graphviz; if successful
			m_bGraphvizFound = true;	// don't find again
		} else {	// graphviz not found
			theApp.GetMainFrame()->m_wndGraphBar.ShowPane(false, false, false);	// hide graph bar
			return false;	// disable graphing
		}
	}
	if (m_iGraphState != GTS_IDLE) {	// if graph thread already busy
		m_iGraphState = GTS_LATE;	// graph will be stale and must be recreated
		return true;
	}
	if (m_pBrowser == NULL) {	// if browser not created yet
		if (!CreateBrowser()) {	// create browser; if unsuccessful
			AfxMessageBox(GetLastErrorString());	// display last error
			return false;
		}
		Navigate(_T("about:blank"));	// this helps with initial sizing
	}
	CString	sDataPath;
	if (!theApp.GetTempFileName(sDataPath, _T("plm")))
		return false;
	m_tfpData.SetPath(sDataPath);
	int	nNodes;
	CRect	rc;
	GetClientRect(rc);
	CSize	szGraph(rc.Size());
	if (!WriteGraph(sDataPath, nNodes, szGraph) || !nNodes) {	// if can't write graph or graph is empty
		m_tfpData.Empty();
		m_tfpGraph.Empty();
		Navigate(_T("about:blank"));
		return false;
	}
	CAutoPtr<CGraphThreadParams>	pGraphParams(new CGraphThreadParams);	// dynamically allocate parameters
	LPCTSTR	pszDataPath = sDataPath;	// convert string to pointer to defeat reference counting
	pGraphParams->m_sDataPath = pszDataPath;	// copy from pointer, not from string instance
	pGraphParams->m_iGraphLayout = m_iGraphLayout;
	if (AfxBeginThread(GraphThread, pGraphParams, 0, 0, 0, NULL) != NULL)	// if thread launched
		pGraphParams.Detach();	// graph thread is responsible for deleting its parameters
	else	// thread didn't launch
		return false;	// auto pointer will delete parameters
	CPathStr	sGraphPath(sDataPath);
	sGraphPath.RenameExtension(_T(".svg"));	// same as data path except extension
	m_tfpGraph.SetPath(sGraphPath);
	m_iGraphState = GTS_BUSY;
	return true;
}

UINT CGraphBar::GraphThread(LPVOID pParam)
{
	CGraphThreadParams	*pGraphParams = static_cast<CGraphThreadParams*>(pParam);
	CGraphThreadParams	params(*pGraphParams);	// copy parameters from heap to local var
	delete pGraphParams;	// delete dynamically allocated parameter instance
	CString	sDataPath(params.m_sDataPath);
	CPathStr	sGraphPath(sDataPath);
	sGraphPath.RenameExtension(_T(".svg"));	// same as data path except extension
	LPCTSTR	pszLayout = m_arrGraphLayout[params.m_iGraphLayout];
	CPathStr	sExePath(theApp.m_Options.m_Graph_sGraphvizFolder);
	sExePath.Append(m_arrGraphExeName);
	LPCTSTR	pszRender;
	if (theApp.m_Options.m_Graph_bGraphUseCairo)
		pszRender = _T(":cairo");	// fix for Graphviz bug #1855; see above
	else	// default render
		pszRender = _T("");
	CString	sCmdLine;
	sCmdLine.Format(_T("%s -K%s -Tsvg%s -o\"%s\" \"%s\""), 
		sExePath.GetString(), pszLayout, pszRender, sGraphPath.GetString(), sDataPath.GetString());
	TCHAR	*pCmdLine = sCmdLine.GetBuffer(0);
	STARTUPINFO	si;
	GetStartupInfo(&si);
	CProcessInformation	pi;	// dtor closes handles
	UINT	dwFlags = CREATE_NO_WINDOW;	// avoid flashing console window
	DWORD	dwExitMsg = UWM_GRAPH_ERROR;	// assume failure
	DWORD	dwExitParam = 0;
	if (CreateProcess(NULL, pCmdLine, NULL, NULL, FALSE, dwFlags, NULL, NULL, &si, &pi)) {
		static const int GRAPH_TIMEOUT = 60000;	// generous timeout in milliseconds
		int	nStatus = WaitForSingleObject(pi.hThread, GRAPH_TIMEOUT);
		switch (nStatus) {
		case WAIT_OBJECT_0:
			if (PathFileExists(sGraphPath))
				dwExitMsg = UWM_GRAPH_DONE;
			else
				dwExitParam = ERROR_FILE_NOT_FOUND;
			break;
		case WAIT_TIMEOUT:
			dwExitParam = ERROR_TIMEOUT;
			break;
		default:
			dwExitParam = GetLastError();
		}
	} else {	// create process error
		dwExitParam = GetLastError();
	}
	CWnd&	wndParent = theApp.GetMainFrame()->m_wndGraphBar;
	if (!wndParent.PostMessage(dwExitMsg, dwExitParam)) {	// if post message fails
		DeleteFile(sDataPath);	// try to clean up temp files
		DeleteFile(sGraphPath);
	}
	return 0;
}

bool CGraphBar::ExportGraph(CString sOutFormat, CString sOutPath, const CSize& szGraph)
{
	CString	sDataPath;	// don't touch member files, create our own
	if (!theApp.GetTempFileName(sDataPath, _T("plm")))
		return false;
	CTempFilePath	tfpData(sDataPath);
	int	nNodes;
	if (!WriteGraph(sDataPath, nNodes, szGraph, true) || !nNodes)
		return false;
	LPCTSTR	pszLayout = m_arrGraphLayout[m_iGraphLayout];
	CPathStr	sExePath(theApp.m_Options.m_Graph_sGraphvizFolder);
	sExePath.Append(m_arrGraphExeName);
	LPCTSTR	pszRender;
	if (theApp.m_Options.m_Graph_bGraphUseCairo)
		pszRender = _T(":cairo");	// fix for Graphviz bug #1855; see above
	else	// default render
		pszRender = _T("");
	CString	sCmdLine;
	sCmdLine.Format(_T("%s -K%s -T%s%s -o\"%s\" \"%s\""), // similar to GraphThread except output format is variable
		sExePath.GetString(), pszLayout, sOutFormat.GetString(), pszRender, sOutPath.GetString(), sDataPath.GetString());
	TCHAR	*pCmdLine = sCmdLine.GetBuffer(0);
	STARTUPINFO	si;
	GetStartupInfo(&si);
	CProcessInformation	pi;	// dtor closes handles
	UINT	dwFlags = CREATE_NO_WINDOW;	// avoid flashing console window
	DWORD	dwError = ERROR_SUCCESS;	// assume success
	if (CreateProcess(NULL, pCmdLine, NULL, NULL, FALSE, dwFlags, NULL, NULL, &si, &pi)) {
		CProgressDlg	dlg;
		dlg.Create(this);	// show progress bar
		dlg.SetMarquee();	// process doesn't notify us of its progress, so set marquee mode
		const DWORD	nTimeout = 100;	// polling period in milliseconds
		DWORD	nWaitStatus;
		do {	// poll thread handle until Graphviz process exits
			dlg.PumpMessages();	// keep UI responsive
			nWaitStatus = WaitForSingleObject(pi.hThread, nTimeout);
		} while (nWaitStatus == WAIT_TIMEOUT);
		if (nWaitStatus == WAIT_OBJECT_0) {	// if thread handle signaled
			if (!PathFileExists(sOutPath))	// if output file doesn't exist
				dwError = ERROR_FILE_NOT_FOUND;
		} else {	// wait failed
			dwError = GetLastError();
		}
	} else {	// create process error
		dwError = GetLastError();
	}
	if (dwError != ERROR_SUCCESS) {
		AfxMessageBox(FormatSystemError(dwError));
	}
	return dwError == ERROR_SUCCESS;
}

bool CGraphBar::WriteGraph(LPCTSTR pszPath, int& nNodes, const CSize& szGraph, bool bIsExporting) const
{
	nNodes = 0;
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc == NULL)
		return false;
	// Graphviz has a DPI setting which defaults to 96. It can be overriden, but doing so
	// breaks the SVG export (it outputs an incorrect view size). The solution is to use
	// the default Graphviz DPI instead of the system DPI when calculating the page size.
	// A small correction is added to the DPI to avoid spurious scroll bars in some cases.
	double	fGraphvizDPI;
	if (bIsExporting) {	// if exporting graph to file
		fGraphvizDPI = 96;
	} else {	// displaying graph in browser control
		fGraphvizDPI = 96.1;	// this dubious kludge helps avoid scroll bars
	}
	double	fWidth = szGraph.cx / fGraphvizDPI;	// width in inches
	double	fHeight = szGraph.cy / fGraphvizDPI;	// height in inches
	// Unicode is optional because UTF-8 slows down writing by an order of magnitude
	CStdioFileEx	fout(pszPath, CFile::modeCreate | CFile::modeWrite, theApp.m_Options.m_Graph_bGraphUnicode);
	if (theApp.m_Options.m_Graph_bGraphUnicode) {	// if writing UTF-8
		fout.SeekToBegin();	// CStdioFileEx writes BOM, but Graphviz expects UTF-8 without BOM, so overwrite BOM
	}
	_fputts(_T("digraph {\n"), fout.m_pStream);
	_ftprintf(fout.m_pStream, _T("graph[size=\"%g,%g\",ratio=fill];\n"), fWidth, fHeight);
	_fputts(_T("overlap=false;\nsplines=true;\n"), fout.m_pStream);
	_fputts(_T("node[fontname=\"Helvetica\"];\nedge[fontname=\"Helvetica\"];\n"), fout.m_pStream);
	int	nTracks = pDoc->GetTrackCount();
	CBoolArrayEx	arrRef;
	arrRef.SetSize(nTracks);
	CTrackBase::CPackedModulationArray	arrMod;
	MOD_TYPE_MASK	nModTypeUsedMask = 0;
	if (m_iGraphScope == GS_ALL) {	// if showing all modulations
		pDoc->m_Seq.GetModulations(arrMod);	// simple case
	} else {	// showing modulations only for selected tracks
		UINT	nLinkFlags;
		switch (m_iGraphScope) {
		case GS_SOURCES:
			nLinkFlags = CTrackArray::MODLINKF_SOURCE;
			break;
		case GS_TARGETS:
			nLinkFlags = CTrackArray::MODLINKF_TARGET;
			break;
		case GS_BIDIRECTIONAL:
			nLinkFlags = CTrackArray::MODLINKF_SOURCE | CTrackArray::MODLINKF_TARGET;
			break;
		default:
			ASSERT(0);	// logic error
			nLinkFlags = 0;
		}
		int	nLevels;
		if (m_nGraphDepth > 0)	// if explicit depth
			nLevels = m_nGraphDepth;
		else	// unlimited depth
			nLevels = INT_MAX;
		pDoc->m_Seq.GetTracks().GetLinkedTracks(pDoc->m_arrTrackSel, arrMod, nLinkFlags, nLevels);
	}
	CBoolArrayEx	arrOnSelectedChannel;
	WORD	nChannelMask = m_nGraphChannelMask;
	if (nChannelMask) {	// if channels specified
		ApplyChannelMask(pDoc, arrMod, arrOnSelectedChannel);
	}
	MOD_TYPE_MASK	nModTypeFilterMask;
	if (m_iGraphFilter >= 0) {	// if modulation type filter index is valid
		if (m_iGraphFilter >= GRAPH_FILTER_MULTI)	// if multiple filters
			nModTypeFilterMask = m_nGraphFilterMask;	// use filter bitmask
		else	// single modulation type is selected
			nModTypeFilterMask = MAKE_MOD_TYPE_MASK(m_iGraphFilter);	// make bitmask for this type
	} else	// no filter
		nModTypeFilterMask = static_cast<MOD_TYPE_MASK>(-1);
	CString	sLine;
	int	nMods = arrMod.GetSize();
	for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
		const CTrackBase::CPackedModulation&	mod = arrMod[iMod];
		if (mod.m_iSource >= 0) {	// if valid modulation source track index
			// if excluding muted tracks and modulation's source or target is muted
			if (!m_bShowMuted && (pDoc->m_Seq.GetMute(mod.m_iSource) || pDoc->m_Seq.GetMute(mod.m_iTarget)))
				continue;	// skip this modulation
			// if channels specified and modulation targets an excluded channel
			if (nChannelMask && !arrOnSelectedChannel[mod.m_iTarget])
				continue;	// skip this modulation
			MOD_TYPE_MASK	nModTypeBit = MAKE_MOD_TYPE_MASK(mod.m_iType);	// make bitmask for this type
			if (nModTypeFilterMask & nModTypeBit) {	// if modulation's type passes filter
				nModTypeUsedMask |= nModTypeBit;	// set corresponding bit in mask of types used in graph
				if (m_bEdgeLabels) {	// if showing edge labels
					_ftprintf(fout.m_pStream, _T("%d->%d[color=%s,label=\"%s\",fontcolor=%s];\n"), 
						mod.m_iSource + 1, mod.m_iTarget + 1, m_arrModTypeColor[mod.m_iType], 
						CTrack::GetModulationTypeName(mod.m_iType).GetString(), m_arrModTypeColor[mod.m_iType]);
				} else {	// no edge labels
					_ftprintf(fout.m_pStream, _T("%d->%d[color=%s];\n"), 
						mod.m_iSource + 1, mod.m_iTarget + 1, m_arrModTypeColor[mod.m_iType]);
				}
				arrRef[mod.m_iSource] = true;	// mark source and target tracks as referenced
				arrRef[mod.m_iTarget] = true;
			}
		}
	}
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		if (arrRef[iTrack]) {	// if at least one edge references this track
			const CTrack& trk = pDoc->m_Seq.GetTrack(iTrack);
			CString	sName;
			if (trk.m_sName.IsEmpty())	// if track name empty
				sName.Format(_T("%d"), iTrack + 1);	// use track number instead
			else {	// track name available
				sName = trk.m_sName;
				int	iSplit = SplitLabel(sName);
				if (iSplit >= 0) {	// if split point found
					sName.SetAt(iSplit, 'n');	// replace space with escaped newline
					sName.Insert(iSplit, '\\');
				}
			}
			_ftprintf(fout.m_pStream, _T("%d[label=\"%s\"];\n"), iTrack + 1, sName.GetString());
			nNodes++;
		}
	}
	if (m_bHighlightSelect && m_iGraphScope != GS_ALL) {
		int	nSels = pDoc->GetSelectedCount();
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iTrack = pDoc->m_arrTrackSel[iSel];
			if (arrRef[iTrack])	// if at least one edge references this track
				_ftprintf(fout.m_pStream, _T("%d[style=filled,color=black,fillcolor=lightcyan2];\n"), iTrack + 1);
		}
	}
	if (m_bShowLegend) {	// if showing graph legend
		CString	s(_T("graph [fontname=\"Helvetica\" labelloc=\"b\" label=")
			_T("<<TABLE BORDER=\"0\"><TR>\n"));	// legend is a single table row
		for (int iModType = 0; iModType < CTrack::MODULATION_TYPES; iModType++) {	// for each modulation type
			if (nModTypeUsedMask & MAKE_MOD_TYPE_MASK(iModType)) {	// if graph uses this modulation type
				s += _T("<TD BGCOLOR=\"");	// add modulation type to legend
				s += m_arrModTypeColor[iModType];
				s += _T("\"> </TD><TD>");	// blank becomes color swatch
				s += CTrack::GetModulationTypeName(iModType);
				s += _T("</TD><TD> </TD>\n");	// extra cell adds margin
			}
		}
		s += _T("</TR><TR><TD> </TD></TR></TABLE>>]\n");	// add blank row to avoid clipping bottom
		_fputts(s, fout.m_pStream);
	}
	_fputts(_T("}\n"), fout.m_pStream);
	return true;
}

__forceinline void CGraphBar::ApplyChannelMask(CPolymeterDoc *pDoc, const CTrackBase::CPackedModulationArray& arrMod, CBoolArrayEx& arrOnSelectedChannel) const
{
	// This method determines which of the modulations in arrMod target the
	// MIDI channels specified by the channel bitmask in m_nGraphChannelMask.
	// The result is returned via the arrOnSelectedChannel array, which has an
	// element for each track, set true if modulations targeting that track
	// should be included. The modulations are processed in two stages:
	//
	// 1. A modulation is included if it targets a non-modulator track and the
	// bit corresponding to that track's channel is set in the channel bitmask.
	//
	// 2. A modulation is included if it targets a modulator track that directly
	// or indirectly targets a non-modulator track that was included in stage 1.
	//
	WORD	nChannelMask = m_nGraphChannelMask;	// copy member var
	int	nTracks = pDoc->GetTrackCount();
	arrOnSelectedChannel.SetSize(nTracks);	// allocate array
	int	nMods = arrMod.GetSize();
	for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
		const CTrackBase::CPackedModulation&	mod = arrMod[iMod];
		const CTrack& trkTarget = pDoc->m_Seq.GetTrack(mod.m_iTarget);
		if (!trkTarget.IsModulator()) {	// if target track isn't a modulator
			WORD	nTargetChannelBit = MAKE_CHANNEL_MASK(trkTarget.m_nChannel);
			if (nChannelMask & nTargetChannelBit) {	// if track's channel matches channel mask
				if (m_bShowMuted || !pDoc->m_Seq.GetMute(mod.m_iTarget)) {	// if showing muted tracks or target track is unmuted
					if (mod.m_iSource >= 0) {	// if valid modulation
						arrOnSelectedChannel[mod.m_iTarget] = true;	// include source track
						arrOnSelectedChannel[mod.m_iSource] = true;	// include target track
					}
				}
			}
		}
	}
	// A modulator track doesn't output MIDI events, hence its MIDI channel is
	// meaningless. To determine whether a modulator track should be included,
	// we must inspect any non-modulator tracks it targets, which may be
	// multiple hops away. We do this by propagating inclusion outward from
	// the non-modulator tracks, until further propagation is impossible.
	//
	bool	bPassChanged;
	do {
		bPassChanged = false;
		for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
			const CTrackBase::CPackedModulation&	mod = arrMod[iMod];
			const CTrack& trkTarget = pDoc->m_Seq.GetTrack(mod.m_iTarget);
			if (trkTarget.IsModulator()) {	// if target track is a modulator
				if (arrOnSelectedChannel[mod.m_iTarget]) {	// if target track included
					if (mod.m_iSource >= 0) {	// if valid modulation
						if (!arrOnSelectedChannel[mod.m_iSource]) {	// if source track not included
							arrOnSelectedChannel[mod.m_iSource] = true;	// include source track
							bPassChanged = true;	// pass made a change
						}
					}
				}
			}
		}
	} while (bPassChanged);	// loop until a pass makes no changes
}

int CGraphBar::SplitLabel(const CString& str)
{
	int	nLen = str.GetLength();
	if (!nLen)
		return -1;
	TCHAR	cSpace = ' ';
	int	iSplit = -1;
	int	iMid = nLen / 2;
	int	iRight, iLeft;
	for (iRight = iMid; iRight < nLen; iRight++) {	// for chars right of middle
		if (str[iRight] == cSpace)	// if space found
			break;
	}
	for (iLeft = iMid - 1; iLeft >= 0; iLeft--) {	// for chars left of middle
		if (str[iLeft] == cSpace)	// if space found
			break;
	}
	if (iRight < nLen) {	// if space on right
		if (iLeft >= 0) {	// if space on left
			if (iRight - iMid < iMid - iLeft)	// if right space is closer
				iSplit = iRight;	// split on right
			else	// left space is closer
				iSplit = iLeft;	// split on left
		} else	// no space on left
			iSplit = iRight;	// split on right
	} else {	// no space on right
		if (iLeft >= 0)	// if space on left
			iSplit = iLeft;	// split on left
	}
	return iSplit;
}

void CGraphBar::StartDeferredUpdate()
{
	if (!m_bUpdatePending) {
		PostMessage(UWM_DEFERRED_UPDATE);
		m_bUpdatePending = true;
	}
}

__forceinline void CGraphBar::RebuildMuteCacheIfNeeded()
{
	if (!m_bShowMuted)	// if excluding muted tracks
		RebuildMuteCache();
}

__forceinline void CGraphBar::OnTrackMuteChange()
{
	if (!m_bShowMuted) {	// if excluding muted tracks
		StartDeferredUpdate();
		RebuildMuteCache();
	}
}

__forceinline void CGraphBar::OnTrackPropertyChange(int iProp)
{
	switch (iProp) {
	case CTrack::PROP_Name:	// if track name change
		StartDeferredUpdate();
		break;
	case CTrack::PROP_Mute:
		OnTrackMuteChange();
		break;
	case CTrack::PROP_Type:	// if track type change
	case CTrack::PROP_Channel:	// or channel change
		if (m_iGraphChannel >= 0)	// if graphing specific channels
			StartDeferredUpdate();
		break;
	}
}

void CGraphBar::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
//	printf("CGraphBar::OnUpdate %p %d %p\n", pSender, lHint, pHint);
	UNREFERENCED_PARAMETER(pSender);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
	case CPolymeterDoc::HINT_MODULATION:
		StartDeferredUpdate();
		RebuildMuteCacheIfNeeded();
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			OnTrackPropertyChange(pPropHint->m_iProp);
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			const CPolymeterDoc::CMultiItemPropHint	*pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			OnTrackPropertyChange(pPropHint->m_iProp);
		}
		break;
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		if (m_iGraphScope != GS_ALL)	// if selection matters
			StartDeferredUpdate();
		break;
	case CPolymeterDoc::HINT_SOLO:
		OnTrackMuteChange();
		break;
	case CPolymeterDoc::HINT_SONG_POS:
		if (!m_bShowMuted) {	// if excluding muted tracks
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			if (pDoc != NULL) {
				int	nTracks = pDoc->GetTrackCount();
				if (nTracks != m_arrMute.GetSize()) {	// if track count differs from cache size
					RebuildMuteCache();	// cache is stale, so rebuild it
				} else {	// track count matches; assume cache is valid
					bool	bMuteChanged = false;
					for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
						bool	bIsMuted = pDoc->m_Seq.GetMute(iTrack);
						if (bIsMuted != m_arrMute[iTrack]) {	// if track mute differs from cache
							m_arrMute[iTrack] = bIsMuted;	// update cache
							bMuteChanged = true;	// set change flag
						}
					}
					if (bMuteChanged)	// if one or more track mutes changed state
						StartDeferredUpdate();
				}
			}
		}
		break;
	case CPolymeterDoc::HINT_OPTIONS:
		{
			const CPolymeterDoc::COptionsPropHint	*pPropHint = static_cast<CPolymeterDoc::COptionsPropHint *>(pHint);
			// if Graphviz folder changed, mark graphviz not found
			if (theApp.m_Options.m_Graph_sGraphvizFolder != pPropHint->m_pPrevOptions->m_Graph_sGraphvizFolder) {
				m_bGraphvizFound = false;
			}
			// if Graph Unicode or Cairo options changed, redraw graph
			if (theApp.m_Options.m_Graph_bGraphUnicode != pPropHint->m_pPrevOptions->m_Graph_bGraphUnicode
			|| theApp.m_Options.m_Graph_bGraphUseCairo != pPropHint->m_pPrevOptions->m_Graph_bGraphUseCairo) {
				StartDeferredUpdate();
			}
		}
		break;
	}
}

void CGraphBar::RebuildMuteCache()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		int	nTracks = pDoc->GetTrackCount();
		m_arrMute.FastSetSize(nTracks);
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			m_arrMute[iTrack] = pDoc->m_Seq.GetMute(iTrack);	// cache mute state 
		}
	} else {	// no document
		m_arrMute.RemoveAll();
	}
}

void CGraphBar::OnShowChanged(bool bShow)
{
	if (bShow) {	// if showing
		RebuildMuteCacheIfNeeded();
		UpdateGraph();	// assume graph is stale
	}
}

void CGraphBar::DoContextMenu(CWnd* pWnd, CPoint point)
{
	if (FixContextMenuPoint(pWnd, point))
		return;
	CMenu	menu;
	VERIFY(menu.LoadMenu(IDR_GRAPH_CTX));
	UpdateMenu(this, &menu);
	CMenu	*pPopup = menu.GetSubMenu(0);
	CMenu	*pSubMenu;
	CStringArrayEx	arrItemStr;	// menu item strings
	// create graph scope submenu
	pSubMenu = pPopup->GetSubMenu(SM_GRAPH_SCOPE);
	ASSERT(pSubMenu != NULL);
	arrItemStr.SetSize(GRAPH_SCOPES);
	for (int iItem = 0; iItem < GRAPH_SCOPES; iItem++) {	// for each graph scope
		arrItemStr[iItem].LoadString(m_arrGraphScopeID[iItem]);
	}
	theApp.MakePopup(*pSubMenu, SMID_GRAPH_SCOPE_FIRST, arrItemStr, m_iGraphScope);
	// create graph layout submenu
	pSubMenu = pPopup->GetSubMenu(SM_GRAPH_LAYOUT);
	ASSERT(pSubMenu != NULL);
	arrItemStr.SetSize(GRAPH_LAYOUTS);
	for (int iItem = 0; iItem < GRAPH_LAYOUTS; iItem++) {	// for each graph layout engine
		arrItemStr[iItem] = m_arrGraphLayout[iItem];
	}
	theApp.MakePopup(*pSubMenu, SMID_GRAPH_LAYOUT_FIRST, arrItemStr, m_iGraphLayout);
	// create graph filter submenu
	pSubMenu = pPopup->GetSubMenu(SM_GRAPH_FILTER);
	ASSERT(pSubMenu != NULL);
	pSubMenu->RemoveMenu(MF_BYCOMMAND, ID_GRAPH_SELECT_MODULATION_TYPES);	// exists only for customizing UI
	arrItemStr.SetSize(GRAPH_FILTERS);
	arrItemStr[0] = LDS(IDS_FILTER_ALL);	// wildcard comes first
	for (int iItem = 0; iItem < CTrack::MODULATION_TYPES; iItem++) {	// for each modulation type
		arrItemStr[iItem + 1] = CTrack::GetModulationTypeName(iItem);	// skip wildcard
	}
	arrItemStr[GRAPH_FILTER_MULTI + 1] = LDS(IDS_GRAPH_FILTER_MULTI);
	theApp.MakePopup(*pSubMenu, SMID_GRAPH_FILTER_FIRST, arrItemStr, m_iGraphFilter + 1);
	// create graph channel submenu
	pSubMenu = pPopup->GetSubMenu(SM_GRAPH_CHANNEL);
	ASSERT(pSubMenu != NULL);
	pSubMenu->RemoveMenu(MF_BYCOMMAND, ID_GRAPH_SELECT_CHANNELS);	// exists only for customizing UI
	arrItemStr.SetSize(MIDI_CHANNELS + 2);	// two extra items, one for wildcard, one for multi
	arrItemStr[0] = LDS(IDS_FILTER_ALL);	// wildcard comes first
	CString	s;
	for (int iItem = 1; iItem <= MIDI_CHANNELS; iItem++) {	// for each channel
		s.Format(_T("%d"), iItem);
		arrItemStr[iItem] = s;
	}
	arrItemStr[MIDI_CHANNELS + 1] = LDS(IDS_CHANNEL_FILTER_MULTI);	// multi comes last
	theApp.MakePopup(*pSubMenu, SMID_GRAPH_CHANNEL_FIRST, arrItemStr, m_iGraphChannel + 1);
	// create graph depth submenu
	pSubMenu = pPopup->GetSubMenu(SM_GRAPH_DEPTH);
	ASSERT(pSubMenu != NULL);
	VERIFY(theApp.InsertNumericMenuItems(pSubMenu, ID_GRAPH_DEPTH_MAX, 
		SMID_GRAPH_DEPTH_FIRST + 1, 1, GRAPH_DEPTHS - 1, true));	// insert after
	MENUITEMINFO itemInfo;
	itemInfo.cbSize = sizeof(itemInfo);
	itemInfo.fMask = MIIM_ID;
	itemInfo.wID = SMID_GRAPH_DEPTH_FIRST;
	VERIFY(pSubMenu->SetMenuItemInfo(0, &itemInfo, true));
	pSubMenu->CheckMenuRadioItem(0, GRAPH_DEPTHS, m_nGraphDepth, MF_BYPOSITION);
	// display the context menu
	pPopup->TrackPopupMenu(0, point.x, point.y, this);
}

BOOL CGraphBar::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		bool	bIsCtrl = (GetKeyState(VK_CONTROL) & GKS_DOWN) != 0;
		switch (pMsg->wParam) {
		case VK_OEM_PLUS:
			if (bIsCtrl) {
				SetZoomLevel(m_iZoomLevel + 1);
				return true;
			}
			break;
		case VK_OEM_MINUS:
			if (bIsCtrl) {
				SetZoomLevel(m_iZoomLevel - 1);
				return true;
			}
			break;
		case '0':
			if (bIsCtrl) {
				SetZoomLevel(0);
				return true;
			}
			break;
		case VK_F11:
			if (bIsCtrl)
				SetFullScreen(!IsFullScreen());
			break;
		case VK_ESCAPE:
			SetFullScreen(false);
			break;
		}
	}
	return CMyDockablePane::PreTranslateMessage(pMsg);
}

BOOL CGraphBar::CBrowserWnd::PreTranslateMessage(MSG *pMsg)
{
	switch (pMsg->message) {
	case WM_SYSKEYDOWN:
		if (pMsg->wParam == VK_F10) {
			CGraphBar	*pGraphBar = STATIC_DOWNCAST(CGraphBar, GetParent());
			ASSERT(pGraphBar != NULL);
			if (pGraphBar != NULL) {
				pGraphBar->DoContextMenu(pGraphBar, CPoint(-1, -1));
			}
			return true;
		}
		break;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		return true;
	case WM_RBUTTONUP:
		{
			// replace browser's context menu with graph bar's
			CGraphBar	*pGraphBar = STATIC_DOWNCAST(CGraphBar, GetParent());
			ASSERT(pGraphBar != NULL);
			if (pGraphBar != NULL) {
				CPoint	ptCursor;
				GetCursorPos(&ptCursor);
				pGraphBar->DoContextMenu(pGraphBar, ptCursor);
			}
		}
		return true;
	}
	return CWnd::PreTranslateMessage(pMsg);
}

bool CGraphBar::FindGraphvizExes(CString sFolderPath)
{
	int	nExes = _countof(m_arrGraphFindExeName);
	for (int iExe = 0; iExe < nExes; iExe++) {	// for each executable
		CPathStr	sExePath(sFolderPath);
		sExePath.Append(m_arrGraphFindExeName[iExe]);
		if (!PathFileExists(sExePath))	// if layout executable not found
			return false;
	}
	return true;	// all layout executables found
}

bool CGraphBar::FindGraphvizExesFlexible(CPathStr& sFolderPath)
{
	if (FindGraphvizExes(sFolderPath))	// if executables found in specified folder
		return true;
	// executables not found; try bin subfolder
	CString	sOriginalPath(sFolderPath);
	sFolderPath.Append(_T("bin"));
	if (FindGraphvizExes(sFolderPath))
		return true;
	// executables still not found; try release\bin subfolder, as seen in portable package
	sFolderPath = sOriginalPath;
	sFolderPath.Append(_T("release\\bin"));
	return FindGraphvizExes(sFolderPath);	// last chance
}

bool CGraphBar::FindGraphvizFast(CString& sGraphvizPath)
{
	// assume graphviz was installed in its default location: a subfolder of program files,
	// the name of which starts with graphviz, containing an executables subfolder named bin
	sGraphvizPath.Empty();
	static const int arrFolder[] = {CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86};
	for (int iFolder = 0; iFolder < _countof(arrFolder); iFolder++) {
		CString	sProgramFiles;
		if (!theApp.GetSpecialFolderPath(arrFolder[iFolder], sProgramFiles)) {
			ASSERT(0);	// drastic error, should never happen
			return false;
		}
		CFileFind	finder;
		CString	sWildcard(sProgramFiles + _T("\\*.*"));
		BOOL bWorking = finder.FindFile(sWildcard);
		while (bWorking) {	// iterate subfolders of program files (x86)
			bWorking = finder.FindNextFile();
			if (finder.IsDirectory() && !finder.IsDots()	// if subfolder
			&& !finder.GetFileName().Left(8).CompareNoCase(_T("graphviz"))) {	// and name matches
				CPathStr	sBinSubfolder(finder.GetFilePath());
				sBinSubfolder.Append(_T("bin"));	// search bin subfolder
				if (FindGraphvizExes(sBinSubfolder)) {	// if needed executables found
					sGraphvizPath = sBinSubfolder;
					return true;	// all is good
				}
			}
		}
	}
	return false;	// not found
}

bool CGraphBar::FindGraphviz()
{
	CString&	sGraphvizPath = theApp.m_Options.m_Graph_sGraphvizFolder;	// alias for brevity
	// if graphviz path exists, verify needed executables are present at specified location
	if (!sGraphvizPath.IsEmpty() && FindGraphvizExes(sGraphvizPath))
		return true;	// all is good
	if (FindGraphvizFast(sGraphvizPath))	// graphviz is missing: try default location first
		return true;	// all is good
	// graphviz not found in default location either; must involve user now
	int	nRet;
	nRet = AfxMessageBox(IDS_GRAPHVIZ_IS_IT_PRESENT, MB_YESNOCANCEL);	// is graphviz present on system?
	if (nRet == IDCANCEL)
		return false;	// user canceled
	bool	bIsGraphvizPresent = nRet == IDYES;
	bool	bKeepTrying = true;
	while (bKeepTrying) {	// while trying to find graphviz
		if (bIsGraphvizPresent) {	// if graphviz is supposedly present
			nRet = AfxMessageBox(IDS_GRAPHVIZ_IS_LOCATION_KNOWN, MB_YESNOCANCEL);	// is location of graphviz known?
			if (nRet == IDCANCEL)
				return false;	// user canceled
			if (nRet == IDYES) {	// if user claims to know where graphviz is
				CPathStr	sFolderPath;
				UINT	nFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
				CString	sTitle(LPCTSTR(IDS_GRAPHVIZ_BROWSE_TITLE));
				if (CFolderDialog::BrowseFolder(sTitle, sFolderPath, NULL, nFlags))	{	// browse for folder
					if (FindGraphvizExesFlexible(sFolderPath)) {	// if executables found in specified folder
						sGraphvizPath = sFolderPath;	// store path of containing folder
						AfxMessageBox(IDS_GRAPHVIZ_IS_READY, MB_ICONINFORMATION);
						return true;	// all is good
					}
				}
			} else {	// graphviz is supposedly present but user doesn't know where it is
				nRet = AfxMessageBox(IDS_GRAPHVIZ_OK_TO_SEARCH, MB_ICONQUESTION | MB_OKCANCEL);	// ok to search for graphviz?
				if (nRet == IDOK) {	// user agreed to search
					CStringArrayEx arrTargetFileName;
					// populate array of target filenames; one for each graphviz layout engine
					arrTargetFileName.SetSize(GRAPH_LAYOUTS);
					for (int iLayout = 0; iLayout < GRAPH_LAYOUTS; iLayout++) {	// for each layout engine
						arrTargetFileName[iLayout] = m_arrGraphLayout[iLayout] + CString(".exe");
					}
					CFileFindDlg	dlg;
					if (dlg.InitDlg(arrTargetFileName)) {	// if dialog initialized
						if (dlg.DoModal() == IDOK) {	// if executables found
							sGraphvizPath = dlg.GetContainingFolderPath();	// store path of containing folder
							AfxMessageBox(IDS_GRAPHVIZ_IS_READY, MB_ICONINFORMATION);
							return true;	// all is good
						}
					}
				}
			}
			bKeepTrying = AfxMessageBox(IDS_GRAPHVIZ_NOT_FOUND_RETRY, MB_YESNO) == IDYES;
		} else {	// user claims graphviz isn't present; display download instructions
			nRet = AfxMessageBox(IDS_GRAPHVIZ_DOWNLOAD_IT, MB_OKCANCEL);	// wait for user to download graphviz
			if (nRet == IDCANCEL)
				return false;	// user canceled
			if (FindGraphvizFast(sGraphvizPath)) {	// try default location first
				AfxMessageBox(IDS_GRAPHVIZ_IS_READY, MB_ICONINFORMATION);
				return true;	// all is good
			}
			bIsGraphvizPresent = true;	// assume graphviz is now present, but location is unknown; back to top of loop
		}
	}	// bottom of while loop
	return false;
}

/////////////////////////////////////////////////////////////////////////////
// CFileFindDlg

CGraphBar::CFileFindDlg::CFileFindDlg(CWnd *pParentWnd) : CDialog(IDD_PROCESSING, pParentWnd)
{
	m_pWorkerThread = NULL;
}

bool CGraphBar::CFileFindDlg::InitDlg(const CStringArrayEx& arrTargetFileName)
{
	m_finder.m_arrTargetFileName = arrTargetFileName;
	CStringArrayEx	arrDrive;
	if (!theApp.GetLogicalDriveStringArray(arrDrive)) {	// get logical drive strings
		AfxMessageBox(GetLastErrorString());
		return false;	// system error
	}
	// filter logical drives to exclude drive types we're not interested in
	for (int iDrive = 0; iDrive < arrDrive.GetSize(); iDrive++) {	// for each drive in system
		switch (GetDriveType(arrDrive[iDrive])) {
		case DRIVE_FIXED:	// keep it simple: avoid network drives etc.
		case DRIVE_REMOVABLE:	// but allow thumb drives for portable users
			m_arrDrive.Add(arrDrive[iDrive]);
			break;
		}
	}
	if (m_arrDrive.IsEmpty()) {	// if no drives passed filter
		ASSERT(0);	// no system drive? shouldn't happen
		return false;
	}
	return true;
}

BOOL CGraphBar::CFileFindDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	// create thread in suspended state so we can disable auto-delete before thread runs;
	// lets us safely access thread handle, which is needed to wait for thread completion
	m_pWorkerThread = AfxBeginThread(WorkerFunc, this, 0, 0, CREATE_SUSPENDED);
	ASSERT(m_pWorkerThread != NULL);
	if (m_pWorkerThread != NULL) {	// if worker thread created
		m_pWorkerThread->m_bAutoDelete = false;	// so we can safely access thread handle
		m_pWorkerThread->ResumeThread();	// start thread running
	}
	return TRUE;
}

void CGraphBar::CFileFindDlg::OnCancel()
{
	m_finder.m_bIsCanceled = true;	// request worker thread to cancel find
	CDialog::OnCancel();	// let base class close dialog
}

void CGraphBar::CFileFindDlg::Work()
{
	bool	bIsFound = false;
	for (int iDrive = 0; iDrive < m_arrDrive.GetSize(); iDrive++) {	// for each logical drive
		PostMessage(UWM_GRAPHVIZ_FIND_DRIVE, iDrive);	// notify dialog that we started a new drive
		if (!m_finder.Recurse(m_arrDrive[iDrive])) {	// search entire drive; if iteration aborted
			bIsFound = !m_finder.m_bIsCanceled;	// success, unless find was canceled
			break;
		}
	}
	PostMessage(UWM_GRAPHVIZ_FIND_DONE, bIsFound);	// notify dialog
}

UINT CGraphBar::CFileFindDlg::WorkerFunc(LPVOID pParam)
{
	CFileFindDlg	*pDlg = static_cast<CFileFindDlg*>(pParam);
	ASSERT(pDlg != NULL);	// else logic error
	pDlg->Work();
	return 0;
}

BEGIN_MESSAGE_MAP(CGraphBar::CFileFindDlg, CDialog)
	ON_WM_DESTROY()
	ON_MESSAGE(UWM_GRAPHVIZ_FIND_DONE, OnFindDone)
	ON_MESSAGE(UWM_GRAPHVIZ_FIND_DRIVE, OnFindDrive)
END_MESSAGE_MAP()

void CGraphBar::CFileFindDlg::OnDestroy()
{
	if (m_pWorkerThread != NULL) {	// if worker thread created
		// wait for worker thread to exit; this is why we needed to disable auto-delete
		WaitForSingleObject(m_pWorkerThread->m_hThread, INFINITE);	// no timeout
		delete m_pWorkerThread;	// we're responsible for deleting thread instance
		m_pWorkerThread = NULL;	// mark thread deleted just to be safe
	}
	CDialog::OnDestroy();
}

LRESULT	CGraphBar::CFileFindDlg::OnFindDone(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	if (wParam)	// if find succeeded
		OnOK();
	else	// files not found, or find was canceled
		OnCancel();
	return 0;
}

LRESULT	CGraphBar::CFileFindDlg::OnFindDrive(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	CWnd	*pStatusWnd = GetDlgItem(IDC_PROCESSING_STATUS);
	ASSERT(pStatusWnd != NULL);
	pStatusWnd->SetWindowText(LDS(IDS_GRAPHVIZ_SEARCHING_DRIVE) + ' ' + m_arrDrive[wParam]);
	return 0;
}

CGraphBar::CFileFindDlg::CMyRecursiveFileFind::CMyRecursiveFileFind()
{
	m_bIsCanceled = false;
}

bool CGraphBar::CFileFindDlg::CMyRecursiveFileFind::OnFile(const CFileFind& finder)
{
	if (m_bIsCanceled)	// if cancel flag set
		return false;	// abort
	if (finder.IsDirectory())	// if directory
		return true;	// continue iterating
	if (m_arrTargetFileName[0].CompareNoCase(finder.GetFileName()))	// if first target file not found
		return true;	// continue iterating
	CPathStr	sContainingFolderPath(finder.GetFilePath());
	sContainingFolderPath.RemoveFileSpec();
	int	nTargets = m_arrTargetFileName.GetSize();
	for (int iTarget = 1; iTarget < nTargets; iTarget++) {	// for remaining target files
		CPathStr	sTargetPath(sContainingFolderPath);
		sTargetPath.Append(m_arrTargetFileName[iTarget]);
		if (!PathFileExists(sTargetPath))	// if target file not found in containing folder
			return true;	// continue iterating
	}
	m_sContainingFolderPath = sContainingFolderPath;	// store containing folder path in member var
	return false;	// stop iterating, we're good
}

CGraphBar::CModulationTypeDlg::CModulationTypeDlg(CWnd* pParentWnd) : CDialog(IDD, pParentWnd)
{
}

BEGIN_MESSAGE_MAP(CGraphBar::CModulationTypeDlg, CDialog)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CModulationTypeDlg

void CGraphBar::CModulationTypeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MOD_TYPE_LIST, m_list);
}

BOOL CGraphBar::CModulationTypeDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	for (int iItem = 0; iItem < CTrack::MODULATION_TYPES; iItem++) {	// for each modulation type
		m_list.AddString(CTrack::GetModulationTypeName(iItem));	// insert list item for this type
		if (m_nModTypeMask & MAKE_MOD_TYPE_MASK(iItem))	// if corresponding bit is set within mask
			m_list.SetCheck(iItem, BST_CHECKED);	// check list item
	}
	return TRUE;
}

void CGraphBar::CModulationTypeDlg::OnOK()
{
	MOD_TYPE_MASK	nModTypeMask = 0;	// init mask to zero (no types selected)
	for (int iItem = 0; iItem < CTrack::MODULATION_TYPES; iItem++) {	// for each modulation type
		if (m_list.GetCheck(iItem) & BST_CHECKED)	// if list item is checked
			nModTypeMask |= MAKE_MOD_TYPE_MASK(iItem);	// set corresponding bit within mask
	}
	m_nModTypeMask = nModTypeMask;
	CDialog::OnOK();
}

/////////////////////////////////////////////////////////////////////////////
// CImageExportDlg

CGraphBar::CImageExportDlg::CImageExportDlg(CWnd *pParentWnd) 
	: CDialog(IDD_IMAGE_EXPORT, pParentWnd)
{
	m_nSizeMode = 0;
	m_nCurSizeMode = 0;
	m_szImg = CSize(0, 0);
}

BOOL CGraphBar::CImageExportDlg::OnInitDialog()
{
	m_nSizeMode = theApp.GetProfileInt(RK_GraphBar, RK_EXPORT_SIZE_MODE, SM_SAME_SIZE_AS_WINDOW);
	m_szImg.cx = theApp.GetProfileInt(RK_GraphBar, RK_EXPORT_WIDTH, 1024);
	m_szImg.cy = theApp.GetProfileInt(RK_GraphBar, RK_EXPORT_HEIGHT, 768);
	CDialog::OnInitDialog();
	m_nCurSizeMode = -1;	// spoof no-op test
	UpdateUI(m_nSizeMode);
	return TRUE;  // return TRUE unless you set the focus to a control
}

void CGraphBar::CImageExportDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_IMAGE_EXPORT_SIZE_MODE_1, m_nSizeMode);
	DDX_Text(pDX, IDC_IMAGE_EXPORT_WIDTH_EDIT, m_szImg.cx);
	DDV_MinMaxInt(pDX, m_szImg.cx, 1, INT_MAX);
	DDX_Text(pDX, IDC_IMAGE_EXPORT_HEIGHT_EDIT, m_szImg.cy);
	DDV_MinMaxInt(pDX, m_szImg.cy, 1, INT_MAX);
}

void CGraphBar::CImageExportDlg::UpdateUI(int nSizeMode)
{
	if (nSizeMode == m_nCurSizeMode)	// if state unchanged
		return;	// nothing to do; handles duplicate notifications
	m_nCurSizeMode = nSizeMode;	// update shadow of size mode
	CWnd	*pWidthCtrl = GetDlgItem(IDC_IMAGE_EXPORT_WIDTH_EDIT);
	CWnd	*pHeightCtrl = GetDlgItem(IDC_IMAGE_EXPORT_HEIGHT_EDIT);
	ASSERT(pWidthCtrl != NULL && pHeightCtrl != NULL);	// controls must exist
	pWidthCtrl->EnableWindow(nSizeMode == SM_CUSTOM_SIZE);
	pHeightCtrl->EnableWindow(nSizeMode == SM_CUSTOM_SIZE);
	if (nSizeMode == SM_SAME_SIZE_AS_WINDOW) {
		CWnd	*pParentWnd = GetParent();	// use parent window's client size
		ASSERT(pParentWnd != NULL);
		if (pParentWnd != NULL) {
			CRect	rClient;
			pParentWnd->GetClientRect(rClient);
			m_szImg = rClient.Size();
			CString	sVal;
			sVal.Format(_T("%d"), m_szImg.cx);
			pWidthCtrl->SetWindowText(sVal);
			sVal.Format(_T("%d"), m_szImg.cy);
			pHeightCtrl->SetWindowText(sVal);
		}
	}
}

void CGraphBar::CImageExportDlg::OnClickedSizeMode(UINT nID)
{
	int	nSizeMode = nID - IDC_IMAGE_EXPORT_SIZE_MODE_1;
	ASSERT(nSizeMode >= 0 && nSizeMode < SIZE_MODES);
	UpdateUI(nSizeMode);
}

void CGraphBar::CImageExportDlg::OnDestroy()
{
	CDialog::OnDestroy();
	if (m_nModalResult == IDOK) {	// if user clicked OK, save state
		theApp.WriteProfileInt(RK_GraphBar, RK_EXPORT_SIZE_MODE, m_nSizeMode);
		theApp.WriteProfileInt(RK_GraphBar, RK_EXPORT_WIDTH, m_szImg.cx);
		theApp.WriteProfileInt(RK_GraphBar, RK_EXPORT_HEIGHT, m_szImg.cy);
	}
}

BEGIN_MESSAGE_MAP(CGraphBar::CImageExportDlg, CDialog)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_IMAGE_EXPORT_SIZE_MODE_1, IDC_IMAGE_EXPORT_SIZE_MODE_2, OnClickedSizeMode)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGraphBar message map

BEGIN_MESSAGE_MAP(CGraphBar, CMyDockablePane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_CONTEXTMENU()
	ON_WM_MENUSELECT()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
	ON_MESSAGE(UWM_GRAPH_DONE, OnGraphDone)
	ON_MESSAGE(UWM_GRAPH_ERROR, OnGraphError)
	ON_MESSAGE(UWM_DEFERRED_UPDATE, OnDeferredUpdate)
	ON_COMMAND_RANGE(SMID_GRAPH_SCOPE_FIRST, SMID_GRAPH_SCOPE_LAST, OnGraphScope)
	ON_COMMAND_RANGE(SMID_GRAPH_DEPTH_FIRST, SMID_GRAPH_DEPTH_LAST, OnGraphDepth)
	ON_COMMAND_RANGE(SMID_GRAPH_LAYOUT_FIRST, SMID_GRAPH_LAYOUT_LAST, OnGraphLayout)
	ON_COMMAND_RANGE(SMID_GRAPH_FILTER_FIRST, SMID_GRAPH_FILTER_LAST, OnGraphFilter)
	ON_COMMAND_RANGE(SMID_GRAPH_CHANNEL_FIRST, SMID_GRAPH_CHANNEL_LAST, OnGraphChannel)
	ON_COMMAND(ID_GRAPH_SAVEAS, OnGraphSaveAs)
	ON_UPDATE_COMMAND_UI(ID_GRAPH_SAVEAS, OnUpdateGraphSaveAs)
	ON_COMMAND(ID_GRAPH_ZOOM_IN, OnGraphZoomIn)
	ON_COMMAND(ID_GRAPH_ZOOM_OUT, OnGraphZoomOut)
	ON_COMMAND(ID_GRAPH_ZOOM_RESET, OnGraphZoomReset)
	ON_COMMAND(ID_GRAPH_ZOOM_RESET, OnGraphZoomReset)
	ON_COMMAND(ID_GRAPH_RELOAD, OnGraphReload)
	ON_COMMAND(ID_GRAPH_HIGHLIGHT_SELECT, OnGraphHighlightSelect)
	ON_UPDATE_COMMAND_UI(ID_GRAPH_HIGHLIGHT_SELECT, OnUpdateGraphHighlightSelect)
	ON_COMMAND(ID_GRAPH_EDGE_LABELS, OnGraphEdgeLabels)
	ON_UPDATE_COMMAND_UI(ID_GRAPH_EDGE_LABELS, OnUpdateGraphEdgeLabels)
	ON_COMMAND(ID_GRAPH_LEGEND, OnGraphLegend)
	ON_UPDATE_COMMAND_UI(ID_GRAPH_LEGEND, OnUpdateGraphLegend)
	ON_COMMAND(ID_GRAPH_SHOW_MUTED, OnGraphShowMuted)
	ON_UPDATE_COMMAND_UI(ID_GRAPH_SHOW_MUTED, OnUpdateGraphShowMuted)
	ON_COMMAND(ID_GRAPH_SELECT_MODULATION_TYPES, OnGraphSelectModulationTypes)
	ON_COMMAND(ID_GRAPH_SELECT_CHANNELS, OnGraphSelectChannels)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGraphBar message handlers

int CGraphBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;
	// don't set window name, otherwise OLE throws member not found
	if (!m_wndBrowser.CreateControl(CLSID_WebBrowser, _T("E"),
		WS_VISIBLE | WS_CHILD, CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST))
		return -1;
	return 0;
}

void CGraphBar::OnDestroy()
{
	CMyDockablePane::OnDestroy();
}

void CGraphBar::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	if (m_wndBrowser.m_hWnd) {	// following is cribbed from CHtmlView
		m_wndBrowser.MoveWindow(0, 0, cx, cy);
		if (FastIsVisible())
			StartDeferredUpdate();
	}
}

void CGraphBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	DoContextMenu(pWnd, point);
}

void CGraphBar::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	if (!(nFlags & MF_SYSMENU)) {	// if not system menu item
		if (nItemID >= SMID_GRAPH_SCOPE_FIRST) {
			if (nItemID <= SMID_GRAPH_SCOPE_LAST) {
				nItemID = m_arrHintGraphScopeID[nItemID - SMID_GRAPH_SCOPE_FIRST];
			} else if (nItemID <= SMID_GRAPH_DEPTH_LAST) {
				if (nItemID == SMID_GRAPH_DEPTH_FIRST)
					nItemID = IDS_HINT_GRAPH_DEPTH_MAX;
				else
					nItemID = IDS_HINT_GRAPH_DEPTH;
			} else if (nItemID <= SMID_GRAPH_LAYOUT_LAST) {
				nItemID = IDS_HINT_GRAPH_LAYOUT;
			} else if (nItemID <= SMID_GRAPH_FILTER_LAST) {
				if (nItemID == SMID_GRAPH_FILTER_FIRST)
					nItemID = IDS_HINT_GRAPH_FILTER_MOD_TYPE_ALL;
				else if (nItemID < SMID_GRAPH_FILTER_LAST)
					nItemID = IDS_HINT_GRAPH_FILTER_MOD_TYPE;
				else
					nItemID = IDS_HINT_GRAPH_FILTER_MOD_TYPE_MULTI;
			} else if (nItemID <= SMID_GRAPH_CHANNEL_LAST) {
				if (nItemID == SMID_GRAPH_CHANNEL_FIRST)
					nItemID = IDS_SM_FILTER_CHANNEL_ALL;
				else if (nItemID < SMID_GRAPH_CHANNEL_LAST)
					nItemID = IDS_SM_FILTER_CHANNEL_SELECT;
				else
					nItemID = IDS_HINT_PIANO_CHANNEL_MULTI;
			}
		}
	}
	CMyDockablePane::OnMenuSelect(nItemID, nFlags, hSysMenu);
}

LRESULT CGraphBar::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	// the channel hint strings are borrowed from the MIDI event and piano bars,
	// hence they must be remapped here to display an appropriate help topic
	UINT	nTrackingID = theApp.GetMainFrame()->GetTrackingID();
	UINT	nNewID;
	switch (nTrackingID) {
	case IDS_SM_FILTER_CHANNEL_ALL:
	case IDS_SM_FILTER_CHANNEL_SELECT:
	case IDS_HINT_PIANO_CHANNEL_MULTI:
		nNewID = IDS_HINT_GRAPH_CHANNEL;
		break;
	default:
		nNewID = 0;
	}
	if (nNewID) {	// if tracking ID was remapped
		theApp.WinHelp(nNewID);
		return TRUE;
	}
	return CMyDockablePane::OnCommandHelp(wParam, lParam);
}

LRESULT CGraphBar::OnGraphDone(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	switch (m_iGraphState) {
	case GTS_BUSY:
		m_iGraphState = GTS_IDLE;
		Navigate(m_tfpGraph.GetPath());
		break;
	case GTS_LATE:
		m_iGraphState = GTS_IDLE;	// discard stale graph
		UpdateGraph();	// start over again
		break;
	}
	return 0;
}

LRESULT CGraphBar::OnGraphError(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	m_iGraphState = GTS_IDLE;
	m_tfpData.Empty();
	m_tfpGraph.Empty();
	CString	sErrorMsg = FormatSystemError(static_cast<DWORD>(wParam));
	if (!sErrorMsg.IsEmpty())
		AfxMessageBox(LDS(IDS_GRAPH_ERROR) + '\n' + sErrorMsg);
	return 0;
}

LRESULT CGraphBar::OnDeferredUpdate(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	m_bUpdatePending = false;
	UpdateGraph();
	return 0;
}

void CGraphBar::OnGraphScope(UINT nID)
{
	int	iGraphScope = nID - SMID_GRAPH_SCOPE_FIRST;
	ASSERT(iGraphScope >= 0 && iGraphScope < GRAPH_SCOPES);
	if (iGraphScope != m_iGraphScope) {	// if state changed
		m_iGraphScope = iGraphScope;
		UpdateGraph();
	}
}

void CGraphBar::OnGraphDepth(UINT nID)
{
	int	nGraphDepth = nID - SMID_GRAPH_DEPTH_FIRST;
	ASSERT(nGraphDepth >= 0 && nGraphDepth < GRAPH_DEPTHS);
	if (nGraphDepth != m_nGraphDepth) {	// if state changed
		m_nGraphDepth = nGraphDepth;
		UpdateGraph();
	}
}

void CGraphBar::OnGraphLayout(UINT nID)
{
	int	iGraphLayout = nID - SMID_GRAPH_LAYOUT_FIRST;
	ASSERT(iGraphLayout >= 0 && iGraphLayout < GRAPH_LAYOUTS);
	if (iGraphLayout != m_iGraphLayout) {	// if state changed
		m_iGraphLayout = iGraphLayout;
		UpdateGraph();
	}
}

void CGraphBar::OnGraphFilter(UINT nID)
{
	int	iGraphFilter = nID - SMID_GRAPH_FILTER_FIRST;
	ASSERT(iGraphFilter >= 0 && iGraphFilter < GRAPH_FILTERS);
	iGraphFilter--;	// account for wildcard
	MOD_TYPE_MASK	nFilterMask = m_nGraphFilterMask;
	if (iGraphFilter == GRAPH_FILTER_MULTI) {	// if user selected multiple
		CSaveRestoreFocus	focus;	// restore focus after modal dialog
		CModulationTypeDlg	dlg(this);	// show modulation type dialog
		dlg.m_nModTypeMask = nFilterMask;	// init dialog's bitmask
		if (dlg.DoModal() != IDOK)	// if user canceled or error
			return;	// bail out
		nFilterMask = dlg.m_nModTypeMask;	// update filter bitmask
		if (nFilterMask) {	// if any modulation types selected
			int	nSetBits = theApp.CountSetBits(nFilterMask);	// count number of set bits in bitmask
			if (nSetBits == 1) {	// if single bit set
				ULONG	iFirstSetBit;
				_BitScanForward(&iFirstSetBit, nFilterMask);	// scan for first set bit
				iGraphFilter = iFirstSetBit;	// first set bit's index is selected type
			}
		} else {	// no modulation types selected
			iGraphFilter = -1;	// select all types
		}
	} else {	// user selected single or all
		if (iGraphFilter >= 0)	// if single
			nFilterMask = MAKE_MOD_TYPE_MASK(iGraphFilter);	// make mask
		else	// all
			nFilterMask = 0;	// reset mask
	}
	if (iGraphFilter != m_iGraphFilter || nFilterMask != m_nGraphFilterMask) {	// if state changed
		m_iGraphFilter = iGraphFilter;
		m_nGraphFilterMask = nFilterMask;
		UpdateGraph();
	}
}

void CGraphBar::OnGraphChannel(UINT nID)
{
	int	iGraphChannel = nID - SMID_GRAPH_CHANNEL_FIRST;
	ASSERT(iGraphChannel >= 0 && iGraphChannel < GRAPH_CHANNELS);
	iGraphChannel--;	// account for wildcard
	WORD	nChannelMask = m_nGraphChannelMask;
	if (iGraphChannel == GRAPH_CHANNEL_MULTI) {	// if user selected multiple
		if (!CPianoBar::PromptForChannelSelection(this, m_iGraphChannel, nChannelMask))
			return;
		if (nChannelMask) {	// if any channels specified
			int	nSetBits = theApp.CountSetBits(nChannelMask);	// count number of set bits in bitmask
			if (nSetBits == 1) {	// if single bit set
				ULONG	iFirstSetBit;
				_BitScanForward(&iFirstSetBit, nChannelMask);	// scan for first set bit
				iGraphChannel = iFirstSetBit;	// first set bit's index is selected channel
			}
		} else {	// no channels specified
			iGraphChannel = -1;	// select all channels
		}
	} else {	// user selected single or all
		if (iGraphChannel >= 0)	// if single
			nChannelMask = MAKE_CHANNEL_MASK(iGraphChannel);	// make channel bitmask
		else	// all
			nChannelMask = 0;	// reset channel bitmask
	}
	if (iGraphChannel != m_iGraphChannel || nChannelMask != m_nGraphChannelMask) {	// if state changed
		m_iGraphChannel = iGraphChannel;
		m_nGraphChannelMask = nChannelMask;
		UpdateGraph();
	}
}

void CGraphBar::OnGraphSaveAs()
{
	CString	sFilter(LPCTSTR(IDS_GRAPH_SAVE_AS_FILE_FILTER));
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	CPathStr	sDefName(pDoc->GetTitle());
	sDefName.RemoveExtension();
	CFileDialog	fd(FALSE, _T(".svg"), sDefName, OFN_OVERWRITEPROMPT, sFilter);
	if (fd.DoModal() == IDOK) {
		// assume supported file extensions match Graphviz output formats
		CString	sOutFormat(fd.GetFileExt());
		sOutFormat.MakeLower();	// Graphviz output formats are lower case
		CImageExportDlg	dlg;
		if (dlg.DoModal() == IDOK) {
			ExportGraph(sOutFormat, fd.GetPathName(), dlg.m_szImg);
		}
	}
}

void CGraphBar::OnUpdateGraphSaveAs(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(!m_tfpGraph.IsEmpty());
}

void CGraphBar::OnGraphZoomIn()
{
	SetZoomLevel(m_iZoomLevel + 1);
}

void CGraphBar::OnGraphZoomOut()
{
	SetZoomLevel(m_iZoomLevel - 1);
}

void CGraphBar::OnGraphZoomReset()
{
	SetZoomLevel(0);
}

void CGraphBar::OnGraphReload()
{
	UpdateGraph();
}

void CGraphBar::OnGraphHighlightSelect()
{
	m_bHighlightSelect ^= 1;	// toggle state
	UpdateGraph();
}

void CGraphBar::OnUpdateGraphHighlightSelect(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bHighlightSelect);
}

void CGraphBar::OnGraphEdgeLabels()
{
	m_bEdgeLabels ^= 1;	// toggle state
	UpdateGraph();
}

void CGraphBar::OnUpdateGraphEdgeLabels(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bEdgeLabels);
}

void CGraphBar::OnGraphLegend()
{
	m_bShowLegend ^= 1;	// toggle state
	UpdateGraph();
}

void CGraphBar::OnUpdateGraphLegend(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowLegend);
}

void CGraphBar::OnGraphShowMuted()
{
	m_bShowMuted ^= 1;	// toggle state
	RebuildMuteCacheIfNeeded();
	UpdateGraph();
}

void CGraphBar::OnUpdateGraphShowMuted(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowMuted);
}

void CGraphBar::OnGraphSelectModulationTypes()
{
	OnGraphFilter(SMID_GRAPH_FILTER_FIRST + 1 + GRAPH_FILTER_MULTI);	// show dialog with check list box
}

void CGraphBar::OnGraphSelectChannels()
{
	OnGraphChannel(SMID_GRAPH_CHANNEL_FIRST + 1 + GRAPH_CHANNEL_MULTI);	// show dialog with check list box
}
