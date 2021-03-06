#include "InputTranslator.h"

#include <Input/InputEvents.h>
#include <Input/InputDevice.h>
#include <Input/ControlLayout.h>
#include <Events/Subscription.h>
#include <Data/DataArray.h>

namespace Input
{

CInputTranslator::CInputTranslator(CStrID UserID): _UserID(UserID)
{
	Contexts.SetKeepOrder(true);
}
//---------------------------------------------------------------------

CInputTranslator::~CInputTranslator()
{
	Clear();
}
//---------------------------------------------------------------------

void CInputTranslator::Clear()
{
	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
		if (Contexts[i].pLayout) n_delete(Contexts[i].pLayout);
	Contexts.Clear();
	DeviceSubs.Clear();
	EventQueue.Clear();
}
//---------------------------------------------------------------------

bool CInputTranslator::LoadSettings(const Data::CParams& Desc)
{
	Clear();

	Data::PParams ContextsDesc;
	if (Desc.Get(ContextsDesc, CStrID("Contexts")))
	{
		for (UPTR i = 0; i < ContextsDesc->GetCount(); ++i)
		{
			const Data::CParam& Prm = ContextsDesc->Get(i);
			const Data::CDataArray& ContextLayoutDesc = *Prm.GetValue<Data::PDataArray>().Get();

			CStrID ContextID = Prm.GetName();
			if (!CreateContext(ContextID)) FAIL;
			CControlLayout* pLayout = GetContextLayout(ContextID);
			if (!pLayout || !pLayout->Initialize(ContextLayoutDesc)) FAIL;
		}
	}

	// read enabled contexts and enable them
	// read axis inversion
	// read other settings

	OK;
}
//---------------------------------------------------------------------

bool CInputTranslator::SaveSettings(Data::CParams& OutDesc) const
{
	FAIL;
}
//---------------------------------------------------------------------

bool CInputTranslator::CreateContext(CStrID ID)
{
	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
		if (Contexts[i].ID == ID) FAIL;
	CInputContext& NewCtx = *Contexts.Add();
	NewCtx.ID = ID;
	NewCtx.Enabled = false;
	NewCtx.pLayout = n_new(CControlLayout);
	OK;
}
//---------------------------------------------------------------------

void CInputTranslator::DestroyContext(CStrID ID)
{
	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
		if (Contexts[i].ID == ID)
		{
			n_delete(Contexts[i].pLayout);
			Contexts.RemoveAt(i);
			break;
		}
}
//---------------------------------------------------------------------

CControlLayout* CInputTranslator::GetContextLayout(CStrID ID)
{
	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
		if (Contexts[i].ID == ID) return Contexts[i].pLayout;
	return NULL;
}
//---------------------------------------------------------------------

void CInputTranslator::EnableContext(CStrID ID)
{
	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
		if (Contexts[i].ID == ID)
		{
			Contexts[i].Enabled = true;
			break;
		}
}
//---------------------------------------------------------------------

void CInputTranslator::DisableContext(CStrID ID)
{
	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
		if (Contexts[i].ID == ID)
		{
			Contexts[i].Enabled = false;
			Contexts[i].pLayout->Reset();
			break;
		}
}
//---------------------------------------------------------------------

void CInputTranslator::ConnectToDevice(IInputDevice* pDevice, U16 Priority)
{
	if (pDevice->GetAxisCount() > 0)
	{
		Events::PSub NewSub;
		pDevice->Subscribe(&Event::AxisMove::RTTI, this, &CInputTranslator::OnAxisMove, &NewSub);
		DeviceSubs.Add(NewSub);
	}
	if (pDevice->GetButtonCount() > 0)
	{
		Events::PSub NewSub;
		pDevice->Subscribe(&Event::ButtonDown::RTTI, this, &CInputTranslator::OnButtonDown, &NewSub);
		DeviceSubs.Add(NewSub);
		pDevice->Subscribe(&Event::ButtonUp::RTTI, this, &CInputTranslator::OnButtonUp, &NewSub);
		DeviceSubs.Add(NewSub);
	}
}
//---------------------------------------------------------------------

void CInputTranslator::DisconnectFromDevice(const IInputDevice* pDevice)
{
	for (UPTR i = 0; i < DeviceSubs.GetCount(); )
		if (DeviceSubs[i]->GetDispatcher() == pDevice)
			DeviceSubs.RemoveAt(i);
		else ++i;
}
//---------------------------------------------------------------------

bool CInputTranslator::OnAxisMove(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	IInputDevice* pDevice = (IInputDevice*)pDispatcher;
	const Event::AxisMove& Ev = (const Event::AxisMove&)Event;

	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
	{
		CInputContext& Ctx = Contexts[i];
		if (!Ctx.Enabled) continue;

		CControlLayout* pLayout = Ctx.pLayout;

		for (UPTR StateIdx = 0; StateIdx < pLayout->States.GetCount(); ++StateIdx)
			pLayout->States.ValueAt(StateIdx)->OnAxisMove(pDevice, Ev);

		for (UPTR EventIdx = 0; EventIdx < pLayout->Events.GetCount(); ++EventIdx)
		{
			CControlLayout::CEventRecord& EvRec = pLayout->Events[EventIdx];
			if (EvRec.pEvent->OnAxisMove(pDevice, Ev))
			{
				Events::CEvent& NewEvent = *EventQueue.Add();
				NewEvent.ID = EvRec.OutEventID;
				NewEvent.Params = n_new(Data::CParams(1));
				NewEvent.Params->Set<float>(CStrID("Amount"), Ev.Amount);
				OK;
			}
		}
	}

	FAIL;
}
//---------------------------------------------------------------------

bool CInputTranslator::OnButtonDown(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	IInputDevice* pDevice = (IInputDevice*)pDispatcher;
	const Event::ButtonDown& Ev = (const Event::ButtonDown&)Event;

	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
	{
		CInputContext& Ctx = Contexts[i];
		if (!Ctx.Enabled) continue;

		CControlLayout* pLayout = Ctx.pLayout;

		for (UPTR StateIdx = 0; StateIdx < pLayout->States.GetCount(); ++StateIdx)
			pLayout->States.ValueAt(StateIdx)->OnButtonDown(pDevice, Ev);

		for (UPTR EventIdx = 0; EventIdx < pLayout->Events.GetCount(); ++EventIdx)
		{
			CControlLayout::CEventRecord& EvRec = pLayout->Events[EventIdx];
			if (EvRec.pEvent->OnButtonDown(pDevice, Ev))
			{
				Events::CEvent& NewEvent = *EventQueue.Add();
				NewEvent.ID = EvRec.OutEventID;
				OK;
			}
		}
	}

	FAIL;
}
//---------------------------------------------------------------------

bool CInputTranslator::OnButtonUp(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	IInputDevice* pDevice = (IInputDevice*)pDispatcher;
	const Event::ButtonUp& Ev = (const Event::ButtonUp&)Event;

	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
	{
		CInputContext& Ctx = Contexts[i];
		if (!Ctx.Enabled) continue;

		CControlLayout* pLayout = Ctx.pLayout;

		for (UPTR StateIdx = 0; StateIdx < pLayout->States.GetCount(); ++StateIdx)
			pLayout->States.ValueAt(StateIdx)->OnButtonUp(pDevice, Ev);

		for (UPTR EventIdx = 0; EventIdx < pLayout->Events.GetCount(); ++EventIdx)
		{
			CControlLayout::CEventRecord& EvRec = pLayout->Events[EventIdx];
			if (EvRec.pEvent->OnButtonUp(pDevice, Ev))
			{
				Events::CEvent& NewEvent = *EventQueue.Add();
				NewEvent.ID = EvRec.OutEventID;
				OK;
			}
		}
	}

	FAIL;
}
//---------------------------------------------------------------------

void CInputTranslator::UpdateTime(float ElapsedTime)
{
	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
	{
		CInputContext& Ctx = Contexts[i];
		if (!Ctx.Enabled) continue;

		CControlLayout* pLayout = Ctx.pLayout;

		for (UPTR StateIdx = 0; StateIdx < pLayout->States.GetCount(); ++StateIdx)
			pLayout->States.ValueAt(StateIdx)->OnTimeElapsed(ElapsedTime);

		for (UPTR EventIdx = 0; EventIdx < pLayout->Events.GetCount(); ++EventIdx)
		{
			CControlLayout::CEventRecord& EvRec = pLayout->Events[EventIdx];
			if (EvRec.pEvent->OnTimeElapsed(ElapsedTime))
			{
				Events::CEvent& NewEvent = *EventQueue.Add();
				NewEvent.ID = EvRec.OutEventID;
			}
		}
	}
}
//---------------------------------------------------------------------

void CInputTranslator::FireQueuedEvents(/*max count*/)
{
	for (UPTR i = 0; i < EventQueue.GetCount(); ++i)
		FireEvent(EventQueue[i]);
	EventQueue.Clear();
}
//---------------------------------------------------------------------

bool CInputTranslator::CheckState(CStrID StateID) const
{
	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
	{
		CInputContext& Ctx = Contexts[i];
		if (!Ctx.Enabled) continue;

		const CDict<CStrID, CInputConditionState*>& States = Ctx.pLayout->States;

		for (UPTR StateIdx = 0; StateIdx < States.GetCount(); ++StateIdx)
			if (States.KeyAt(StateIdx) == StateID && States.ValueAt(StateIdx)->IsOn()) OK;
	}

	FAIL;
}
//---------------------------------------------------------------------

void CInputTranslator::Reset(/*device type*/)
{
	for (UPTR i = 0; i < Contexts.GetCount(); ++i)
		if (Contexts[i].Enabled)
			Contexts[i].pLayout->Reset();
}
//---------------------------------------------------------------------

}