#include "InputConditionDown.h"

#include <Input/InputEvents.h>
#include <Input/InputDevice.h>

namespace Input
{

bool CInputConditionDown::Initialize(const Data::CParams& Desc)
{
	DeviceType = StringToDeviceType(Desc.Get<CString>(CStrID("Device"), CString::Empty));
	Button = StringToMouseButton(Desc.Get<CString>(CStrID("Button"), CString::Empty));
	OK;
}
//---------------------------------------------------------------------

bool CInputConditionDown::OnButtonDown(const IInputDevice* pDevice, const Event::ButtonDown& Event)
{
	return pDevice->GetType() == DeviceType && Event.Code == Button;
}
//---------------------------------------------------------------------

}