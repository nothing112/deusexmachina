#include "PropUIControl.h"

#include <Game/Entity.h>
#include <Game/GameLevel.h>
#include <AI/PropActorBrain.h>
#include <AI/PropSmartObject.h>
#include <AI/Behaviour/ActionSequence.h>
#include <AI/SmartObj/Actions/ActionGotoSmartObj.h>
#include <AI/SmartObj/Actions/ActionUseSmartObj.h>
#include <Physics/PhysicsServer.h>
#include <Scene/PropSceneNode.h>
#include <Scripting/PropScriptable.h>
#include <Scripting/EventHandlerScript.h>
#include <Resources/ResourceManager.h>
#include <Resources/ResourceLoader.h>
#include <Resources/Resource.h>
#include <Data/ParamsUtils.h>
#include <Events/EventServer.h>
#include <Core/Factory.h>

namespace Prop
{
__ImplementClass(Prop::CPropUIControl, 'PUIC', Game::CProperty);
__ImplementPropertyStorage(CPropUIControl);

CPropUIControl::CAction::CAction() {}
CPropUIControl::CAction::CAction(CStrID _ID, const char* Name, int _Priority): ID(_ID), UIName(Name), Priority(_Priority) {}
CPropUIControl::CPropUIControl() {}

bool CPropUIControl::InternalActivate()
{
	Enable(GetEntity()->GetAttr<bool>(CStrID("UIEnabled"), true));

	UIName = GetEntity()->GetAttr<CString>(CStrID("Name"), CString::Empty);
	UIDesc = GetEntity()->GetAttr<CString>(CStrID("Desc"), CString::Empty);
	ReflectSOActions = false;

	const CString& UIDescPath = GetEntity()->GetAttr<CString>(CStrID("UIDesc"), CString::Empty);
	Data::PParams Desc;
	if (UIDescPath.IsValid()) ParamsUtils::LoadParamsFromPRM(CString("GameUI:") + UIDescPath + ".prm", Desc);
	if (Desc.IsValidPtr())
	{
		if (UIName.IsEmpty()) UIName = Desc->Get<CString>(CStrID("UIName"), CString::Empty);
		if (UIDesc.IsEmpty()) UIDesc = Desc->Get<CString>(CStrID("UIDesc"), CString::Empty);

		//???read priorities for actions? or all through scripts?

		Data::PParams UIActionNames = Desc->Get<Data::PParams>(CStrID("UIActionNames"), NULL);

		if (Desc->Get<bool>(CStrID("Explorable"), false))
		{
			CStrID ID("Explore");
			const char* pUIName = UIActionNames.IsValidPtr() ? UIActionNames->Get<CString>(ID, CString::Empty).CStr() : ID.CStr();
			n_assert(AddActionHandler(ID, pUIName, this, &CPropUIControl::OnExecuteExploreAction, 1, false));
			CAction* pAct = GetActionByID(ID);
			pAct->Enabled = UIDesc.IsValid();
			pAct->Visible = pAct->Enabled;
		}

		if (Desc->Get<bool>(CStrID("Selectable"), false))
		{
			CStrID ID("Select");
			const char* pUIName = UIActionNames.IsValidPtr() ? UIActionNames->Get<CString>(ID, CString::Empty).CStr() : ID.CStr();
			n_assert(AddActionHandler(ID, pUIName, this, &CPropUIControl::OnExecuteSelectAction, Priority_Top, false));
		}

		EnableSmartObjReflection(Desc->Get<bool>(CStrID("AutoAddSmartObjActions"), true));
	}

	//???move to the desc as field? per-entity allows not to spawn redundant IAO descs
	//???Shape desc or collision object desc? CollObj can have offset, but Group & Mask must be overridden to support picking.
	CStrID PickShapeID = GetEntity()->GetAttr<CStrID>(CStrID("PickShape"), CStrID::Empty);
	if (PickShapeID.IsValid() && GetEntity()->GetLevel()->GetPhysics())
	{
		CStrID PickShapeURI = CStrID(CString("Physics:") + PickShapeID.CStr() + ".prm");
		Resources::PResource RShape = ResourceMgr->RegisterResource(PickShapeURI);
		if (!RShape->IsLoaded())
		{
			Resources::PResourceLoader Loader = ResourceMgr->CreateDefaultLoaderFor<Physics::CCollisionShape>("prm"); //!!!get ext from URI!
			if (Loader.IsNullPtr()) FAIL;
			ResourceMgr->LoadResourceSync(*RShape, *Loader);
			if (!RShape->IsLoaded()) FAIL;
		}
		Physics::PCollisionShape Shape = RShape->GetObject<Physics::CCollisionShape>();

		U16 Group = PhysicsSrv->CollisionGroups.GetMask("MousePickTarget");
		U16 Mask = PhysicsSrv->CollisionGroups.GetMask("MousePick");

		MousePickShape = n_new(Physics::CNodeAttrCollision);
		MousePickShape->CollObj = n_new(Physics::CCollisionObjMoving);
		MousePickShape->CollObj->Init(*Shape, Group, Mask); // Can specify offset
		MousePickShape->CollObj->SetUserData(*(void**)&GetEntity()->GetUID());
		MousePickShape->CollObj->SetTransform(GetEntity()->GetAttr<matrix44>(CStrID("Transform")));
		MousePickShape->CollObj->AttachToLevel(*GetEntity()->GetLevel()->GetPhysics());

		CPropSceneNode* pProp = GetEntity()->GetProperty<CPropSceneNode>();
		if (pProp && pProp->IsActive())
			pProp->GetNode()->AddAttribute(*MousePickShape);
	}

	CPropScriptable* pProp = GetEntity()->GetProperty<CPropScriptable>();
	if (pProp && pProp->IsActive()) EnableSI(*pProp);

	PROP_SUBSCRIBE_PEVENT(OnPropActivated, CPropUIControl, OnPropActivated);
	PROP_SUBSCRIBE_PEVENT(OnPropDeactivating, CPropUIControl, OnPropDeactivating);
	PROP_SUBSCRIBE_PEVENT(OnLevelSaving, CPropUIControl, OnLevelSaving);
	PROP_SUBSCRIBE_PEVENT(OnMouseEnter, CPropUIControl, OnMouseEnter);
	PROP_SUBSCRIBE_PEVENT(OnMouseLeave, CPropUIControl, OnMouseLeave);
	OK;
}
//---------------------------------------------------------------------

void CPropUIControl::InternalDeactivate()
{
	UNSUBSCRIBE_EVENT(OnPropActivated);
	UNSUBSCRIBE_EVENT(OnPropDeactivating);
	UNSUBSCRIBE_EVENT(OnLevelSaving);
	UNSUBSCRIBE_EVENT(OnMouseEnter);
	UNSUBSCRIBE_EVENT(OnMouseLeave);
	UNSUBSCRIBE_EVENT(OnPropActivated);
	UNSUBSCRIBE_EVENT(OnPropDeactivating);
	UNSUBSCRIBE_EVENT(OnSOActionAvailabile);

	CPropScriptable* pProp = GetEntity()->GetProperty<CPropScriptable>();
	if (pProp && pProp->IsActive()) DisableSI(*pProp);

	HideTip();

	Actions.Clear();

	if (MousePickShape.IsValidPtr())
	{
		MousePickShape->RemoveFromNode();
		MousePickShape->CollObj->RemoveFromLevel();
		MousePickShape = NULL;
	}
}
//---------------------------------------------------------------------

bool CPropUIControl::OnPropActivated(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	Data::PParams P = ((const Events::CEvent&)Event).Params;
	Game::CProperty* pProp = (Game::CProperty*)P->Get<PVOID>(CStrID("Prop"));
	if (!pProp) FAIL;

	if (ReflectSOActions && pProp->IsA<CPropSmartObject>())
	{
		AddSOActions(*(CPropSmartObject*)pProp);
		OK;
	}

	if (MousePickShape.IsValidPtr() && pProp->IsA<CPropSceneNode>() && ((CPropSceneNode*)pProp)->GetNode())
	{
		((CPropSceneNode*)pProp)->GetNode()->AddAttribute(*MousePickShape);
		OK;
	}

	if (pProp->IsA<CPropScriptable>())
	{
		EnableSI(*(CPropScriptable*)pProp);
		OK;
	}

	FAIL;
}
//---------------------------------------------------------------------

bool CPropUIControl::OnPropDeactivating(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	Data::PParams P = ((const Events::CEvent&)Event).Params;
	Game::CProperty* pProp = (Game::CProperty*)P->Get<PVOID>(CStrID("Prop"));
	if (!pProp) FAIL;

	if (ReflectSOActions && pProp->IsA<CPropSmartObject>())
	{
		RemoveSOActions();
		OK;
	}

	if (MousePickShape.IsValidPtr() && pProp->IsA<CPropSceneNode>())
	{
		MousePickShape->RemoveFromNode();
		//MousePickShape->CollObj->RemoveFromLevel();
		OK;
	}

	if (pProp->IsA<CPropScriptable>())
	{
		DisableSI(*(CPropScriptable*)pProp);
		OK;
	}

	FAIL;
}
//---------------------------------------------------------------------

void CPropUIControl::AddSOActions(CPropSmartObject& Prop)
{
	const CString& UIDescPath = GetEntity()->GetAttr<CString>(CStrID("UIDesc"), CString::Empty);
	Data::PParams UIDesc;
	if (UIDescPath.IsValid()) ParamsUtils::LoadParamsFromPRM(CString("GameUI:") + UIDescPath + ".prm", UIDesc);
	Data::PParams Desc = UIDesc.IsValidPtr() ? UIDesc->Get<Data::PParams>(CStrID("SmartObjActions"), NULL) : NULL;
	if (Desc.IsNullPtr()) return;

	const CPropSmartObject::CActionList& SOActions = Prop.GetActions();

	for (UPTR i = 0; i < Desc->GetCount(); ++i)
	{
		const Data::CParam& Prm = Desc->Get(i);
		CStrID ID = Prm.GetName();
		const CPropSmartObject::CAction* pSOAction = SOActions.Get(ID);
		if (!pSOAction) continue;

		const char* pUIName = Prm.GetValue<CString>().CStr();
		n_assert(AddActionHandler(ID, pUIName, this, &CPropUIControl::OnExecuteSmartObjAction, Priority_Default, true));

		CAction* pUIAction = GetActionByID(ID);
		n_assert(pUIAction); // Action is added in AddActionHandler
		pUIAction->Visible = pSOAction->Enabled;
	}
}
//---------------------------------------------------------------------

void CPropUIControl::RemoveSOActions()
{
	for (UPTR i = 0 ; i < Actions.GetCount(); )
	{
		if (Actions[i].IsSOAction) Actions.RemoveAt(i);
		else ++i;
	}
}
//---------------------------------------------------------------------

void CPropUIControl::EnableSmartObjReflection(bool Enable)
{
	if (ReflectSOActions == Enable) return;
	ReflectSOActions = Enable;
	if (ReflectSOActions)
	{
		CPropSmartObject* pProp = GetEntity()->GetProperty<CPropSmartObject>();
		if (pProp && pProp->IsActive()) AddSOActions(*(CPropSmartObject*)pProp);

		PROP_SUBSCRIBE_PEVENT(OnPropActivated, CPropUIControl, OnPropActivated);
		PROP_SUBSCRIBE_PEVENT(OnPropDeactivating, CPropUIControl, OnPropDeactivating);
		PROP_SUBSCRIBE_PEVENT(OnSOActionAvailabile, CPropUIControl, OnSOActionAvailabile);
	}
	else
	{
		UNSUBSCRIBE_EVENT(OnPropActivated);
		UNSUBSCRIBE_EVENT(OnPropDeactivating);
		UNSUBSCRIBE_EVENT(OnSOActionAvailabile);

		RemoveSOActions();
	}
}
//---------------------------------------------------------------------

bool CPropUIControl::OnLevelSaving(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	if (Enabled) GetEntity()->DeleteAttr(CStrID("UIEnabled"));
	else GetEntity()->SetAttr(CStrID("UIEnabled"), false);
	OK;
}
//---------------------------------------------------------------------

bool CPropUIControl::OnMouseEnter(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	CGameLevelView* pView = (CGameLevelView*)((const Events::CEvent&)Event).Params->Get<PVOID>(CStrID("LevelViewPtr"));
	ShowTip(pView);
	OK;
}
//---------------------------------------------------------------------

bool CPropUIControl::OnMouseLeave(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	HideTip();
	OK;
}
//---------------------------------------------------------------------

bool CPropUIControl::OnSOActionAvailabile(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	Data::PParams P = ((const Events::CEvent&)Event).Params;
	CAction* pAction = GetActionByID(P->Get<CStrID>(CStrID("ActionID")));
	if (pAction) pAction->Visible = P->Get<bool>(CStrID("Enabled"));
	OK;
}
//---------------------------------------------------------------------

void CPropUIControl::Enable(bool SetEnabled)
{
	if (Enabled == SetEnabled) return;

	if (!SetEnabled)
	{
		Data::PParams P = n_new(Data::CParams(1));
		P->Set<PVOID>(CStrID("CtlPtr"), this);
		EventSrv->FireEvent(CStrID("HideActionListPopup"), P);
	}

	Enabled = SetEnabled;
}
//---------------------------------------------------------------------

void CPropUIControl::SetUIName(const char* pNewName)
{
	//???use attribute?
	UIName = pNewName;
	if (TipVisible) ShowTip(NULL);
}
//---------------------------------------------------------------------

void CPropUIControl::ShowTip(CGameLevelView* pView)
{
	Data::PParams P = n_new(Data::CParams(2));
	P->Set(CStrID("Text"), UIName.IsValid() ? UIName : CString(GetEntity()->GetUID().CStr()));
	P->Set(CStrID("EntityID"), GetEntity()->GetUID());
	P->Set<PVOID>(CStrID("LevelViewPtr"), pView);
	TipVisible = (EventSrv->FireEvent(CStrID("ShowIAOTip"), P) > 0);
}
//---------------------------------------------------------------------

void CPropUIControl::HideTip()
{
	if (!TipVisible) return;
	EventSrv->FireEvent(CStrID("HideIAOTip")); //!!!later should send entity ID here to identify which tip to hide!
	TipVisible = false;
}
//---------------------------------------------------------------------

bool CPropUIControl::AddActionHandler(CStrID ID, const char* UIName, const char* ScriptFuncName, int Priority, bool IsSOAction)
{
	CPropScriptable* pScriptable = GetEntity()->GetProperty<CPropScriptable>();
	CScriptObject* pScriptObj = pScriptable ? pScriptable->GetScriptObject() : NULL;
	if (!pScriptObj) FAIL;
	return AddActionHandler(ID, UIName, n_new(Events::CEventHandlerScript)(pScriptObj, ScriptFuncName), Priority, IsSOAction);
}
//---------------------------------------------------------------------

bool CPropUIControl::AddActionHandler(CStrID ID, const char* UIName, Events::PEventHandler Handler, int Priority, bool IsSOAction)
{
	for (CArray<CAction>::CIterator It = Actions.Begin(); It != Actions.End(); ++It)
		if (It->ID == ID) FAIL;

	CAction Act(ID, UIName, Priority);

	char EvIDString[64];
	sprintf_s(EvIDString, 63, "OnUIAction%s", ID.CStr());
	Act.EventID = CStrID(EvIDString);
	GetEntity()->Subscribe(Act.EventID, Handler, &Act.Sub);
	if (Act.Sub.IsNullPtr()) FAIL;
	Act.IsSOAction = IsSOAction;

	Actions.InsertSorted(Act);
	
	OK;
}
//---------------------------------------------------------------------

void CPropUIControl::RemoveActionHandler(CStrID ID)
{
	for (CArray<CAction>::CIterator It = Actions.Begin(); It != Actions.End(); ++It)
		if (It->ID == ID)
		{
			Actions.Remove(It);
			return;
		}
}
//---------------------------------------------------------------------

bool CPropUIControl::ExecuteAction(Game::CEntity* pActorEnt, CAction& Action)
{
	if (!Enabled || !Action.Enabled) FAIL;

	Data::PParams P = n_new(Data::CParams(2));
	P->Set(CStrID("ActorEntityPtr"), (PVOID)pActorEnt);
	P->Set(CStrID("ActionID"), Action.ID);
	GetEntity()->FireEvent(Action.EventID, P);

	OK;
}
//---------------------------------------------------------------------

bool CPropUIControl::ExecuteAction(Game::CEntity* pActorEnt, CStrID ID)
{
	if (!Enabled) FAIL;

	CAction* pAction = GetActionByID(ID);
	if (!pAction) FAIL;
	if (pAction->IsSOAction)
	{
		CPropActorBrain* pActor = pActorEnt->GetProperty<CPropActorBrain>();
		CPropSmartObject* pSO = GetEntity()->GetProperty<CPropSmartObject>();
		n_assert(pActor && pSO);
		pAction->Enabled = pSO->IsActionAvailable(ID, pActor);
	}
	return pAction->Enabled && ExecuteAction(pActorEnt, *pAction);
}
//---------------------------------------------------------------------

bool CPropUIControl::ExecuteDefaultAction(Game::CEntity* pActorEnt)
{
	if (!Enabled || !Actions.GetCount()) FAIL;

	// Cmd can have the highest priority but be disabled. Imagine character under the 
	// silence spell who left-clicks on NPC. Default cmd is "Talk" which is disabled
	// and next cmd is "Attack". We definitely don't want to attack friendly NPC by
	// default (left-click) just because we can't speak at the moment xD

	CPropActorBrain* pActor = NULL;
	CPropSmartObject* pSO = NULL;
	if (ReflectSOActions)
	{
		if (pActorEnt) pActor = pActorEnt->GetProperty<CPropActorBrain>();
		pSO = GetEntity()->GetProperty<CPropSmartObject>();
	}

	CAction* pTopAction = Actions.Begin();
	for (CArray<CPropUIControl::CAction>::CIterator It = Actions.Begin(); It != Actions.End(); ++It)
	{
		if (It->IsSOAction)
		{
			n_assert_dbg(pSO);
			It->Enabled = pSO->IsActionAvailable(It->ID, pActor);
			// Update Priority
		}

		// FIXME
		// This line discards disabled actions, so for example door in transition between
		// Opened and Closed has default action Explore, which is executed on left click.
		// Need to solve this and choose desired behaviour. Force set default action?
		if ((*It) < (*pTopAction)) pTopAction = It;
	}

	return ExecuteAction(pActorEnt, *pTopAction);
}
//---------------------------------------------------------------------

void CPropUIControl::ShowPopup(Game::CEntity* pActorEnt)
{
	if (!Enabled) return;

	CPropActorBrain* pActor = NULL;
	CPropSmartObject* pSO = NULL;
	if (ReflectSOActions)
	{
		pActor = pActorEnt->GetProperty<CPropActorBrain>();
		pSO = GetEntity()->GetProperty<CPropSmartObject>();
	}

	int VisibleCount = 0;
	for (CArray<CPropUIControl::CAction>::CIterator It = Actions.Begin(); It != Actions.End(); ++It)
		if (It->Visible)
		{
			if (It->IsSOAction)
			{
				It->Enabled = pSO->IsActionAvailable(It->ID, pActor);
				// Update Priority
			}

			++VisibleCount;
		}

	if (!VisibleCount) return;

	Actions.Sort();

	Data::PParams P = n_new(Data::CParams(2));
	P->Set(CStrID("ActorEntityPtr"), (PVOID)pActorEnt);
	P->Set(CStrID("CtlPtr"), (PVOID)this);
	EventSrv->FireEvent(CStrID("ShowActionListPopup"), P);
}
//---------------------------------------------------------------------

bool CPropUIControl::OnExecuteExploreAction(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	if (!UIDesc.IsValid()) FAIL;
	Data::PParams P = n_new(Data::CParams(1));
	P->Set<CString>(CStrID("UIDesc"), UIDesc);
	EventSrv->ScheduleEvent(CStrID("OnObjectDescRequested"), P);
	OK;
}
//---------------------------------------------------------------------

bool CPropUIControl::OnExecuteSelectAction(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	NOT_IMPLEMENTED;
	//GetEntity()->GetLevel()->AddToSelection(GetEntity()->GetUID());
	OK;
}
//---------------------------------------------------------------------

// Special handler for auto-added smart object actions
bool CPropUIControl::OnExecuteSmartObjAction(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	Data::PParams P = ((const Events::CEvent&)Event).Params;

	Game::CEntity* pActorEnt = (Game::CEntity*)P->Get<PVOID>(CStrID("ActorEntityPtr"));
	n_assert(pActorEnt);

	CStrID ActionID = P->Get<CStrID>(CStrID("ActionID"));

	//!!!CODE DUPLICATION, see brain SI! to method AISrv->CreateUseSOPlan?
	PActionGotoSmartObj ActGoto = n_new(CActionGotoSmartObj);
	ActGoto->Init(GetEntity()->GetUID(), ActionID);

	PActionUseSmartObj ActUse = n_new(CActionUseSmartObj);
	ActUse->Init(GetEntity()->GetUID(), ActionID);

	PActionSequence Plan = n_new(CActionSequence);
	Plan->AddChild(ActGoto);
	Plan->AddChild(ActUse);

	CTask Task;
	Task.Plan = Plan;
	Task.Relevance = Relevance_Absolute;
	Task.FailOnInterruption = false;
	Task.ClearQueueOnFailure = true;
	pActorEnt->GetProperty<Prop::CPropActorBrain>()->EnqueueTask(Task);

	OK;
}
//---------------------------------------------------------------------

}