#pragma once
#ifndef __DEM_L1_INPUT_CONDITION_RELEASED_H__
#define __DEM_L1_INPUT_CONDITION_RELEASED_H__

#include <Input/InputCondition.h>

// State condition that is Off on ButtonDown and On on ButtonUp.
// Allows to check if some button is NOT pressed.

namespace Input
{

class CInputConditionReleased: public CInputConditionState
{
	__DeclareClass(CInputConditionReleased);

protected:

	EDeviceType	DeviceType;
	U8			Button;

public:

	virtual bool Initialize(const Data::CParams& Desc);
	virtual void Reset() { On = true; }
	virtual void OnButtonDown(const IInputDevice* pDevice, const Event::ButtonDown& Event);
	virtual void OnButtonUp(const IInputDevice* pDevice, const Event::ButtonUp& Event);
};

}

#endif
