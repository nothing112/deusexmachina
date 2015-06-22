#pragma once
#ifndef __DEM_L2_APP_ENV_H__
#define __DEM_L2_APP_ENV_H__

//???!!!forward declarations?
#include <Core/CoreServer.h>
#include <Time/TimeServer.h>
#include <Debug/DebugServer.h>
#include <IO/IOServer.h>
#include <Data/DataServer.h>
#include <Events/EventServer.h>
#include <Scripting/ScriptServer.h>
#include <Debug/DebugDraw.h>
//#include <Audio/AudioServer.h>
#include <Physics/PhysicsServer.h>
#include <Input/InputServer.h>
#include <Game/GameServer.h>
#include <AI/AIServer.h>
#include <UI/UIServer.h>
#include <Render/DisplayMode.h>
#include <Render/VideoDriverFactory.h>
#include <Video/VideoServer.h>
#ifdef RegisterClass
#undef RegisterClass
#endif

//???need at all? redesign application framework!

// Environment class helps to setup and stores ptrs to engine subsystems. Use it to implement
// application classes faster. Init & shutdown processes are split into parts
// grouped by layers (either DEM LN or logical). It's done to allow user to perform
// his own initialization steps correctly.

// NOTE: this class doesn't handle L3 initialization since it belongs to L2. Derive from it into
// your L3 lib or application framework or initialize/release L3 systems manually in application.

namespace App
{

#define AppEnv App::CEnvironment::Instance()

class CEnvironment
{
protected:

	CString							AppName;
	CString							AppVersion;
	CString							AppVendor;

	bool							AllowMultipleInstances;

	CString							WindowTitle;
	CString							IconName;

	Ptr<Time::CTimeServer>			TimeServer;
	Ptr<Debug::CDebugServer>		DebugServer;
	Ptr<IO::CIOServer>				IOServer;
	Ptr<Data::CDataServer>			DataServer;
	Ptr<Scripting::CScriptServer>	ScriptServer;
	Ptr<Events::CEventServer>		EventServer;
	Ptr<Debug::CDebugDraw>			DD;
	Ptr<Physics::CPhysicsServer>	PhysicsServer;
	Ptr<Input::CInputServer>		InputServer;
	//Ptr<Audio::CAudioServer>		AudioServer;
	Ptr<Video::CVideoServer>		VideoServer;
	Ptr<Game::CGameServer>			GameServer;
	Ptr<AI::CAIServer>				AIServer;
	Ptr<UI::CUIServer>				UIServer;

	void RegisterAttributes();

public:


	CEnvironment(): AllowMultipleInstances(false) {}
	
	static CEnvironment* Instance() { static CEnvironment Singleton; return &Singleton; }

	bool InitEngine();			//L1 systems, L2 non-game systems
	void ReleaseEngine();
	bool InitGameSystem();		//L2 game system
	void ReleaseGameSystem();

	void					SetAllowMultipleInstances(bool Allow) { AllowMultipleInstances = Allow; }
	bool					GetAllowMultipleInstances() const { return AllowMultipleInstances; }
};

}

#endif
