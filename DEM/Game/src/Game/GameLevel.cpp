#include "GameLevel.h"

#include <Frame/View.h>
#include <Frame/NodeAttrCamera.h>
#include <Game/GameServer.h>
#include <Game/Entity.h>
#include <Game/SceneNodeValidateAttrs.h>
#include <Scripting/ScriptObject.h>
#include <Scene/SceneNodeRenderDebug.h>
#include <Scene/PropSceneNode.h>
#include <Physics/PhysicsLevel.h>
#include <Physics/PhysicsServer.h>
#include <AI/AILevel.h>
#include <Events/EventServer.h>
#include <IO/IOServer.h>
#include <Data/ParamsUtils.h>
#include <Data/DataArray.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <Core/Factory.h>

namespace Game
{

void PhysicsPreTick(btDynamicsWorld* world, btScalar timeStep)
{
	n_assert_dbg(world && world->getWorldUserInfo());
	Data::PParams P = n_new(Data::CParams(1));
	P->Set(CStrID("FrameTime"), (float)timeStep);
	((CGameLevel*)world->getWorldUserInfo())->FireEvent(CStrID("BeforePhysicsTick"), P);
}
//---------------------------------------------------------------------

void PhysicsTick(btDynamicsWorld* world, btScalar timeStep)
{
	n_assert_dbg(world && world->getWorldUserInfo());
	Data::PParams P = n_new(Data::CParams(1));
	P->Set(CStrID("FrameTime"), (float)timeStep);
	((CGameLevel*)world->getWorldUserInfo())->FireEvent(CStrID("AfterPhysicsTick"), P);

	/*for (int i = 0; i < world->getDispatcher()->getNumManifolds(); ++i)
	{
		btPersistentManifold* pManifold = world->getDispatcher()->getManifoldByIndexInternal(i);
		//pManifold->getBody0();
	}*/
}
//---------------------------------------------------------------------

CGameLevel::CGameLevel()
{
}
//---------------------------------------------------------------------

CGameLevel::~CGameLevel()
{
	Term();
}
//---------------------------------------------------------------------

bool CGameLevel::Load(CStrID LevelID, const Data::CParams& Desc)
{
	//n_assert(!Initialized);

	ID = LevelID; //Desc.Get<CStrID>(CStrID("ID"), CStrID::Empty);
	Name = Desc.Get<CString>(CStrID("Name"), CString::Empty);

	CString PathBase("Levels:");
	PathBase += LevelID.CStr();

	// Load level script

	CString ScriptFile = PathBase + ".lua";
	if (IOSrv->FileExists(ScriptFile))
	{
		Script = n_new(Scripting::CScriptObject((CString("Level_") + ID.CStr()).CStr()));
		Script->Init(); // No special class
		if (ExecResultIsError(Script->LoadScriptFile(ScriptFile)))
			Sys::Log("Error loading script for level %s\n", ID.CStr());
	}

	// Create scene and spatial partitioning structure (always)

	vector3 SceneCenter(vector3::Zero);
	vector3 SceneExtents(512.f, 128.f, 512.f);
	int SPSHierarchyDepth = 3;

	Data::PParams SubDesc;
	if (Desc.Get(SubDesc, CStrID("Scene")))
	{
		SubDesc->Get(SceneCenter, CStrID("Center"));
		SubDesc->Get(SceneExtents, CStrID("Extents"));
		SubDesc->Get(SPSHierarchyDepth, CStrID("QuadTreeDepth"));
	}

	SceneRoot = n_new(Scene::CSceneNode(CStrID::Empty));

	//!!!Can load base scene here instead of creating empty root!
	//(loading is especially useful for baking static objects)

	SPS.Init(SceneCenter, SceneExtents * 2.f, (UPTR)SPSHierarchyDepth);

	// Create physics layer, if requested

	if (Desc.Get(SubDesc, CStrID("Physics")))
	{
		vector3 Center = SubDesc->Get(CStrID("Center"), vector3::Zero);
		vector3 Extents = SubDesc->Get(CStrID("Extents"), vector3(512.f, 128.f, 512.f));
		CAABB Bounds(Center, Extents);

		PhysicsLevel = n_new(Physics::CPhysicsLevel);
		if (!PhysicsLevel->Init(Bounds)) FAIL;

		//???load .bullet base contents, useful for static collisions?

		PhysicsLevel->GetBtWorld()->setInternalTickCallback(PhysicsPreTick, this, true);
		PhysicsLevel->GetBtWorld()->setInternalTickCallback(PhysicsTick, this, false);
	}

	// Create AI and navigation layer, if requested

	if (Desc.Get(SubDesc, CStrID("AI")))
	{
		vector3 Center = SubDesc->Get(CStrID("Center"), vector3::Zero);
		vector3 Extents = SubDesc->Get(CStrID("Extents"), vector3(512.f, 128.f, 512.f));
		int QTDepth = SubDesc->Get<int>(CStrID("QuadTreeDepth"), 3);
		CAABB Bounds(Center, Extents);

		AILevel = n_new(AI::CAILevel);
		if (!AILevel->Init(Bounds, QTDepth)) FAIL;

		CString NMFile = PathBase + ".nm";
		if (IOSrv->FileExists(NMFile))
		{
			if (!AILevel->LoadNavMesh(NMFile))
				Sys::Log("Error loading navigation mesh for level %s\n", ID.CStr());

			//Data::PParams NavRegDesc;
			//if (SubDesc->Get(NavRegDesc, CStrID("Regions")))
			//	for (int i = 0; i < NavRegDesc->GetCount(); ++i)
			//		AILevel->SwitchNavRegionFlags(NavRegDesc->Get(i).GetName(), NavRegDesc->Get<bool>(i), NAV_FLAG_LOCKED);
		}
	}

	// Create entities, initially inactive

	if (Desc.Get(SubDesc, CStrID("Entities")))
	{
		FireEvent(CStrID("OnEntitiesLoading"));

		for (UPTR i = 0; i < SubDesc->GetCount(); ++i)
		{
			const Data::CParam& EntityPrm = SubDesc->Get(i);
			if (!EntityPrm.IsA<Data::PParams>()) continue;
			Data::PParams EntityDesc = EntityPrm.GetValue<Data::PParams>();

			//!!!move to separate function to allow creating entities after level is loaded!

			const CString& TplName = EntityDesc->Get<CString>(CStrID("Tpl"), CString::Empty);
			if (TplName.IsValid())
			{
				Data::PParams Tpl;
				ParamsUtils::LoadParamsFromPRM("EntityTpls:" + TplName + ".prm", Tpl);
				if (Tpl.IsNullPtr())
				{
					Sys::Log("Entity template '%s' not found for entity %s in level %s\n",
						TplName.CStr(), EntityPrm.GetName().CStr(), ID.CStr());
					continue;
				}
				Data::PParams MergedDesc = n_new(Data::CParams(EntityDesc->GetCount() + Tpl->GetCount()));
				Tpl->MergeDiff(*MergedDesc, *EntityDesc);
				EntityDesc = MergedDesc;
			}

			Data::CParams& RefEntityDesc = *EntityDesc.Get();

			PEntity Entity = GameSrv->GetEntityMgr()->CreateEntity(EntityPrm.GetName(), *this);
			if (Entity.IsNullPtr())
			{
				Sys::Log("Entity %s in a level %s not loaded\n", EntityPrm.GetName().CStr(), ID.CStr());
				continue;
			}

			Data::PParams AttrsDesc;
			if (RefEntityDesc.Get(AttrsDesc, CStrID("Attrs")) && AttrsDesc->GetCount())
			{
				Entity->BeginNewAttrs(AttrsDesc->GetCount());
				for (UPTR i = 0; i < AttrsDesc->GetCount(); ++i)
				{
					const Data::CParam& Attr = AttrsDesc->Get(i);
					Entity->AddNewAttr(Attr.GetName(), Attr.GetRawValue());
				}
				Entity->EndNewAttrs();
			}

			Data::PDataArray Props;
			if (RefEntityDesc.Get(Props, CStrID("Props")))
				for (UPTR i = 0; i < Props->GetCount(); ++i)
				{
					const Data::CData& PropID = Props->Get(i);
					const Core::CRTTI* pRTTI = NULL;
					if (PropID.IsA<int>()) pRTTI = Factory->GetRTTI((Data::CFourCC)PropID.GetValue<int>());
					else if (PropID.IsA<CString>()) pRTTI = Factory->GetRTTI(PropID.GetValue<CString>());

					if (pRTTI) GameSrv->GetEntityMgr()->AttachProperty(*Entity, pRTTI);
					else Sys::Log("Failed to attach property #%d to entity %s at level %s\n", i, EntityPrm.GetName().CStr(), ID.CStr());
				}
		}

		FireEvent(CStrID("OnEntitiesLoaded"));
	}

	// Broadcast all global events to the level script and hosted entities

	EventSrv->Subscribe(NULL, this, &CGameLevel::OnEvent, &GlobalSub);

	OK;
}
//---------------------------------------------------------------------

//???validation or activation? activation must be one-time, validation must only validate resources!
//!!!OnLevelValidated and OnLevelActivated must be separate events!
bool CGameLevel::Validate(Render::CGPUDriver* pGPU)
{
	HostGPU = pGPU;

	bool Result;
	if (SceneRoot.IsValidPtr())
	{
		CSceneNodeValidateAttrs Visitor;
		Visitor.Level = this;
		Result = SceneRoot->AcceptVisitor(Visitor);
	}
	else Result = true; // Nothing to validate

	FireEvent(CStrID("ValidateEntities"));

	Data::PParams P = n_new(Data::CParams(1));
	P->Set(CStrID("ID"), ID);
	EventSrv->FireEvent(CStrID("OnLevelValidated"), P); //???global or internal?

	return Result;
}
//---------------------------------------------------------------------

void CGameLevel::Term()
{
	GlobalSub = NULL;
	AILevel = NULL;
	PhysicsLevel = NULL;
	SceneRoot = NULL;
	Script = NULL;
}
//---------------------------------------------------------------------

bool CGameLevel::Save(Data::CParams& OutDesc, const Data::CParams* pInitialDesc)
{
	// This is a chance for all properties to write their attrs to entities
	FireEvent(CStrID("OnLevelSaving")); //, &OutDesc);

	// Save selection
	//!!!in views saving!
	//Data::PDataArray SGSelection = n_new(Data::CDataArray);
	//for (UPTR i = 0; i < SelectedEntities.GetCount(); ++i)
	//	SGSelection->Add(SelectedEntities[i]);
	//OutDesc.Set(CStrID("SelectedEntities"), SGSelection);

	// Save nav. regions status
	// No iterator, no consistency. Needs redesign.
/*	if (AILevel.IsValid())
	{
		Data::PParams SGAI = n_new(Data::CParams);
		OutDesc.Set(CStrID("AI"), SGAI);

		// In fact, must save per-nav-poly flags, because regions may intersect
		Data::PParams CurrRegionsDesc = n_new(Data::CParams);
		for ()

		Data::PParams InitialAI;
		Data::PParams InitialRegions;
		if (pInitialDesc &&
			pInitialDesc->Get(InitialAI, CStrID("AI")) &&
			InitialAI->Get(InitialRegions, CStrID("Regions")))
		{
			Data::PParams SGRegions = n_new(Data::CParams);
			InitialRegions->GetDiff(*SGRegions, *CurrRegionsDesc);
			if (SGRegions->GetCount()) SGAI->Set(CStrID("Regions"), SGRegions);
		}
		else SGAI->Set(CStrID("Regions"), CurrRegionsDesc);
	}
*/

	// Save entities diff
	Data::PParams SGEntities = n_new(Data::CParams);

	Data::PParams InitialEntities;
	if (pInitialDesc && pInitialDesc->Get(InitialEntities, CStrID("Entities")))
	{
		for (UPTR i = 0; i < InitialEntities->GetCount(); ++i)
		{
			CStrID EntityID = InitialEntities->Get(i).GetName();
			CEntity* pEntity = GameSrv->GetEntityMgr()->GetEntity(EntityID);
			if (!pEntity || pEntity->GetLevel() != this)
				SGEntities->Set(EntityID, Data::CData());
		}
	}

	//???is there any better way to iterate over all entities of this level? mb send them an event?
	CArray<CEntity*> Entities(128, 128);
	GameSrv->GetEntityMgr()->GetEntitiesByLevel(this, Entities);
	Data::PParams SGEntity = n_new(Data::CParams);
	const Data::CParams* pInitialEntities = InitialEntities.IsValidPtr() && InitialEntities->GetCount() ? InitialEntities.Get() : NULL;
	for (UPTR i = 0; i < Entities.GetCount(); ++i)
	{
		CEntity* pEntity = Entities[i];
		if (SGEntity->GetCount()) SGEntity = n_new(Data::CParams);
		Data::PParams InitialDesc = pInitialEntities ? pInitialEntities->Get<Data::PParams>(pEntity->GetUID(), NULL).Get() : NULL;
		if (InitialDesc.IsValidPtr())
		{
			const CString& TplName = InitialDesc->Get<CString>(CStrID("Tpl"), CString::Empty);
			if (TplName.IsValid())
			{
				Data::PParams Tpl;
				ParamsUtils::LoadParamsFromPRM("EntityTpls:" + TplName + ".prm", Tpl);
				n_assert(Tpl.IsValidPtr());
				Data::PParams MergedDesc = n_new(Data::CParams(InitialDesc->GetCount() + Tpl->GetCount()));
				Tpl->MergeDiff(*MergedDesc, *InitialDesc);
				InitialDesc = MergedDesc;
			}
		}
		pEntity->Save(*SGEntity, InitialDesc);
		if (SGEntity->GetCount()) SGEntities->Set(pEntity->GetUID(), SGEntity);
	}

	OutDesc.Set(CStrID("Entities"), SGEntities);

	OK;
}
//---------------------------------------------------------------------

/*
bool CGameServer::SetActiveLevel(CStrID ID)
{
	PGameLevel NewLevel;
	if (ID.IsValid())
	{
		IPTR LevelIdx = Levels.FindIndex(ID);
		if (LevelIdx == INVALID_INDEX) FAIL;
		NewLevel = Levels.ValueAt(LevelIdx);
	}

	if (NewLevel != ActiveLevel)
	{
		EventSrv->FireEvent(CStrID("OnActiveLevelChanging"));
		ActiveLevel = NewLevel;
		SetGlobalAttr<CStrID>(CStrID("ActiveLevel"), ActiveLevel.IsValidPtr() ? ID : CStrID::Empty);

		EntityUnderMouse = CStrID::Empty;
		HasMouseIsect = false;
		UpdateMouseIntersectionInfo();

		EventSrv->FireEvent(CStrID("OnActiveLevelChanged"));
	}

	OK;
}
//---------------------------------------------------------------------
*/

bool CGameLevel::OnEvent(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	return !!FireEvent(Event);
}
//---------------------------------------------------------------------

/*void CGameLevel::RenderScene()
{
//???add to each view variables?
	RenderSrv->SetAmbientLight(AmbientLight);
	RenderSrv->SetCameraPosition(MainCamera->GetPosition());
	RenderSrv->SetViewProjection(ViewProj);
*/
// Dependent cameras:
	// Some shapes may request textures that are RTs of specific cameras
	// These textures must be rendered before shapes are rendered
	// Good way is to collect all cameras and recurse, filling required textures
	// Pass may disable recursing into cameras to prevent infinite recursion
	// Generally, only FrameBuffer pass should recurse
	// Camera shouldn't render its texture more than once per frame (it won't change)!
	//???as "RenderDependentCameras" flag in Camera/Pass? maybe even filter by dependent camera type
	// Collect - check all meshes to be rendered, check all their textures, select textures rendered from camera RTs
	// Non-mesh dependent textures (shadows etc) must be rendered in one of the previous passes

	// Shapes are sorted by shader, by distance (None, FtB or BtF, depending on shader requirements),
	// by geometry (for the instancing), may be by lights that affect them
	// For the front-to-back sorting, can sort once on first request, always FtB, and when BtF is needed,
	// iterate through the array from the end reversely

	// For each shader (batch):
	// Shader is set
	// Light params are updated, if it is light pass
	// Shader params are updated
	// Shapes are rendered, instanced, when possible

//!!!NOTE: meshes visible from the light's camera are the meshes in light's range!
// can avoid collecting visible meshes on shadow pass
// can render non-shadow-receiving objects without shadow mapping (another technique)
// can render to SM only casters with extruded shadow box visible from main camera

// Rendering must be like this:
	// - begin frame shader
	// - Renderer: apply frame shader constant render states
	// - Renderer: set View and Projection
	// - determine visible meshes [and lights, if lighting is enabled ?in one of passes?]
	// - for each pass, render scene pass, occlusion pass, shadow pass (for shadow-casting lights) or posteffect pass
	// - end frame shader
	//
	// After this some UI, text and debug shapes can be rendered
	// Nebula treats all them as different batches or render plugins, it is good idea maybe...
	// Then backbuffer is present
	//
	// Scene pass:
	// - begin pass
	// - if pass renders dependent textures
	//   - collect them and their cameras (check if already up-to-date this frame)
	//   - if recurse to rendering call with another camera and frame shader
	//   - now our textures from cameras are ready to use
	// - Renderer: set RT
	// - Renderer: optionally clear RT
	// - Renderer: apply pass shader (divide this pass to some phases/batches like opaque, atest, alpha etc inside, mb before a renderer...)
	// - Renderer: set pass technique
	// - Renderer: pass meshes [and lights]
	// - Renderer: render to RT
	//    (link meshes and lights(here?), sort meshes, batch instances, select lighting code,
	//     set shared state of instance sets)
	// - end pass
//}
////---------------------------------------------------------------------

/*
//!!!???bool flags what subsystems to render?
void CGameLevel::RenderDebug()
{
	PhysicsLevel->RenderDebug();

	FireEvent(CStrID("OnRenderDebug"));

	if (SceneRoot.IsValidPtr())
	{
		Scene::CSceneNodeRenderDebug RD;
		SceneRoot->AcceptVisitor(RD);
	}
}
//---------------------------------------------------------------------
*/

//???write 2 versions, physics-based and mesh-based?
bool CGameLevel::GetFirstIntersectedEntity(const line3& Ray, vector3* pOutPoint3D, CStrID* pOutEntityUID) const
{
	if (PhysicsLevel.IsNullPtr()) FAIL;

	U16 Group = PhysicsSrv->CollisionGroups.GetMask("MousePick");
	U16 Mask = PhysicsSrv->CollisionGroups.GetMask("All|MousePickTarget");
	Physics::PPhysicsObj PhysObj;
	if (!PhysicsLevel->GetClosestRayContact(Ray.Start, Ray.End(), Group, Mask, pOutPoint3D, &PhysObj)) FAIL;

	if (pOutEntityUID)
	{
		void* pUserData = PhysObj.IsValidPtr() ? PhysObj->GetUserData() : NULL;
		*pOutEntityUID = pUserData ? *(CStrID*)&pUserData : CStrID::Empty;
	}

	OK;
}
//---------------------------------------------------------------------

UPTR CGameLevel::GetEntitiesAtScreenRect(CArray<CEntity*>& Out, const Data::CRect& RelRect) const
{
	// calc frustum
	// query SPS with this frustum
	// select only render objects
	// return newly selected obj count
	Sys::Error("CGameLevel::GetEntitiesAtScreenRect() -> IMPLEMENT ME!");
	return 0;
}
//---------------------------------------------------------------------

bool CGameLevel::GetEntityScreenPos(vector2& Out, const Game::CEntity& Entity, const vector3* Offset) const
{
	Frame::PNodeAttrCamera MainCamera; //!!!DBG TMP!
	if (MainCamera.IsNullPtr()) FAIL;
	vector3 EntityPos = Entity.GetAttr<matrix44>(CStrID("Transform")).Translation();
	if (Offset) EntityPos += *Offset;
	MainCamera->GetPoint2D(EntityPos, Out.x, Out.y);
	OK;
}
//---------------------------------------------------------------------

bool CGameLevel::GetEntityScreenPosUpper(vector2& Out, const Game::CEntity& Entity) const
{
	Frame::PNodeAttrCamera MainCamera; //!!!DBG TMP!
	if (MainCamera.IsNullPtr()) FAIL;

	Prop::CPropSceneNode* pNode = Entity.GetProperty<Prop::CPropSceneNode>();
	if (!pNode) FAIL;

	CAABB AABB;
	pNode->GetAABB(AABB);
	vector3 Center = AABB.Center();
	MainCamera->GetPoint2D(vector3(Center.x, AABB.Max.y, Center.z), Out.x, Out.y);
	OK;
}
//---------------------------------------------------------------------

UPTR CGameLevel::GetEntitiesInPhysBox(CArray<CEntity*>& Out, const matrix44& OBB) const
{
	// request physics level for shapes and bodies
	// select ones that are attached to entities
	// return newly selected obj count
	Sys::Error("CGameLevel::GetEntitiesInPhysBox() -> IMPLEMENT ME!");
	return 0;
}
//---------------------------------------------------------------------

UPTR CGameLevel::GetEntitiesInPhysSphere(CArray<CEntity*>& Out, const vector3& Center, float Radius) const
{
	// request physics level for shapes and bodies
	// select ones that are attached to entities
	// return newly selected obj count
	Sys::Error("CGameLevel::GetEntitiesInPhysBox() -> IMPLEMENT ME!");
	return 0;
}
//---------------------------------------------------------------------

bool CGameLevel::GetSurfaceInfoBelow(CSurfaceInfo& Out, const vector3& Position, float ProbeLength) const
{
	n_assert(ProbeLength > 0);
	vector3 Dir(0.0f, -ProbeLength, 0.0f);

	//!!!can request closest contacts for Default and Terrain!
	U16 Group = PhysicsSrv->CollisionGroups.GetMask("Default");
	U16 Mask = PhysicsSrv->CollisionGroups.GetMask("All");
	vector3 ContactPos;
	if (!PhysicsLevel->GetClosestRayContact(Position, Position + Dir, Group, Mask, &ContactPos)) FAIL;
	Out.WorldHeight = ContactPos.y;

	//!!!material from CPhysicsObject!

	OK;
}
//---------------------------------------------------------------------

bool CGameLevel::HostsEntity(CStrID EntityID) const
{
	CEntity* pEnt = GameSrv->GetEntityMgr()->GetEntity(EntityID);
	return pEnt && pEnt->GetLevel() == this;
}
//---------------------------------------------------------------------

}