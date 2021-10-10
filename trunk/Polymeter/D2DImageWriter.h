// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		02apr20	initial version
		
*/

#pragma once

class CD2DImageWriter : public WObject {
public:
	CD2DImageWriter();
	virtual	~CD2DImageWriter();
	bool	Create(CSize szBitmap);
	bool	Write(LPCTSTR pszPath);
	CRenderTarget	m_rt;

protected:
	CSize	m_szImage;
	CComPtr<ID2D1Factory> m_pD2DFactory;
	CComPtr<IWICImagingFactory>	m_pWICFactory;
	CComPtr<IWICBitmap>	m_pWICBitmap;
	CComPtr<ID2D1RenderTarget>	m_pRenderTarget;
	HRESULT	m_hr;
	virtual	void OnError();
};
