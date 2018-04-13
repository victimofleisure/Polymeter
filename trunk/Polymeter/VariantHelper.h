// Copyleft 2017 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar17	initial version
		
*/

#pragma once

inline void GetVariant(const CComVariant& var, int& val)
{
	val = var.intVal;
}

inline void GetVariant(const CComVariant& var, UINT& val)
{
	val = var.uintVal;
}

inline void GetVariant(const CComVariant& var, DWORD& val)
{
	val = var.uintVal;
}

inline void GetVariant(const CComVariant& var, float& val)
{
	val = var.fltVal;
}

inline void GetVariant(const CComVariant& var, double& val)
{
	val = var.dblVal;
}

inline void GetVariant(const CComVariant& var, bool& val)
{
	val = var.boolVal != 0;
}

inline void GetVariant(const CComVariant& var, CString& val)
{
	val = var.bstrVal;
}
