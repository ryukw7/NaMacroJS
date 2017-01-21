#include "NaImage.h"

#include "ScreenModule.h"

#include <gdiplus.h>

Global<ObjectTemplate> NaImage::s_NaImageTemplate;

NaImage::NaImage()
{
	m_hMemoryDC = NULL;
	NaDebugOut(L"NaImage(): 0x%08x\n", this);
}

NaImage::~NaImage()
{
	NaDebugOut(L"~NaImage(): 0x%08x\n", this);

	if (m_hMemoryDC)
	{
		::DeleteDC(m_hMemoryDC);
	}

	if (m_hBitmap)
	{
		::DeleteObject(m_hBitmap);
	}
}

Local<ObjectTemplate> NaImage::MakeObjectTemplate(Isolate * isolate)
{
	EscapableHandleScope handle_scope(isolate);

	Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
	templ->SetInternalFieldCount(1);

	// bind image methods
#define ADD_IMAGE_ACCESSOR(_prop, _getter, _setter)	ADD_OBJ_ACCESSOR(templ, _prop, _getter, _setter);
#define ADD_IMAGE_METHOD(_js_func, _c_func)			ADD_TEMPLATE_METHOD(templ, _js_func, _c_func);

	// accessor
	ADD_IMAGE_ACCESSOR(width, GetWidth, SetWidth);
	ADD_IMAGE_ACCESSOR(height, GetHeight, SetHeight);

	// methods
	ADD_IMAGE_METHOD(getPixel, GetPixel);
	ADD_IMAGE_METHOD(findImage, FindImage);

	return handle_scope.Escape(templ);
}

POINT NaImage::FindColor(DWORD dwColor)
{
	POINT pt;
	pt.x = -1;
	pt.y = -1;

	if (m_hMemoryDC && m_hBitmap)
	{
		HGDIOBJ hOldBitmap = ::SelectObject(m_hMemoryDC, m_hBitmap);
		COLORREF color;
		int nWidth = m_rc.right - m_rc.left;
		int nHeight = m_rc.bottom - m_rc.top;
		for (int x = 0; x < nWidth; x++)
		{
			for (int y = 0; y < nHeight; y++)
			{
				color = ::GetPixel(m_hMemoryDC, x, y);
				if (color == dwColor)
				{
					pt.x = x;
					pt.y = y;
					::SelectObject(m_hMemoryDC, hOldBitmap);
					return pt;
				}
			}
		}

		::SelectObject(m_hMemoryDC, hOldBitmap);
	}

	return pt;
}

// description: capture screen & create Image object
NaImage* NaImage::CaptureScreen(int x, int y, int width, int height)
{
	NaImage *pImage = new NaImage();

	HDC hDC = NaScreenModule::GetDesktopDC();
	pImage->m_hMemoryDC = ::CreateCompatibleDC(hDC);
	pImage->m_hBitmap = ::CreateCompatibleBitmap(hDC, width, height);
	HGDIOBJ hOldBitmap = ::SelectObject(pImage->m_hMemoryDC, pImage->m_hBitmap);
	::BitBlt(pImage->m_hMemoryDC, 0, 0, width, height, hDC, x, y, SRCCOPY);
	::SelectObject(pImage->m_hMemoryDC, hOldBitmap);

	// TODO must delete
	//::DeleteDC(pImage->m_hMemoryDC);

	//::ReleaseDC(NaScreenModule::GetDesktopHWND(), hDC);

	pImage->m_rc.left = x;
	pImage->m_rc.top = y;
	pImage->m_rc.right = x + width;
	pImage->m_rc.bottom = y + height;

	return pImage;
}

// description: load image from file using gdi+
NaImage * NaImage::Load(const wchar_t * filename)
{
	NaImage *pImage = new NaImage;
	HDC hDC = NaScreenModule::GetDesktopDC();
	pImage->m_hMemoryDC = ::CreateCompatibleDC(hDC);

	// initialize gdi+
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// load image
	Gdiplus::Image* pGdiImage = new Gdiplus::Image(filename);

	// converting to bitmap
	Gdiplus::Bitmap* pGdiBitmap = static_cast<Gdiplus::Bitmap*>(pGdiImage);
	pGdiBitmap->GetHBITMAP(Gdiplus::Color(0, 0, 0), &pImage->m_hBitmap);

	pImage->m_rc.left = 0;
	pImage->m_rc.top = 0;
	pImage->m_rc.right = pGdiImage->GetWidth();
	pImage->m_rc.bottom = pGdiImage->GetHeight();

	// shutdown gdi+
	delete pGdiImage;
	Gdiplus::GdiplusShutdown(gdiplusToken);

	return pImage;
}

POINT NaImage::SearchImageInImage(NaImage * pTarget, NaImage * pSource)
{
	POINT pt = { -1, -1 };
	if (pTarget != nullptr && pSource != nullptr)
	{
		int nTargetWidth = pTarget->m_rc.right - pTarget->m_rc.left;
		int nTargetHeight = pTarget->m_rc.bottom - pTarget->m_rc.top;
		int nSourceWidth = pSource->m_rc.right - pSource->m_rc.left;
		int nSourceHeight = pSource->m_rc.bottom - pSource->m_rc.top;

		if (nTargetWidth >= nSourceWidth && nTargetHeight >= nSourceHeight)
		{
			HGDIOBJ hObjTarget = ::SelectObject(pTarget->m_hMemoryDC, pTarget->m_hBitmap);
			HGDIOBJ hObjSource = ::SelectObject(pSource->m_hMemoryDC, pSource->m_hBitmap);

			bool bMismatch;
			for (int y = 0; y < nTargetHeight - nSourceHeight; y++)
			{
				for (int x = 0; x < nTargetWidth - nSourceWidth; x++)
				{
					bMismatch = false;
					for (int y2 = 0; y2 < nSourceHeight; y2++)
					{
						for (int x2 = 0; x2 < nSourceWidth; x2++)
						{
							COLORREF nCol1 = ::GetPixel(pTarget->m_hMemoryDC, x + x2, y + y2);
							COLORREF nCol2 = ::GetPixel(pSource->m_hMemoryDC, x2, y2);
							if (nCol1 != nCol2)
							{
								bMismatch = true;
								break;
							}
						}
						if (bMismatch)
							break;
					}

					if (bMismatch == false)
					{
						pt.x = x;
						pt.y = y;
						::SelectObject(pTarget->m_hMemoryDC, hObjTarget);
						::SelectObject(pSource->m_hMemoryDC, hObjSource);

						return pt;
					}
				}
			}

			::SelectObject(pTarget->m_hMemoryDC, hObjTarget);
			::SelectObject(pSource->m_hMemoryDC, hObjSource);
		}
	}

	return pt;
}

// description: width property getter/setter
void NaImage::GetWidth(V8_GETTER_ARGS)
{
	NaImage *pImage = reinterpret_cast<NaImage*>(UnwrapObject(info.This()));
	Isolate *isolate = info.GetIsolate();
	int nWidth = 0;
	if (pImage)
	{
		nWidth = pImage->m_rc.right - pImage->m_rc.left;
	}

	info.GetReturnValue().Set(
		Integer::New(isolate, nWidth)
		);
}

void NaImage::SetWidth(V8_SETTER_ARGS)
{
	NaImage *pImage = reinterpret_cast<NaImage*>(UnwrapObject(info.This()));
	if (pImage)
	{
		// Not Impl
	}
}

// description: height property getter/setter
void NaImage::GetHeight(V8_GETTER_ARGS)
{
	NaImage *pImage = reinterpret_cast<NaImage*>(UnwrapObject(info.This()));
	Isolate *isolate = info.GetIsolate();
	int nHeight = 0;
	if (pImage)
	{
		nHeight = pImage->m_rc.bottom - pImage->m_rc.top;
	}

	info.GetReturnValue().Set(
		Integer::New(isolate, nHeight)
		);
}

void NaImage::SetHeight(V8_SETTER_ARGS)
{
	NaImage *pImage = reinterpret_cast<NaImage*>(UnwrapObject(info.This()));
	if (pImage)
	{
		// Not Impl
	}
}

// description: constructor function
// syntax:		new Window([x, y[, width, height]]) : windowObj
void NaImage::Constructor(V8_FUNCTION_ARGS)
{
	if (args.Length() >= 1)
	{
		String::Value filepath(args[0]);

		Local<StackTrace> stack_trace = StackTrace::CurrentStackTrace(
			args.GetIsolate(), 1, StackTrace::kScriptName);
		Local<StackFrame> cur_frame = stack_trace->GetFrame(0);
		NaString strBase(cur_frame->GetScriptName());
		NaString strFilePath(filepath);

		NaUrl url;
		url.SetBase(strBase);
		url.SetUrl(strFilePath);

		NaString strFullPath(url.GetFullUrl());

		NaImage *pImage = NaImage::Load(strFullPath.wstr());
		Local<Object> obj = WrapObject(args.GetIsolate(), pImage);

		args.GetReturnValue().Set(obj);
		return;
	}

	args.GetReturnValue().Set(Null(args.GetIsolate()));
}

// description: get pixel from image buffer
// syntax:		imageObj.getPixel(x, y);
void NaImage::GetPixel(V8_FUNCTION_ARGS)
{
	Isolate *isolate = args.GetIsolate();
	Local<Object> obj = args.This();
	NaImage *pImage = reinterpret_cast<NaImage*>(UnwrapObject(obj));
	if (pImage == nullptr)
	{
		// error
		args.GetReturnValue().Set(Integer::New(isolate, -1));
		return;
	}

	if (args.Length() < 2)
	{
		// error
		args.GetReturnValue().Set(Integer::New(isolate, -1));
		return;
	}

	int x = args[0]->Int32Value();
	int y = args[1]->Int32Value();

	HGDIOBJ hOldBitmap = ::SelectObject(pImage->m_hMemoryDC, pImage->m_hBitmap);
	COLORREF color = ::GetPixel(pImage->m_hMemoryDC, x, y);
	::SelectObject(pImage->m_hMemoryDC, hOldBitmap);

	// return
	args.GetReturnValue().Set(Integer::New(isolate, color));
}

// description: find image(argument) from image(this)
// syntax:		imageObj.findImage(image object)
void NaImage::FindImage(V8_FUNCTION_ARGS)
{
	Isolate *isolate = args.GetIsolate();
	Local<Object> objThis = args.This();
	NaImage *pImageThis = reinterpret_cast<NaImage*>(UnwrapObject(objThis));
	if (pImageThis == nullptr)
	{
		// error
		args.GetReturnValue().Set(Integer::New(isolate, -1));
		return;
	}

	if (args.Length() < 1)
	{
		// error
		args.GetReturnValue().Set(Integer::New(isolate, -1));
		return;
	}

	Local<Object> objFind = args[0]->ToObject();
	NaImage *pImageFind = reinterpret_cast<NaImage*>(UnwrapObject(objFind));
	if (pImageFind == nullptr)
	{
		// error
		args.GetReturnValue().Set(Integer::New(isolate, -1));
		return;
	}

	POINT pt = SearchImageInImage(pImageThis, pImageFind);

	Local<Object> objRet = Object::New(isolate);
	objRet->Set(
		String::NewFromUtf8(isolate, "x", NewStringType::kNormal).ToLocalChecked(),
		Integer::New(isolate, pt.x)
	);
	objRet->Set(
		String::NewFromUtf8(isolate, "y", NewStringType::kNormal).ToLocalChecked(),
		Integer::New(isolate, pt.y)
	);

	// return
	args.GetReturnValue().Set(objRet);
}
