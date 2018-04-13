
// stdafx.cpp : source file that includes just the standard includes
// Polymeter.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"
#include "resource.h"

void DDV_Fail(CDataExchange* pDX, int nIDC)
{
	ASSERT(pDX != NULL);
	ASSERT(pDX->m_pDlgWnd != NULL);
	// test for edit control via GetClassName instead of IsKindOf,
	// so test works on controls that aren't wrapped in MFC object
	HWND	hWnd = ::GetDlgItem(pDX->m_pDlgWnd->m_hWnd, nIDC);
	ASSERT(hWnd != NULL);
	TCHAR	szClassName[6];
	// if control is an edit control
	if (GetClassName(hWnd, szClassName, 6) && !_tcsicmp(szClassName, _T("Edit")))
		pDX->PrepareEditCtrl(nIDC);
	else	// not an edit control
		pDX->PrepareCtrl(nIDC);
	pDX->Fail();
}

CString FormatSystemError(DWORD ErrorCode)
{
	LPTSTR	lpszTemp;
	DWORD	bRet = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, ErrorCode, 0, (LPTSTR)&lpszTemp, 0, NULL);	// default language
	CString	sError;	
	if (bRet) {	// if format succeeded
		sError = lpszTemp;	// create string from message buffer
		LocalFree(lpszTemp);	// free message buffer
	} else	// format failed
		sError.Format(IDS_APP_UNKNOWN_SYSTEM_ERROR, ErrorCode, ErrorCode);
	return(sError);
}

CString	GetLastErrorString()
{
	return(FormatSystemError(GetLastError()));
}

void SetTimelyProgressPos(CProgressCtrl& Progress, int nPos)
{
	// workaround for Aero animated progress bar's absurd lag
	int nLower, nUpper;
	Progress.GetRange(nLower, nUpper);
	if (nPos < nUpper) {	// if not at limit
		// decrement causes immediate update, bypassing animation
		Progress.SetPos(nPos + 1);	// set one beyond desired value
		Progress.SetPos(nPos);	// set desired value
	} else {	// limit reached
		// more elaborate kludge to bypass animation for last position
		Progress.SetRange32(nLower, nUpper + 1);
		Progress.SetPos(nUpper + 1);
		Progress.SetPos(nUpper);
		Progress.SetRange32(nLower, nUpper);
	}
}

bool GetUserNameString(CString& sName)
{
	sName.Empty();
	DWORD	dwLen = UNLEN + 1;
	LPTSTR	lpszName = sName.GetBuffer(dwLen);
	bool	bRetVal = GetUserName(lpszName, &dwLen) != 0;
	sName.ReleaseBuffer();
	return bRetVal;
}

bool GetComputerNameString(CString& sName)
{
	sName.Empty();
	DWORD	dwLen = MAX_COMPUTERNAME_LENGTH + 1;
	LPTSTR	lpszName = sName.GetBuffer(dwLen);
	bool	bRetVal = GetComputerName(lpszName, &dwLen) != 0;
	sName.ReleaseBuffer();
	return bRetVal;
}

bool CopyStringToClipboard(HWND m_hWnd, const CString& strData)
{
	if (!OpenClipboard(m_hWnd))
		return false;
	EmptyClipboard();
	size_t	nLen = (strData.GetLength() + 1) * sizeof(TCHAR);
	HGLOBAL	hBuf = GlobalAlloc(GMEM_DDESHARE, nLen);
	if (hBuf == NULL)
		return false;
	TCHAR	*pBuf = (TCHAR *)GlobalLock(hBuf);
	USES_CONVERSION;
	memcpy(pBuf, strData, nLen);
	GlobalUnlock(hBuf);
#ifdef UNICODE
	UINT	nFormat = CF_UNICODETEXT;
#else
	UINT	nFormat = CF_TEXT;
#endif
	HANDLE	hData = SetClipboardData(nFormat, hBuf);
	CloseClipboard();
	return hData != NULL;
}

void EnableChildWindows(CWnd& Wnd, bool Enable, bool Deep)
{
	CWnd	*wp = Wnd.GetWindow(GW_CHILD);
	while (wp != NULL) {
		if (Deep)	// if recursively searching all children
			EnableChildWindows(*wp, Enable, Deep);
		wp->EnableWindow(Enable);
		wp = wp->GetNextWindow();
	}
}

void UpdateMenu(CWnd *pWnd, CMenu *pMenu)
{
	CCmdUI	cui;
	cui.m_pMenu = pMenu;
	cui.m_nIndexMax = pMenu->GetMenuItemCount();
	for (UINT i = 0; i < cui.m_nIndexMax; i++) {
		cui.m_nID = pMenu->GetMenuItemID(i);
		if (!cui.m_nID)	// separator
			continue;
		if (cui.m_nID == -1) {	// popup submenu
			CMenu	*pSubMenu = pMenu->GetSubMenu(i);
			if (pSubMenu != NULL)
				UpdateMenu(pWnd, pSubMenu);	// recursive call
		}
		cui.m_nIndex = i;
		cui.m_pMenu = pMenu;
		cui.DoUpdate(pWnd, FALSE);
	}
}

