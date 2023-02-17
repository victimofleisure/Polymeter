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

#include "stdafx.h"
#include "D2DImageWriter.h"
#include "wincodec.h"

#pragma comment(lib,"d2d1.lib")	// avoids linker error

#define CHECK(x) if (FAILED(m_hr = x)) { OnError(); return false; }

CD2DImageWriter::CD2DImageWriter()
{
	m_szImage = CSize(0, 0);
}

CD2DImageWriter::~CD2DImageWriter()
{
	if (m_rt.IsValid())
		m_rt.Detach();
}

void CD2DImageWriter::OnError()
{
	CString	sMsg(FormatSystemError(m_hr));
	AfxMessageBox(sMsg);
}

bool CD2DImageWriter::Create(CSize szImage)
{
	m_szImage = szImage;
	CHECK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory));
	CHECK(m_pWICFactory.CoCreateInstance(CLSID_WICImagingFactory));
	CHECK(m_pWICFactory->CreateBitmap(szImage.cx, szImage.cy, 
		GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &m_pWICBitmap));
	D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    rtProps.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    rtProps.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    rtProps.usage = D2D1_RENDER_TARGET_USAGE_NONE;
	CHECK(m_pD2DFactory->CreateWicBitmapRenderTarget(m_pWICBitmap, &rtProps, &m_pRenderTarget));
	m_rt.Attach(m_pRenderTarget);
	return m_rt.IsValid() != 0;
}

bool CD2DImageWriter::Write(LPCTSTR pszPath)
{
	CComPtr<IWICStream>	pWICStream;
	CComPtr<IWICBitmapEncoder> pEncoder;
	CComPtr<IWICBitmapFrameEncode> pFrameEncode;
	CHECK(m_pWICFactory->CreateStream(&pWICStream));
	CHECK(pWICStream->InitializeFromFilename(pszPath, GENERIC_WRITE));	// requires UNICODE
    CHECK(m_pWICFactory->CreateEncoder(GUID_ContainerFormatPng, NULL, &pEncoder));
    CHECK(pEncoder->Initialize(pWICStream, WICBitmapEncoderNoCache));
    CHECK(pEncoder->CreateNewFrame(&pFrameEncode, NULL));
    CHECK(pFrameEncode->Initialize(NULL));
    CHECK(pFrameEncode->WriteSource(m_pWICBitmap, NULL));
    CHECK(pFrameEncode->Commit());
    CHECK(pEncoder->Commit());
	return true;
}
