#include "GameServer.h"

#include <Game/EntityLoaderCommon.h>
#include <Game/GameLevelView.h>
#include <AI/AIServer.h>
#include <Frame/SceneNodeValidateResources.h>
#include <Frame/SceneNodeUpdateInSPS.h>
#include <Frame/NodeAttrCamera.h>
#include <Physics/PhysicsLevel.h>
#include <IO/IOServer.h>
#include <IO/FSBrowser.h>
#include <UI/UIServer.h>
#include <Input/InputServer.h>
#include <Time/TimeServer.h>
#include <Scripting/ScriptObject.h>
#include <Data/DataServer.h>
#include <Data/DataArray.h>
#include <Events/EventServer.h>

namespace Game
{
__ImplementClassNoFactory(Game::CGameServer, Core::CObject);
__ImplementSingleton(CGameServer);

bool CGameServer::Open()
{
	n_assert(!IsOpen);

	GameTimeSrc = n_new(Time::CTimeSource);
	TimeSrv->AttachTimeSource(CStrID("Game"), GameTimeSrc);

	if (DefaultLoader.IsNullPtr()) DefaultLoader = n_new(CEntityLoaderCommon);

	IsOpen = true;
	OK;
}
//---------------------------------------------------------------------

void CGameServer::Close()
{
	n_assert(IsOpen);

	for (UPTR i = 0; i < LevelViews.GetCount(); ++i) n_delete(LevelViews[i]);
	LevelViews.Clear();
	LevelViewHandles.Clear();

	TimeSrv->RemoveTimeSource(CStrID("Game"));
	GameTimeSrc = NULL;

	IsOpen = false;
}
//---------------------------------------------------------------------

void CGameServer::Trigger()
{
	EntityMgr->DeferredDeleteEntities();

	EventSrv->FireEvent(CStrID("OnBeginFrame"));

	AISrv->Trigger(); // Pathfinding queries inside

	float FrameTime = (float)GameTimeSrc->GetFrameTime();

	UPTR ViewCount = LevelViews.GetCount();
	vector3* pCOIArray = ViewCount ? (vector3*)_malloca(sizeof(vector3) * ViewCount) : NULL;

	for (UPTR i = 0; i < Levels.GetCount(); ++i)
	{
		CGameLevel* pLevel = Levels.ValueAt(i);

		pLevel->FireEvent(CStrID("BeforeTransforms"));

		UPTR COICount = 0;
		for (UPTR i = 0; i < ViewCount; ++i)
		{
			CGameLevelView* pView = LevelViews[i];
			if (pView->GetLevel() == pLevel)
				pCOIArray[COICount++] = pView->GetCenterOfInterest();
		}

		Scene::CSceneNode* pSceneRoot = pLevel->GetSceneRoot();

		DefferedNodes.Clear(false);
		if (pSceneRoot) pSceneRoot->UpdateTransform(pCOIArray, COICount, false, &DefferedNodes);

		Physics::CPhysicsLevel* pPhysLvl = pLevel->GetPhysics();
		if (pPhysLvl)
		{
			pLevel->FireEvent(CStrID("BeforePhysics"));
			pPhysLvl->Trigger(FrameTime);
			pLevel->FireEvent(CStrID("AfterPhysics"));
		}

		for (UPTR i = 0; i < DefferedNodes.GetCount(); ++i)
			DefferedNodes[i]->UpdateTransform(pCOIArray, COICount, true, NULL);

		pLevel->FireEvent(CStrID("AfterTransforms"));

		if (pSceneRoot && COICount > 0)
		{
			Frame::CSceneNodeUpdateInSPS Visitor;
			Visitor.pSPS = pLevel->GetSPS();
			pSceneRoot->AcceptVisitor(Visitor);
		}
	}

	if (pCOIArray) _freea(pCOIArray);

	EventSrv->FireEvent(CStrID("OnEndFrame"));
}
//---------------------------------------------------------------------

// If group loader exists and is set to NULL, group will be skipped
void CGameServer::SetEntityLoader(CStrID Group, PEntityLoader Loader)
{
	if (Group.IsValid()) Loaders.Set(Group, Loader);
	else DefaultLoader = Loader;
}
//---------------------------------------------------------------------

void CGameServer::ClearEntityLoader(CStrID Group)
{
	if (Group.IsValid()) Loaders.Remove(Group);
	else DefaultLoader = NULL; //???allow?
}
//---------------------------------------------------------------------

bool CGameServer::LoadLevel(CStrID ID, const Data::CParams& Desc)
{
	IPTR LevelIdx = Levels.FindIndex(ID);
	if (LevelIdx != INVALID_INDEX)
	{
		//???update already existing objects or unload the level?
		FAIL;
	}

	Data::PParams P = n_new(Data::CParams);
	P->Set(CStrID("ID"), ID);
	EventSrv->FireEvent(CStrID("OnLevelLoading"), P); //???or after a level is added, but entities aren't loaded?

	PGameLevel Level = n_new(CGameLevel);
	if (!Level->Init(ID, Desc)) FAIL;

	Levels.Add(Level->GetID(), Level);

	Data::PParams SubDesc;
	if (Desc.Get(SubDesc, CStrID("Entities")))
	{
		Level->FireEvent(CStrID("OnEntitiesLoading"));

		for (UPTR i = 0; i < SubDesc->GetCount(); ++i)
		{
			const Data::CParam& EntityPrm = SubDesc->Get(i);
			if (!EntityPrm.IsA<Data::PParams>()) continue;
			Data::PParams EntityDesc = EntityPrm.GetValue<Data::PParams>();

			//!!!move to separate function to allow creating entities after level is loaded!

			CStrID LoadingGroup = EntityDesc->Get<CStrID>(CStrID("LoadingGroup"), CStrID::Empty);
			IPTR LoaderIdx = Loaders.FindIndex(LoadingGroup);
			PEntityLoader Loader = (LoaderIdx == INVALID_INDEX) ? DefaultLoader : Loaders.ValueAt(LoaderIdx);
			if (Loader.IsNullPtr()) continue;

			const CString& TplName = EntityDesc->Get<CString>(CStrID("Tpl"), CString::Empty);
			if (TplName.IsValid())
			{
				Data::PParams Tpl = DataSrv->LoadPRM("EntityTpls:" + TplName + ".prm");
				if (Tpl.IsNullPtr())
				{
					Sys::Log("Entity template '%s' not found for entity %s in level %s\n",
						TplName.CStr(), EntityPrm.GetName().CStr(), Level->GetID().CStr());
					continue;
				}
				Data::PParams MergedDesc = n_new(Data::CParams(EntityDesc->GetCount() + Tpl->GetCount()));
				Tpl->MergeDiff(*MergedDesc, *EntityDesc);
				EntityDesc = MergedDesc;
			}

			if (!Loader->Load(EntityPrm.GetName(), *Level, *EntityDesc))
				Sys::Log("Entity %s not loaded in level %s, group is %s\n",
					EntityPrm.GetName().CStr(), Level->GetID().CStr(), LoadingGroup.CStr());
		}

		Level->FireEvent(CStrID("OnEntitiesLoaded"));
	}

	//!!!to view loading!
	//Data::PDataArray SelArray;
	//if (Desc.Get(SelArray, CStrID("SelectedEntities")))
	//	for (UPTR i = 0; i < SelArray->GetCount(); ++i)
	//		Level->AddToSelection(SelArray->Get<CStrID>(i));

	EventSrv->FireEvent(CStrID("OnLevelLoaded"), P);

	OK;
}
//---------------------------------------------------------------------

void CGameServer::UnloadLevel(CStrID ID)
{
	IPTR LevelIdx = Levels.FindIndex(ID);
	if (ID == INVALID_INDEX) return;

	PGameLevel Level = Levels.ValueAt(LevelIdx);

	Data::PParams P = n_new(Data::CParams(1));
	P->Set(CStrID("ID"), ID);
	EventSrv->FireEvent(CStrID("OnLevelUnloading"), P);

	UPTR i = 0;
	while (i < LevelViews.GetCount())
	{
		CGameLevelView* pView = LevelViews[i];
		if (pView->GetLevel() == Level)
		{
			LevelViewHandles.CloseHandle(pView->GetHandle());
			LevelViews.RemoveAt(i);
			n_delete(pView);
		}
		else ++i;
	}

	Level->FireEvent(CStrID("OnEntitiesUnloading"));
	EntityMgr->DeleteEntities(*Level);
	StaticEnvMgr->DeleteStaticObjects(*Level);
	Level->FireEvent(CStrID("OnEntitiesUnloaded"));

	Levels.RemoveAt(LevelIdx);

	Level->Term();

	EventSrv->FireEvent(CStrID("OnLevelUnloaded"), P);

	n_assert_dbg(Level->GetRefCount() == 1);
}
//---------------------------------------------------------------------

bool CGameServer::ValidateLevel(CGameLevel& Level)
{
	bool Result;
	
	if (Level.GetSceneRoot())
	{
		Frame::CSceneNodeValidateResources Visitor;
		//!!!Visitor.GPU = Level.
		Result = Level.GetSceneRoot()->AcceptVisitor(Visitor);
	}
	else Result = true; // Nothing to validate

	//!!!???activate entities here and not in level loading?!

	Data::PParams P = n_new(Data::CParams(1));
	P->Set(CStrID("ID"), Level.GetID());
	EventSrv->FireEvent(CStrID("OnLevelValidated"), P);
	return Result;
}
//---------------------------------------------------------------------

HHandle CGameServer::CreateLevelView(CStrID LevelID)
{
	IPTR Idx = Levels.FindIndex(LevelID);
	if (Idx == INVALID_INDEX) return INVALID_HANDLE;

	CGameLevel* pLevel = Levels.ValueAt(Idx);
	if (!pLevel) return INVALID_HANDLE;

	CGameLevelView* pView = n_new(CGameLevelView);

	HHandle hView = LevelViewHandles.OpenHandle(pView);
	if (hView == INVALID_HANDLE)
	{
		n_delete(pView);
		return INVALID_HANDLE;
	}

	if (!pView->Setup(*pLevel, hView))
	{
		LevelViewHandles.CloseHandle(hView);
		n_delete(pView);
		return INVALID_HANDLE;
	}

	LevelViews.Add(pView);
	return hView;
}
//---------------------------------------------------------------------

void CGameServer::DestroyLevelView(HHandle hView)
{
	CGameLevelView* pView = (CGameLevelView*)LevelViewHandles.GetHandleData(hView);
	if (!pView) return;
	LevelViewHandles.CloseHandle(hView);
	LevelViews.RemoveByValue(pView);
	n_delete(pView);
}
//---------------------------------------------------------------------

void CGameServer::EnumProfiles(CArray<CString>& Out) const
{
	Out.Clear();

	CString ProfilesDir("AppData:Profiles");
	if (!IOSrv->DirectoryExists(ProfilesDir))
	{
		IOSrv->CreateDirectory(ProfilesDir);
		return;
	}

	IO::CFSBrowser Browser;
	Browser.SetAbsolutePath(ProfilesDir);
	if (!Browser.IsCurrDirEmpty()) do
	{
		if (Browser.IsCurrEntryDir()) Out.Add(Browser.GetCurrEntryName());
	}
	while (Browser.NextCurrDirEntry());
}
//---------------------------------------------------------------------

bool CGameServer::CreateProfile(const char* pName) const
{
	CString ProfileDir("AppData:Profiles/");
	ProfileDir += pName;
	if (IOSrv->DirectoryExists(ProfileDir)) FAIL;
	return IOSrv->CreateDirectory(ProfileDir + "/Saves") &&
		IOSrv->CreateDirectory(ProfileDir + "/Continue/Levels");
}
//---------------------------------------------------------------------

bool CGameServer::DeleteProfile(const char* pName) const
{
	if (CurrProfile == pName && GameFileName.IsValid()) FAIL; //???or stop game and set current to empty?
	return IOSrv->DeleteDirectory(CString("AppData:Profiles/") + pName);
}
//---------------------------------------------------------------------

bool CGameServer::SetCurrentProfile(const char* pName)
{
	CurrProfile = pName;

	//!!!load and apply settings!

	OK;
}
//---------------------------------------------------------------------

//!!!pack saves to files! single PRM would suffice
void CGameServer::EnumSavedGames(CArray<CString>& Out, const char* pProfile) const
{
	Out.Clear();

	CString Path("AppData:Profiles/");
	if (!pProfile || !*pProfile)
	{
		if (CurrProfile.IsEmpty()) return;
		Path += CurrProfile;
	}
	else Path += pProfile;
	Path += "/Saves";

	IO::CFSBrowser Browser;
	Browser.SetAbsolutePath(Path);
	if (!Browser.IsCurrDirEmpty()) do
	{
		if (Browser.IsCurrEntryFile()) Out.Add(Browser.GetCurrEntryName());
	}
	while (Browser.NextCurrDirEntry());
}
//---------------------------------------------------------------------

bool CGameServer::SavedGameExists(const char* pName, const char* pProfile)
{
	if (!pName || !*pName) FAIL;

	CString Path("AppData:Profiles/");
	if (!pProfile || !*pProfile)
	{
		if (CurrProfile.IsEmpty()) FAIL;
		Path += CurrProfile;
	}
	else Path += pProfile;
	Path += "/Saves/";
	Path += pName;

	return IOSrv->DirectoryExists(Path);
}
//---------------------------------------------------------------------

bool CGameServer::StartNewGame(const char* pFileName)
{
	n_assert(CurrProfile.IsValid() && !Levels.GetCount() && !Attrs.GetCount());
	CString ContinueDir = "AppData:Profiles/" + CurrProfile + "/Continue";
	if (IOSrv->DirectoryExists(ContinueDir)) { n_verify_dbg(IOSrv->DeleteDirectory(ContinueDir)); }
	return ContinueGame(pFileName);
}
//---------------------------------------------------------------------

bool CGameServer::ContinueGame(const char* pFileName)
{
	n_assert(CurrProfile.IsValid() && !Levels.GetCount() && !Attrs.GetCount());

	Data::PParams InitialCommon = DataSrv->LoadPRM(pFileName);
	if (InitialCommon.IsNullPtr()) FAIL;

	Data::PParams SGCommon = DataSrv->ReloadPRM("AppData:Profiles/" + CurrProfile + "/Continue/Main.prm", false);

	Data::PParams GameDesc;
	if (SGCommon.IsValidPtr())
	{
		GameDesc = n_new(Data::CParams);
		InitialCommon->MergeDiff(*GameDesc, *SGCommon);
	}
	else GameDesc = InitialCommon;

	Data::PParams SubSection;
	if (GameDesc->Get<Data::PParams>(SubSection, CStrID("Game")) && SubSection->GetCount())
		SubSection->ToDataDict(Attrs);

	if (GameDesc->Get<Data::PParams>(SubSection, CStrID("Time")) && SubSection->GetCount())
		TimeSrv->Load(*SubSection);
	else TimeSrv->ResetAll();

	// Allow custom gameplay managers to load their data
	EventSrv->FireEvent(CStrID("OnGameDescLoaded"), GameDesc->Get<Data::PParams>(CStrID("Managers"), NULL));

	Data::PDataArray LoadedLevels = GetGlobalAttr<Data::PDataArray>(CStrID("LoadedLevels"), NULL);
	if (LoadedLevels.IsNullPtr())
	{
		LoadedLevels = n_new(Data::CDataArray);
		SetGlobalAttr(CStrID("LoadedLevels"), LoadedLevels);
	}

	for (UPTR i = 0; i < LoadedLevels->GetCount(); ++i)
		n_verify(LoadGameLevel(LoadedLevels->Get<CStrID>(i)));

	GameFileName = pFileName;

	EventSrv->FireEvent(CStrID("OnGameLoaded"), GameDesc);

	OK;
}
//---------------------------------------------------------------------

bool CGameServer::LoadGameLevel(CStrID ID)
{
	n_assert(CurrProfile.IsValid());

	CString RelLevelPath = CString(ID.CStr()) + ".prm";

	Data::PParams InitialLvl = DataSrv->LoadPRM("Levels:" + RelLevelPath);
	n_assert(InitialLvl.IsValidPtr());

	CString DiffPath = "AppData:Profiles/" + CurrProfile + "/Continue/Levels/";
	Data::PParams SGLvl = DataSrv->ReloadPRM(DiffPath + RelLevelPath, false);

	Data::PParams LevelDesc;
	if (SGLvl.IsValidPtr())
	{
		LevelDesc = n_new(Data::CParams);
		InitialLvl->MergeDiff(*LevelDesc, *SGLvl);
	}
	else LevelDesc = InitialLvl;

	return LoadLevel(ID, *LevelDesc);
}
//---------------------------------------------------------------------

void CGameServer::UnloadGameLevel(CStrID ID)
{
	IPTR LevelIdx = Levels.FindIndex(ID);
	if (ID == INVALID_INDEX) return;
	n_assert(CurrProfile.IsValid());
	n_verify(CommitLevelDiff(*Levels.ValueAt(LevelIdx)));
	UnloadLevel(ID);
}
//---------------------------------------------------------------------

void CGameServer::PauseGame(bool Pause) const
{
	if (Pause)
	{
		GameTimeSrc->Pause();
		EventSrv->FireEvent(CStrID("OnGamePaused"));
	}
	else
	{
		GameTimeSrc->Unpause();
		EventSrv->FireEvent(CStrID("OnGameUnpaused"));
	}
}
//---------------------------------------------------------------------

bool CGameServer::CommitContinueData()
{
	EntityManager.DeferredDeleteEntities();

	//???!!!here or in Load/Unload level?
	Data::PDataArray LoadedLevels = n_new(Data::CDataArray);
	for (UPTR i = 0; i < Levels.GetCount(); ++i)
		LoadedLevels->Add(Levels.KeyAt(i));
	SetGlobalAttr(CStrID("LoadedLevels"), LoadedLevels);

	Data::PParams GameDesc = DataSrv->LoadPRM(GameFileName);
	if (GameDesc.IsNullPtr()) FAIL;

	// Save main game file with common data
	Data::PParams SGCommon = n_new(Data::CParams);

	// Save global game attributes diff
	Data::PParams SGGame = n_new(Data::CParams);
	Data::PParams GameSection;
	if (!GameDesc->Get<Data::PParams>(GameSection, CStrID("Game")))
		GameSection = n_new(Data::CParams);
	GameSection->GetDiff(*SGGame, Attrs);
	if (SGGame->GetCount()) SGCommon->Set(CStrID("Game"), SGGame);

	// Time data is never present in the initial game file, so save without diff
	Data::PParams SGTime = n_new(Data::CParams);
	TimeSrv->Save(*SGTime);
	if (SGTime->GetCount()) SGCommon->Set(CStrID("Time"), SGTime);

	// Allow custom gameplay managers to save their data
	Data::PParams SGManagers;
	Data::PParams NewManagers = n_new(Data::CParams);
	EventSrv->FireEvent(CStrID("OnGameSaving"), NewManagers);
	Data::PParams InitialManagers;
	if (GameDesc->Get<Data::PParams>(InitialManagers, CStrID("Managers")))
	{
		SGManagers = n_new(Data::CParams);
		InitialManagers->GetDiff(*SGManagers, *NewManagers);
	}
	else SGManagers = NewManagers;
	if (SGManagers->GetCount()) SGCommon->Set(CStrID("Managers"), SGManagers);

	CString Path = "AppData:Profiles/" + CurrProfile + "/Continue";
	IOSrv->CreateDirectory(Path);
	DataSrv->SavePRM(Path + "/Main.prm", SGCommon);

	//!!!DBG TMP!
	DataSrv->SaveHRD(Path + "/Main.hrd", SGCommon);

	// Save diffs of each level
	for (UPTR i = 0; i < Levels.GetCount(); ++i)
		n_verify(CommitLevelDiff(*Levels.ValueAt(i)));

	OK;
}
//---------------------------------------------------------------------

bool CGameServer::CommitLevelDiff(CGameLevel& Level)
{
	EntityManager.DeferredDeleteEntities();

	Data::PParams SGLevel = n_new(Data::CParams);
	Data::PParams LevelDesc = DataSrv->LoadPRM(CString("Levels:") + Level.GetID().CStr() + ".prm");
	if (!Level.Save(*SGLevel, LevelDesc)) FAIL;
	if (SGLevel->GetCount())
	{
		CString DirName = "AppData:Profiles/" + CurrProfile + "/Continue/Levels/";
		IOSrv->CreateDirectory(DirName);
		DataSrv->SavePRM(DirName + Level.GetID().CStr() + ".prm", SGLevel);

		//!!!DBG TMP!
		DataSrv->SaveHRD(DirName + Level.GetID().CStr() + ".hrd", SGLevel);
	}

	OK;
}
//---------------------------------------------------------------------

bool CGameServer::SaveGame(const char* pName)
{
	n_assert(CurrProfile.IsValid());

	if (!CommitContinueData()) FAIL;

	//!!!pack savegame! on load can unpack to the override (continue) directory!
	CString ProfileDir = "AppData:Profiles/" + CurrProfile;
	CString SaveDir = ProfileDir + "/Saves/" + pName;
	if (IOSrv->DirectoryExists(SaveDir)) { n_verify_dbg(IOSrv->DeleteDirectory(SaveDir)); }
	IOSrv->CopyDirectory(ProfileDir + "/Continue", SaveDir, true);

	OK;
}
//---------------------------------------------------------------------

bool CGameServer::LoadGame(const char* pName)
{
	//???event?

	while (Levels.GetCount()) UnloadLevel(Levels.KeyAt(Levels.GetCount() - 1));
	Attrs.Clear();

	CString ProfileDir = "AppData:Profiles/" + CurrProfile;
	CString ContinueDir = ProfileDir + "/Continue";
	if (IOSrv->DirectoryExists(ContinueDir)) { n_verify_dbg(IOSrv->DeleteDirectory(ContinueDir)); }
	IOSrv->CopyDirectory(ProfileDir + "/Saves/" + pName, ContinueDir, true);

	return ContinueGame(GameFileName);
}
//---------------------------------------------------------------------

void CGameServer::ExitGame()
{
	if (!IsGameStarted()) return;

	n_verify(CommitContinueData());
	GameFileName = CString::Empty;

	//???event?

	while (Levels.GetCount()) UnloadLevel(Levels.KeyAt(Levels.GetCount() - 1));
	Attrs.Clear();
}
//---------------------------------------------------------------------

}