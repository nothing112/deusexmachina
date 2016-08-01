#pragma once
#ifndef __DEM_L2_GAME_LEVEL_H__
#define __DEM_L2_GAME_LEVEL_H__

#include <Events/EventDispatcher.h>
#include <Scene/SPS.h>
#include <Render/GPUDriver.h>
#include <Data/Regions.h>

// Represents one game location, including all entities in it and property worlds (physics, AI, scene).
// In MVC pattern level would be a Model.

namespace Data
{
	class CParams;
}

namespace Scripting
{
	typedef Ptr<class CScriptObject> PScriptObject;
}

namespace Scene
{
	typedef Ptr<class CSceneNode> PSceneNode;
}

namespace Physics
{
	typedef Ptr<class CPhysicsLevel> PPhysicsLevel;
	typedef int CMaterialType;
}

namespace AI
{
	typedef Ptr<class CAILevel> PAILevel;
}

namespace Game
{
class CEntity;

// Information about a world surface at the given point/region
struct CSurfaceInfo
{
	//float					TerrainHeight;
	float					WorldHeight;
	Physics::CMaterialType	Material;
	//???where to ignore dynamic/AI objects, where not to?
	//???how to check multilevel ground (bridge above a road etc)?
	//???how to check is point inside world geom?
};

class CGameLevel: public Events::CEventDispatcher
{
protected:

	CStrID						ID;
	CString						Name;
	CAABB						BBox;			// Now primarily for culling through a spatial partitioning structure
	Events::PSub				GlobalSub;
	Scripting::PScriptObject	Script;

	Scene::PSceneNode			SceneRoot;
	Scene::CSPS					SPS;			// Spatial partitioning structure
	Physics::PPhysicsLevel		PhysicsLevel;
	AI::PAILevel				AILevel;
	Render::PGPUDriver			HostGPU;

	vector4						AmbientLight;
	//Fog settings
	//???shadow settings?
	//!!!to World::CNatureManager or smth like (weather, time of day etc) and set to the render server directly!

	bool OnEvent(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event);

public:

	CGameLevel(): AmbientLight(0.2f, 0.2f, 0.2f, 1.f) {}
	virtual ~CGameLevel() { Term(); }

	bool					Init(CStrID LevelID, const Data::CParams& Desc);
	void					Term();
	bool					Save(Data::CParams& OutDesc, const Data::CParams* pInitialDesc = NULL);
	//void					RenderDebug();

	//???GetEntityAABB(AABB_Gfx | AABB_Phys);?

	//!!!ensure there are SPS-accelerated queries!
	// Screen queries
	//???pass camera? or move to view? here is more universal, but may need renaming as there is no "screen" at the server part
	//can add shortcut methods to a View, with these names, calling renamed level methods with a view camera
	bool					GetIntersectionAtScreenPos(float XRel, float YRel, vector3* pOutPoint3D = NULL, CStrID* pOutEntityUID = NULL) const;
	UPTR					GetEntitiesAtScreenRect(CArray<CEntity*>& Out, const Data::CRect& RelRect) const;
	bool					GetEntityScreenPos(vector2& Out, const Game::CEntity& Entity, const vector3* Offset = NULL) const;
	bool					GetEntityScreenPosUpper(vector2& Out, const Game::CEntity& Entity) const;
	bool					GetEntityScreenRect(Data::CRect& Out, const Game::CEntity& Entity, const vector3* Offset = NULL) const;

	// Physics-based queries
	UPTR					GetEntitiesInPhysBox(CArray<CEntity*>& Out, const matrix44& OBB) const;
	UPTR					GetEntitiesInPhysSphere(CArray<CEntity*>& Out, const vector3& Center, float Radius) const;
	bool					GetSurfaceInfoBelow(CSurfaceInfo& Out, const vector3& Position, float ProbeLength = 1000.f) const;

	//Other queries
	bool					HostsEntity(CStrID EntityID) const;

	CStrID					GetID() const { return ID; }
	const CString&			GetName() const { return Name; }

	Scene::CSceneNode*		GetSceneRoot() { return SceneRoot.GetUnsafe(); }
	Scene::CSPS*			GetSPS() { return &SPS; }
	Physics::CPhysicsLevel*	GetPhysics() const { return PhysicsLevel.GetUnsafe(); }
	AI::CAILevel*			GetAI() const { return AILevel.GetUnsafe(); }
	Render::CGPUDriver*		GetHostGPU() const { return HostGPU.GetUnsafe(); }
};

typedef Ptr<CGameLevel> PGameLevel;

}

#endif
