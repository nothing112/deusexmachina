#pragma once
#ifndef __DEM_L1_INPUT_DEVICE_H__
#define __DEM_L1_INPUT_DEVICE_H__

#include <StdDEM.h>

// An interface for various input devices such as keyboards, mouses and gamepads

namespace Input
{

class IInputDevice //: public Core::CRefCounted (or event dispatcher?)
{
private:

public:

	static const U8 InvalidCode = 0xff;

	virtual U8			GetAxisCount() const = 0;
	virtual U8			GetAxisCode(const char* pAlias) const = 0;
	virtual const char*	GetAxisAlias(U8 Code) const = 0;
	virtual U8			GetButtonCount() const = 0;
	virtual U8			GetButtonCode(const char* pAlias) const = 0;
	virtual const char*	GetButtonAlias(U8 Code) const = 0;
};

//typedef Ptr<CControlLayout> PControlLayout;

}

#endif
