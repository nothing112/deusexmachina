#include "Application.h"

#include <IO/IOServer.h>
#include <IO/FSBrowser.h>
#include <IO/PathUtils.h>
#include <Events/EventServer.h>
#include <Events/Subscription.h>
#include <Math/Math.h>
#include <System/Platform.h>
#include <System/OSWindow.h>
#include <Render/SwapChain.h>
#include <Render/GPUDriver.h>
#include <Data/ParamsUtils.h>
#include <Input/InputTranslator.h>
#include <Input/InputDevice.h>

namespace DEM { namespace Core
{

static CString GetProfilesPath()
{
	return CString("RW:profiles/");
}
//---------------------------------------------------------------------

static CString GetUserProfilePath(const char* pUserID)
{
	CString Path = GetProfilesPath();
	Path += pUserID;
	PathUtils::EnsurePathHasEndingDirSeparator(Path);
	return Path;
}
//---------------------------------------------------------------------

static CString GetUserSavesPath(const char* pUserID)
{
	return GetUserProfilePath(pUserID) + "saves/";
}
//---------------------------------------------------------------------

static CString GetUserScreenshotsPath(const char* pUserID)
{
	return GetUserProfilePath(pUserID) + "screenshots/";
}
//---------------------------------------------------------------------

static CString GetUserCurrDataPath(const char* pUserID)
{
	return GetUserProfilePath(pUserID) + "current/";
}
//---------------------------------------------------------------------

static CString GetUserSettingsFilePath(const char* pUserID)
{
	return GetUserProfilePath(pUserID) + "Settings.hrd";
}
//---------------------------------------------------------------------

//???empty constructor, add Init to process init-time failures?
CApplication::CApplication(Sys::IPlatform& _Platform)
	: Platform(_Platform)
{
	// check multiple instances

	//???move RNG instance to an application instead of static vars? pass platform system time as seed?
	// RNG is initialized in constructor to be available anywhere
	Math::InitRandomNumberGenerator();

	// create default file system from platform
	// setup hard assigns from platform and application

	n_new(Events::CEventServer);
	IOServer.reset(n_new(IO::CIOServer));
}
//---------------------------------------------------------------------

CApplication::~CApplication()
{
	if (Events::CEventServer::HasInstance()) n_delete(EventSrv);
}
//---------------------------------------------------------------------

IO::CIOServer& CApplication::IO() const
{
	return *IOServer;
}
//---------------------------------------------------------------------

CStrID CApplication::CreateUserProfile(const char* pUserID)
{
	CString UserStr(pUserID);
	if (UserStr.IsEmpty() || UserStr.ContainsAny("\t\n\r\\/:?&%$#@!~")) return CStrID::Empty;

	CString Path = GetUserProfilePath(pUserID);
	if (IO().DirectoryExists(Path)) return CStrID::Empty;

	if (!IO().CreateDirectory(Path)) return CStrID::Empty;

	bool Result = true;
	if (!IO().CreateDirectory(Path + "/saves")) Result = false;
	if (Result && !IO().CreateDirectory(Path + "/screenshots")) Result = false;
	if (Result && !IO().CreateDirectory(Path + "/current")) Result = false;

	//!!!DBG TMP!
	//!!!template must be packed into NPK or reside in bin/data or smth!
	//!!!app must set file path or CParams for default user settings, engine must not hardcode where they are!
	if (Result && !IO().CopyFile("../content/DefaultUserSettings.hrd", Path + "/Settings.hrd")) Result = false;

	if (!Result)
	{
		IOSrv->DeleteDirectory(Path);
		return CStrID::Empty;
	}

	return CStrID(pUserID);
}
//---------------------------------------------------------------------

bool CApplication::DeleteUserProfile(const char* pUserID)
{
	// Can't delete current user
	if (CurrentUserID == pUserID) FAIL;

	// TODO: if one of active users, FAIL

	return IOSrv->DeleteDirectory(GetUserProfilePath(pUserID));
}
//---------------------------------------------------------------------

UPTR CApplication::EnumUserProfiles(CArray<CStrID>& Out) const
{
	CString ProfilesDir = GetProfilesPath();
	if (!IO().DirectoryExists(ProfilesDir)) return 0;

	const UPTR OldCount = Out.GetCount();

	IO::CFSBrowser Browser;
	Browser.SetAbsolutePath(ProfilesDir);
	if (!Browser.IsCurrDirEmpty()) do
	{
		if (Browser.IsCurrEntryDir())
			Out.Add(CStrID(Browser.GetCurrEntryName()));
	}
	while (Browser.NextCurrDirEntry());

	return Out.GetCount() - OldCount;
}
//---------------------------------------------------------------------

CStrID CApplication::ActivateUser(CStrID UserID)
{
	auto It = std::find_if(ActiveUsers.cbegin(), ActiveUsers.cend(), [UserID](const CUser& User) { return User.ID == UserID; });
	if (It != ActiveUsers.cend()) return UserID;

	CString Path = GetUserProfilePath(UserID);
	PathUtils::EnsurePathHasEndingDirSeparator(Path);

	if (!IO().DirectoryExists(Path)) return CStrID::Empty;

	CUser NewUser;
	NewUser.ID = UserID;
	if (!ParamsUtils::LoadParamsFromHRD(Path + "Settings.hrd", NewUser.Settings)) return CStrID::Empty;

	NewUser.Input.reset(n_new(Input::CInputTranslator(UserID)));
	//!!!load input contexts from app & user settings! may use separate files, not settings files
	//or use sections in settings

	if (!CurrentUserID.IsValid())
	{
		CArray<Input::PInputDevice> InputDevices;
		Platform.EnumInputDevices(InputDevices);
		for (UPTR i = 0; i < InputDevices.GetCount(); ++i)
		{
			NewUser.Input->ConnectToDevice(InputDevices[i].Get());
		}

		CurrentUserID = UserID;

		SetStringSetting("User", CString(UserID), CStrID::Empty);
	}

	ActiveUsers.push_back(std::move(NewUser));

	return UserID;
}
//---------------------------------------------------------------------

void CApplication::DeactivateUser(CStrID UserID)
{
	auto It = std::find_if(ActiveUsers.cbegin(), ActiveUsers.cend(), [UserID](const CUser& User) { return User.ID == UserID; });
	if (It == ActiveUsers.cend()) return;

	//???save settings IF CHANGED? if want to use file time as "last seen" for the user,
	//may use FS::Touch without actually writing the data
	if (It->Settings) ParamsUtils::SaveParamsToHRD(GetUserSettingsFilePath(UserID), *It->Settings);

	ActiveUsers.erase(It);

	if (CurrentUserID == UserID)
	{
		//!!!process current user changes!
		CurrentUserID = ActiveUsers.empty() ? CStrID::Empty : ActiveUsers.back().ID;
	}
}
//---------------------------------------------------------------------

Input::CInputTranslator* CApplication::GetUserInput(CStrID UserID) const
{
	auto It = std::find_if(ActiveUsers.cbegin(), ActiveUsers.cend(), [UserID](const CUser& User) { return User.ID == UserID; });
	return It == ActiveUsers.cend() ? nullptr : It->Input.get();
}
//---------------------------------------------------------------------

void CApplication::ParseCommandLine(const char* pCmdLine)
{
	if (!pCmdLine || !*pCmdLine) return;

	//!!!DBG TMP!
	if (!strcmp(pCmdLine, "-O TestFloat=999.0"))
	{
		OverrideSettings = n_new(Data::CParams(1));
		OverrideSettings->Set<float>(CStrID("TestFloat"), 999.f);
	}
}
//---------------------------------------------------------------------

bool CApplication::LoadSettings(const char* pFilePath, bool Reload, CStrID UserID)
{
	Data::PParams Prm;
	if (!ParamsUtils::LoadParamsFromHRD(pFilePath, Prm)) FAIL;

	if (Reload || !GlobalSettings) GlobalSettings = Prm;
	else GlobalSettings->Merge(*Prm, Data::Merge_Replace | Data::Merge_Deep);

	OK;
}
//---------------------------------------------------------------------

//???need public function?
void CApplication::SaveSettings()
{
	//save all changed setting files (global & per-user)
	//must save global when app exits & user when user is deactivated
}
//---------------------------------------------------------------------

// Private, for internal use only, to avoid declaring all this logic in a header
template<class T>
T CApplication::GetSetting(const char* pKey, const T& Default, CStrID UserID) const
{
	CStrID Key(pKey);

	if (OverrideSettings)
	{
		Data::CData OverrideData;
		if (OverrideSettings->Get<T>(OverrideData, Key))
		{
			return OverrideData.GetValue<T>();
		}
	}

	if (UserID.IsValid())
	{
		auto It = std::find_if(ActiveUsers.cbegin(), ActiveUsers.cend(), [UserID](const CUser& User) { return User.ID == UserID; });
		if (It == ActiveUsers.cend())
		{
			::Sys::Error("CApplication::GetSetting() > requested user is inactive");
			return Default;
		}

		Data::CData UserData;
		if (It->Settings && It->Settings->Get(UserData, Key) && UserData.IsA<T>())
		{
			return UserData.GetValue<T>();
		}
	}

	return GlobalSettings ? GlobalSettings->Get<T>(Key, Default) : Default;
}
//---------------------------------------------------------------------

// Private, for internal use only, to avoid declaring all this logic in a header.
// Returns true if value actually changed.
template<class T> bool CApplication::SetSetting(const char* pKey, const T& Value, CStrID UserID)
{
	CStrID Key(pKey);
	Data::CParams* pSettings = nullptr;

	if (UserID.IsValid())
	{
		auto It = std::find_if(ActiveUsers.begin(), ActiveUsers.end(), [UserID](const CUser& User) { return User.ID == UserID; });
		if (It == ActiveUsers.end())
		{
			::Sys::Error("CApplication::SetSetting() > requested user is inactive");
			FAIL;
		}

		if (!It->Settings) It->Settings = n_new(Data::CParams);
		pSettings = It->Settings.Get();
	}
	else
	{
		if (!GlobalSettings) GlobalSettings = n_new(Data::CParams);
		pSettings = GlobalSettings.Get();
	}

	Data::CParam* pPrm = pSettings->Find(Key);

	// Value not changed
	if (pPrm && pPrm->IsA<T>() && pPrm->GetValue<T>() == Value) FAIL;

	if (pPrm) pPrm->SetValue(Value);
	else pSettings->Set(Key, Value);

	//???set "settings changed" flag for user or global?

	OK;
}
//---------------------------------------------------------------------

bool CApplication::GetBoolSetting(const char* pKey, bool Default, CStrID UserID)
{
	return GetSetting<bool>(pKey, Default, UserID);
}
//---------------------------------------------------------------------

int CApplication::GetIntSetting(const char* pKey, int Default, CStrID UserID)
{
	return GetSetting<int>(pKey, Default, UserID);
}
//---------------------------------------------------------------------

float CApplication::GetFloatSetting(const char* pKey, float Default, CStrID UserID)
{
	return GetSetting<float>(pKey, Default, UserID);
}
//---------------------------------------------------------------------

CString CApplication::GetStringSetting(const char* pKey, const CString& Default, CStrID UserID)
{
	return GetSetting<CString>(pKey, Default, UserID);
}
//---------------------------------------------------------------------

bool CApplication::SetBoolSetting(const char* pKey, bool Value, CStrID UserID)
{
	return SetSetting<bool>(pKey, Value, UserID);
}
//---------------------------------------------------------------------

bool CApplication::SetIntSetting(const char* pKey, int Value, CStrID UserID)
{
	return SetSetting<int>(pKey, Value, UserID);
}
//---------------------------------------------------------------------

bool CApplication::SetFloatSetting(const char* pKey, float Value, CStrID UserID)
{
	return SetSetting<float>(pKey, Value, UserID);
}
//---------------------------------------------------------------------

bool CApplication::SetStringSetting(const char* pKey, const CString& Value, CStrID UserID)
{
	return SetSetting<CString>(pKey, Value, UserID);
}
//---------------------------------------------------------------------

// Creates a GUI window most suitable for 3D scene rendering, based on app & profile settings
int CApplication::CreateRenderWindow(Render::CGPUDriver* pGPU, U32 Width, U32 Height)
{
	auto Wnd = Platform.CreateGUIWindow();
	Wnd->SetRect(Data::CRect(50, 50, Width, Height));

	Render::CRenderTargetDesc BBDesc;
	BBDesc.Format = Render::PixelFmt_DefaultBackBuffer;
	BBDesc.MSAAQuality = Render::MSAA_None;
	BBDesc.UseAsShaderInput = false;
	BBDesc.MipLevels = 0;
	BBDesc.Width = 0;
	BBDesc.Height = 0;

	Render::CSwapChainDesc SCDesc;
	SCDesc.BackBufferCount = 2;
	SCDesc.SwapMode = Render::SwapMode_CopyDiscard;
	SCDesc.Flags = Render::SwapChain_AutoAdjustSize | Render::SwapChain_VSync;

	int SwapChainID = pGPU->CreateSwapChain(BBDesc, SCDesc, Wnd);
	n_assert(pGPU->SwapChainExists(SwapChainID));
	return SwapChainID;
}
//---------------------------------------------------------------------

//???need Run()? use Init()?
bool CApplication::Run(/*initial state?*/)
{
	BaseTime = Platform.GetSystemTime();
	PrevTime = BaseTime;
	FrameTime = 0.0;

	// initialize systems

	// start FSM initial state (enter state)
	OK;
}
//---------------------------------------------------------------------

bool CApplication::Update()
{
	// Update time

	constexpr double MAX_FRAME_TIME = 0.25;

	const double CurrTime = Platform.GetSystemTime();
	FrameTime = CurrTime - PrevTime;
	if (FrameTime < 0.0) FrameTime = 1.0 / 60.0;
	else if (FrameTime > MAX_FRAME_TIME) FrameTime = MAX_FRAME_TIME;
	FrameTime *= TimeScale;

	PrevTime = CurrTime;

	// Process OS messages etc

	Platform.Update();

	// ...

	// Update current application state
	// ...

	//!!!DBG TMP!
	if (Exiting) FAIL;

	OK;
}
//---------------------------------------------------------------------

//!!!Init()'s pair!
void CApplication::Term()
{
}
//---------------------------------------------------------------------

void CApplication::ExitOnWindowClosed(Sys::COSWindow* pWindow)
{
	if (pWindow)
	{
		DISP_SUBSCRIBE_PEVENT(pWindow, OnClosing, CApplication, OnMainWindowClosing);
	}
	else
	{
		UNSUBSCRIBE_EVENT(OnClosing);
	}
}
//---------------------------------------------------------------------

bool CApplication::OnMainWindowClosing(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	UNSUBSCRIBE_EVENT(OnClosing);

	//FSM.RequestState(CStrID::Empty);

	//!!!DBG TMP!
	Exiting = true;

	OK;
}
//---------------------------------------------------------------------

}};
