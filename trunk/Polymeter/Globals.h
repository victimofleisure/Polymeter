// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		23mar18	initial version
		01		29jan19	add MIDI event message
		02		20mar20	add track property change message
		03		01apr20	add generic context menu method
		04		05apr20	add track step change message
		05		07sep20	add apply preset and part messages
		06		15feb21	add mapped command message
		07		08jun21	fix LDS macro warning
		08		30dec21	replace AfxGetApp with faster method

		global definitions and inlines

*/

#pragma once

#include "Wrapx64.h"	// ck: special types for supporting both 32-bit and 64-bit
#include "WObject.h"	// ck: ultra-minimal base class used by many of my objects
#include "ArrayEx.h"	// ck: wraps MFC dynamic arrays, adding speed and features
#include "Round.h"		// ck: round floating point to integer
#include "Benchmark.h"	// ck: wraps performance counter for benchmarking

// define registry section for settings
#define REG_SETTINGS _T("Settings")

// key status bits for GetKeyState and GetAsyncKeyState
#define GKS_TOGGLED			0x0001
#define GKS_DOWN			0x8000

// clamp a value to a range
#define CLAMP(x, lo, hi) (min(max((x), (lo)), (hi)))

// trap bogus default case in switch statement
#define NODEFAULTCASE	ASSERT(0)

// load string from resource via temporary object
#define LDS(x) CString(MAKEINTRESOURCE(x))

// ck: define containers for some useful built-in types
typedef CArrayEx<float, float> CFloatArray;
typedef CArrayEx<double, double> CDoubleArray;
typedef CArrayEx<char, char> CCharArray;

// wrapper for formatting system errors
CString FormatSystemError(DWORD ErrorCode);
CString	GetLastErrorString();

// workaround for Aero animated progress bar's absurd lag
void SetTimelyProgressPos(CProgressCtrl& Progress, int nPos);

bool GetUserNameString(CString& sName);
bool GetComputerNameString(CString& sName);
bool CopyStringToClipboard(HWND m_hWnd, const CString& strData);

void EnableChildWindows(CWnd& Wnd, bool Enable, bool Deep = TRUE);
void UpdateMenu(CWnd *pWnd, CMenu *pMenu);
void DoGenericContextMenu(UINT nIDResource, CPoint point, CWnd* pWnd);
bool FormatNumberCommas(LPCTSTR pszSrc, CString& sDst, int nPrecision = 0);
int StringReplaceNoCase(CString& str, LPCTSTR pszOld, LPCTSTR pszNew);

// data validation method to flunk a control
void DDV_Fail(CDataExchange* pDX, int nIDC);

// swap values of any type that allows assignment
template<typename T> inline void Swap(T& a, T& b)
{
	T tmp = a;
	a = b;
	b = tmp;
}

// wrap dynamic object creation
template<typename T> inline bool SafeCreateObject(CRuntimeClass *pRuntimeClass, T*& pObject)
{
	pObject = static_cast<T *>(pRuntimeClass->CreateObject());
	ASSERT(pObject->IsKindOf(pRuntimeClass));
	return pObject != NULL;	// can be null if class name not found or insufficient memory
}

// replace AfxGetApp with faster method
class CPolymeterApp;
extern CPolymeterApp theApp;
inline CWinApp *FastGetApp()
{
	return reinterpret_cast<CWinApp*>(&theApp);
}
#define AfxGetApp FastGetApp

// define benchmarking macros
#define BENCH_START CBenchmark b;
#define BENCH_STOP printf("%f\n", b.Elapsed());

// base ID for dynamic submenus, far above menu resource IDs and below MFC reserved IDs
#define ID_APP_DYNAMIC_SUBMENU_BASE 0xc800

enum {	// substitute resource IDs for remapping help topics
	IDS_HINT_PIANO_FILTER_CHANNEL = ID_APP_DYNAMIC_SUBMENU_BASE - 100,
};

enum {	// application-wide user window messages, based on WP_APP
	UWM_FIRST = WM_APP,
	UWM_HANDLE_DLG_KEY,			// wParam: MSG pointer, lParam: none
	UWM_MODELESS_DESTROY,		// wParam: CDialog*, lParam: none
	UWM_DEFERRED_UPDATE,		// wParam: none, lParam: none
	UWM_DEFERRED_SIZING,		// wParam: none, lParam: none
	UWM_DELAYED_CREATE,			// wParam: none, lParam: none
	UWM_PROPERTY_CHANGE,		// wParam: iProp, lParam: CWnd*
	UWM_PROPERTY_SELECT,		// wParam: iProp or -1 if none, lParam: CWnd*
	UWM_MIDI_ERROR,				// wParam: error code, lParam: CDocument*
	UWM_DEVICE_NODE_CHANGE,		// wParam: none, lParam: none
	UWM_SHOW_CHANGING,			// wParam: none, lParam: none
	UWM_MIDI_EVENT,				// wParam: timestamp, lParam: MIDI message
	UWM_TRACK_PROPERTY_CHANGE,	// wParam: track index, lParam: property index
	UWM_TRACK_STEP_CHANGE,		// wParam: track index, lParam: step index
	UWM_PRESET_APPLY,			// wParam: preset index, lParam: none
	UWM_PART_APPLY,				// wParam: part index, lParam: bool mute state
	UWM_MAPPED_COMMAND,			// wParam: command ID, lParam: varies
};

// undo natter should always be zero in a shipping version
#define UNDO_NATTER 0	// set non-zero to enable undo natter

// undo tests should always be zero in a shipping version
#define TRACK_UNDO_TEST 0	// set non-zero to enable track undo test

// if testing undo, UNDO_TEST must also be non-zero, else linker errors result
#define UNDO_TEST TRACK_UNDO_TEST
