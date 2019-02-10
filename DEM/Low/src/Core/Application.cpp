#include "Application.h"

#include <Core/ApplicationState.h>
#include <IO/IOServer.h>
#include <IO/FSBrowser.h>
#include <IO/PathUtils.h>
#include <Events/EventServer.h>
#include <Events/Subscription.h>
#include <Resources/ResourceManager.h>
#include <Resources/Resource.h>
#include <Math/Math.h>
#include <System/Platform.h>
#include <System/OSWindow.h>
#include <Frame/View.h>
#include <Frame/RenderPath.h>
#include <Render/SwapChain.h>
#include <Render/RenderTarget.h>
#include <Render/GPUDriver.h>
#include <Data/ParamsUtils.h>
#include <Input/InputTranslator.h>
#include <Input/InputDevice.h>

namespace DEM { namespace Core
{

CString GetProfilesPath(const CString& AppDataPath)
{
	return AppDataPath + "profiles/";
}
//---------------------------------------------------------------------

static CString GetUserProfilePath(const CString& AppDataPath, const char* pUserID)
{
	CString Path = GetProfilesPath(AppDataPath);
	Path += pUserID;
	PathUtils::EnsurePathHasEndingDirSeparator(Path);
	return Path;
}
//---------------------------------------------------------------------

static CString GetUserSavesPath(const CString& AppDataPath, const char* pUserID)
{
	return GetUserProfilePath(AppDataPath, pUserID) + "saves/";
}
//---------------------------------------------------------------------

static CString GetUserScreenshotsPath(const CString& AppDataPath, const char* pUserID)
{
	return GetUserProfilePath(AppDataPath, pUserID) + "screenshots/";
}
//---------------------------------------------------------------------

static CString GetUserCurrDataPath(const CString& AppDataPath, const char* pUserID)
{
	return GetUserProfilePath(AppDataPath, pUserID) + "current/";
}
//---------------------------------------------------------------------

static CString GetUserSettingsFilePath(const CString& AppDataPath, const char* pUserID)
{
	return GetUserProfilePath(AppDataPath, pUserID) + "Settings.hrd";
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
	ResMgr.reset(n_new(Resources::CResourceManager(IOServer.get())));
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

//UI::CUIServer& CApplication::UI() const
//{
//	return *UIServer;
//}
//---------------------------------------------------------------------

Resources::CResourceManager& CApplication::ResourceManager() const
{
	return *ResMgr;
}
//---------------------------------------------------------------------

CStrID CApplication::CreateUserProfile(const char* pUserID)
{
	CString UserStr(pUserID);
	if (UserStr.IsEmpty() || UserStr.ContainsAny("\t\n\r\\/:?&%$#@!~")) return CStrID::Empty;

	CString Path = GetUserProfilePath(AppDataPath, pUserID);
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
		IO().DeleteDirectory(Path);
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

	return IO().DeleteDirectory(GetUserProfilePath(AppDataPath, pUserID));
}
//---------------------------------------------------------------------

UPTR CApplication::EnumUserProfiles(CArray<CStrID>& Out) const
{
	CString ProfilesDir = GetProfilesPath(AppDataPath);
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

	CString Path = GetUserProfilePath(AppDataPath, UserID);
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

		//!!!
		//SetStringSetting("User", UserID.CStr(), CStrID::Empty);
	}

	ActiveUsers.push_back(std::move(NewUser));

	return UserID;
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

int CApplication::GetIntSetting(const char* pKey, int Default, CStrID UserID)
{
	if (OverrideSettings)
	{
		Data::CData OverrideData;
		if (OverrideSettings->Get(OverrideData, CStrID(pKey)) && OverrideData.IsA<int>())
		{
			return OverrideData.GetValue<int>();
		}
	}

	if (UserID.IsValid())
	{
		auto It = std::find_if(ActiveUsers.cbegin(), ActiveUsers.cend(), [UserID](const CUser& User) { return User.ID == UserID; });
		if (It == ActiveUsers.cend())
		{
			::Sys::Error("CApplication::GetIntSetting() > requested user is inactive");
			return Default;
		}

		Data::CData UserData;
		if (It->Settings->Get(UserData, CStrID(pKey)) && UserData.IsA<int>())
		{
			return UserData.GetValue<int>();
		}
	}

	return GlobalSettings ? GlobalSettings->Get<int>(CStrID(pKey), Default) : Default;
}
//---------------------------------------------------------------------

float CApplication::GetFloatSetting(const char* pKey, float Default, CStrID UserID)
{
	if (OverrideSettings)
	{
		Data::CData OverrideData;
		if (OverrideSettings->Get(OverrideData, CStrID(pKey)) && OverrideData.IsA<float>())
		{
			return OverrideData.GetValue<float>();
		}
	}

	if (UserID.IsValid())
	{
		auto It = std::find_if(ActiveUsers.cbegin(), ActiveUsers.cend(), [UserID](const CUser& User) { return User.ID == UserID; });
		if (It == ActiveUsers.cend())
		{
			::Sys::Error("CApplication::GetIntSetting() > requested user is inactive");
			return Default;
		}

		Data::CData UserData;
		if (It->Settings->Get(UserData, CStrID(pKey)) && UserData.IsA<float>())
		{
			return UserData.GetValue<float>();
		}
	}

	return GlobalSettings ? GlobalSettings->Get<float>(CStrID(pKey), Default) : Default;
}
//---------------------------------------------------------------------

const CString& CApplication::GetStringSetting(const char* pKey, const CString& Default, CStrID UserID)
{
	if (OverrideSettings)
	{
		Data::CData OverrideData;
		if (OverrideSettings->Get(OverrideData, CStrID(pKey)) && OverrideData.IsA<CString>())
		{
			return OverrideData.GetValue<CString>();
		}
	}

	if (UserID.IsValid())
	{
		auto It = std::find_if(ActiveUsers.cbegin(), ActiveUsers.cend(), [UserID](const CUser& User) { return User.ID == UserID; });
		if (It == ActiveUsers.cend())
		{
			::Sys::Error("CApplication::GetIntSetting() > requested user is inactive");
			return Default;
		}

		Data::CData UserData;
		if (It->Settings->Get(UserData, CStrID(pKey)) && UserData.IsA<CString>())
		{
			return UserData.GetValue<CString>();
		}
	}

	return GlobalSettings ? GlobalSettings->Get<CString>(CStrID(pKey), Default) : Default;
}
//---------------------------------------------------------------------

// Creates a GUI window most suitable for 3D scene rendering, based on app & profile settings
int CApplication::CreateRenderWindow(Render::CGPUDriver& GPU, U32 Width, U32 Height)
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

	const int SwapChainID = GPU.CreateSwapChain(BBDesc, SCDesc, Wnd);
	n_assert(GPU.SwapChainExists(SwapChainID));
	return SwapChainID;
}
//---------------------------------------------------------------------

Frame::PView CApplication::CreateFrameView(Render::CGPUDriver& GPU, int SwapChainID, const char* pRenderPathID, bool WithGUI)
{
	if (!GPU.SwapChainExists(SwapChainID)) return nullptr;

	Render::PRenderTarget SwapChainRT = GPU.GetSwapChainRenderTarget(SwapChainID);

	Resources::PResource RRP = ResMgr->RegisterResource<Frame::CRenderPath>(pRenderPathID);

	Frame::PView View(n_new(Frame::CView));
	View->GPU = &GPU;
	View->SetRenderPath(RRP->ValidateObject<Frame::CRenderPath>());
	if (View->RTs.GetCount()) View->RTs[0] = SwapChainRT;

	if (WithGUI)
	{
		NOT_IMPLEMENTED;
		// New CEGUI context must be created
		// Current CEGUI architecture is suited bad for this,
		// so now you must set UI context in the calling code

		//UI::CUIContextSettings UICtxSettings;
		//UICtxSettings.HostWindow = GPU.GetSwapChainWindow(SwapChainID);
		//UICtxSettings.Width = static_cast<float>(SwapChainRT->GetDesc().Width);
		//UICtxSettings.Height = static_cast<float>(SwapChainRT->GetDesc().Height);
		//View->UIContext = UIServer->CreateContext(UICtxSettings);

		//???need always?
		//Input::CInputTranslator* pUserInput = GetUserInput(GetCurrentUserID());
		//if (pUserInput)
		//{
		//	// Bypass context //!!!check if exists!
		//	pUserInput->CreateContext(CStrID("UI"), true);
		//	pUserInput->EnableContext(CStrID("UI"));
		//	FrameView->UIContext->SubscribeOnInput(pUserInput, 100);
		//}
	}

	return View;
}
//---------------------------------------------------------------------

bool CApplication::Run(PApplicationState InitialState)
{
	BaseTime = Platform.GetSystemTime();
	PrevTime = BaseTime;
	FrameTime = 0.0;

	RequestedState = InitialState;

	return InitialState.IsValidPtr();
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

	if (!Platform.Update()) FAIL;

	//???render views here or in states? what about video?

	// Update application state

	if (CurrState != RequestedState)
	{
		if (CurrState) CurrState->OnExit(*this, RequestedState.Get());
		if (RequestedState) RequestedState->OnEnter(*this, CurrState.Get());
		CurrState = RequestedState;
	}

	if (CurrState)
	{
		RequestedState = CurrState->Update(*this, FrameTime);
		OK;
	}
	else FAIL;
}
//---------------------------------------------------------------------

//!!!Init()'s pair!
void CApplication::Term()
{
	UNSUBSCRIBE_EVENT(OnClosing);

	//!!!kill all windows!
}
//---------------------------------------------------------------------

void CApplication::RequestState(PApplicationState NewState)
{
	RequestedState = NewState;
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
	RequestState(nullptr);
	OK;
}
//---------------------------------------------------------------------

}};
