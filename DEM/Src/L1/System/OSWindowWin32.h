#pragma once
#ifdef __WIN32__
#ifndef __DEM_L1_SYS_OS_WINDOW_WIN32_H__
#define __DEM_L1_SYS_OS_WINDOW_WIN32_H__

#include <Events/EventDispatcher.h>
#include <Data/Regions.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Win32 operating system window implementation

namespace Sys
{
typedef Ptr<class COSWindowClassWin32> POSWindowClassWin32;

class COSWindowWin32: public Events::CEventDispatcher, public Data::CRefCounted
{
protected:

	enum
	{
		Wnd_Open		= 0x01,
		Wnd_Minimized	= 0x02,
		Wnd_Topmost		= 0x04,
		Wnd_Fullscreen	= 0x08
	};

	CString				WindowTitle;
	CString				IconName;

	Data::CFlags		Flags;
	COSWindowWin32*		pParent;
	Data::CRect			Rect;		// Client rect

	HWND				hWnd;
	HACCEL				hAccel;
	POSWindowClassWin32	WndClass;

public:

	COSWindowWin32(): pParent(NULL), hWnd(NULL), hAccel(NULL) {}
	~COSWindowWin32();

	bool					Open();
	void					Close();
	void					Minimize();
	void					Restore();
	bool					HandleWindowMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, LONG& Result); // Mainly for internal use

	bool					SetRect(const Data::CRect& NewRect, bool FullscreenMode = false);
	const Data::CRect&		GetRect() const { return Rect; }
	unsigned int			GetWidth() const { return Rect.W; }
	unsigned int			GetHeight() const { return Rect.H; }

	bool					GetAbsoluteXY(float XRel, float YRel, int& XAbs, int& YAbs) const;
	bool					GetRelativeXY(int XAbs, int YAbs, float& XRel, float& YRel) const;

	void					SetWindowClass(COSWindowClassWin32& WindowClass);
	COSWindowClassWin32*	GetWindowClass() const { return WndClass.GetUnsafe(); }
	void					SetTitle(const char* pTitle);
	const char*				GetTitle() const { return WindowTitle; }
	void					SetIcon(const char* pIconName);
	const char*				GetIcon() const { return IconName; }

	bool					IsOpen() const { return Flags.Is(Wnd_Open); }
	bool					(IsMinimized)() const { return Flags.Is(Wnd_Minimized); }
	bool					IsTopmost() const { return Flags.Is(Wnd_Topmost); }
	bool					IsFullscreen() const { return Flags.Is(Wnd_Fullscreen); }
	bool					IsChild() const { return !!pParent; }
	bool					SetTopmost(bool Topmost);
	bool					SetInputFocus();

	COSWindowWin32*			GetParent() const { return pParent; }
	HWND					GetHWND() const { return hWnd; }
	HACCEL					GetWin32AcceleratorTable() const { return hAccel; }
	LONG					GetWin32Style() const;
};

inline bool COSWindowWin32::GetAbsoluteXY(float XRel, float YRel, int& XAbs, int& YAbs) const
{
	RECT r;
	if (!hWnd || !::GetClientRect(hWnd, &r)) FAIL;
	XAbs = (int)(XRel * n_max(r.right - r.left, 1));
	YAbs = (int)(YRel * n_max(r.bottom - r.top, 1));
	OK;
}
//---------------------------------------------------------------------

inline bool COSWindowWin32::GetRelativeXY(int XAbs, int YAbs, float& XRel, float& YRel) const
{
	RECT r;
	if (!hWnd || !::GetClientRect(hWnd, &r)) FAIL;
	XRel = XAbs / float(n_max(r.right - r.left, 1));
	YRel = YAbs / float(n_max(r.bottom - r.top, 1));
	OK;
}
//---------------------------------------------------------------------

}

#endif
#endif //__WIN32__
