#include "D3D9GPUDriver.h"

#include <Render/D3D9/D3D9DriverFactory.h>
#include <Render/D3D9/D3D9DisplayDriver.h>
#include <Render/D3D9/D3D9RenderTarget.h>
#include <Render/D3D9/D3D9DepthStencilBuffer.h>
#include <Render/D3D9/D3D9Texture.h>
#include <Events/EventServer.h>
#include <System/OSWindow.h>
#include <Core/Factory.h>

namespace Render
{
__ImplementClass(Render::CD3D9GPUDriver, 'D9GD', Render::CGPUDriver);

bool CD3D9GPUDriver::Init(DWORD AdapterNumber, EGPUDriverType DriverType)
{
	if (!CGPUDriver::Init(AdapterNumber, DriverType)) FAIL;

	n_assert(AdapterID != Adapter_AutoSelect || Type != GPU_AutoSelect);
	n_assert(AdapterID == Adapter_AutoSelect || D3D9DrvFactory->AdapterExists(AdapterID));

	Type = DriverType; // Cache requested value for further initialization
	OK;
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::Reset(D3DPRESENT_PARAMETERS& D3DPresentParams)
{
	if (!pD3DDevice) FAIL;

	UNSUBSCRIBE_EVENT(OnPaint);

	//!!!unbind and release all resources!

	for (DWORD i = 0; i < CurrRT.GetCount() ; ++i)
		if (CurrRT[i].IsValidPtr())
		{
			CurrRT[i]->Destroy();
			CurrRT[i] = NULL;
		}

	for (int i = 0; i < SwapChains.GetCount() ; ++i)
		SwapChains[i].Release();

	//!!!ReleaseQueries();

	//???call some Release() code instead? kill all except the device itself!
	EventSrv->FireEvent(CStrID("OnRenderDeviceLost"));

	HRESULT hr = pD3DDevice->TestCooperativeLevel();
	while (hr != S_OK && hr != D3DERR_DEVICENOTRESET)
	{
		// NB: In single-threaded app, engine will stuck here until device can be reset
		//???instead of Sleep() return and allow application to do frame & msg processing? then start from here again.
		Sys::Sleep(10);
		hr = pD3DDevice->TestCooperativeLevel();
	}

	hr = pD3DDevice->Reset(&D3DPresentParams);
	if (FAILED(hr))
	{
		Sys::Log("Failed to reset Direct3D9 device object!\n");
		FAIL;
	}

	// Can setup W-buffer: D3DCaps.RasterCaps | D3DPRASTERCAPS_WBUFFER -> D3DZB_USEW

	pD3DDevice->SetRenderState(D3DRS_DITHERENABLE, FALSE);
	pD3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	pD3DDevice->SetRenderState(D3DRS_FILLMODE, /*Wireframe ? D3DFILL_WIREFRAME :*/ D3DFILL_SOLID);

	bool IsFullscreenNow = (D3DPresentParams.Windowed == FALSE);
	bool Failed = false;

	for (int i = 0; i < SwapChains.GetCount() ; ++i)
	{
		CD3D9SwapChain& SC = SwapChains[i];
		
		if (IsFullscreenNow)
		{
			if (!SC.IsFullscreen()) continue;
			SC.TargetWindow->Subscribe<CD3D9GPUDriver>(CStrID("OnPaint"), this, &CD3D9GPUDriver::OnOSWindowPaint, &Sub_OnPaint);
		}
		else
		{
//???may it be that non-implicit swap chain was full->wnd to index 0?
//!!!!When full->wnd, always resore 0th SC by reset and others by create additional SC!!!!
			// Recreate additional swap chains. Skip implicit swap chain, index is always 0.
			if (i != 0)
			{
//!!!wrong backbuffer size!
				D3DPRESENT_PARAMETERS SCD3DPresentParams = { 0 };
				if (!GetCurrD3DPresentParams(SC, SCD3DPresentParams) ||
					FAILED(pD3DDevice->CreateAdditionalSwapChain(&SCD3DPresentParams, &SC.pSwapChain)))
				{
					n_assert_dbg(false);
					DestroySwapChain(i);
					Failed = true;
					continue;
				}
			}

			if (SC.Desc.Flags.Is(SwapChain_AutoAdjustSize))
				SC.TargetWindow->Subscribe<CD3D9GPUDriver>(CStrID("OnSizeChanged"), this, &CD3D9GPUDriver::OnOSWindowSizeChanged, &SC.Sub_OnSizeChanged);
		}

		SC.TargetWindow->Subscribe<CD3D9GPUDriver>(CStrID("OnToggleFullscreen"), this, &CD3D9GPUDriver::OnOSWindowToggleFullscreen, &SC.Sub_OnToggleFullscreen);
		SC.TargetWindow->Subscribe<CD3D9GPUDriver>(CStrID("OnClosing"), this, &CD3D9GPUDriver::OnOSWindowClosing, &SC.Sub_OnClosing);

		IDirect3DSurface9* pRTSurface = NULL;
		if (SC.pSwapChain)
			n_assert(SUCCEEDED(SC.pSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pRTSurface)));
		else
			n_assert(SUCCEEDED(pD3DDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pRTSurface)));
		SC.BackBufferRT->As<CD3D9RenderTarget>()->Create(pRTSurface, NULL);

		// Default swap chain may be automatically set as a render target
		if (!SC.pSwapChain)
		{
			IDirect3DSurface9* pCurrRTSurface = NULL;
			if (SUCCEEDED(pD3DDevice->GetRenderTarget(0, &pCurrRTSurface)) && pCurrRTSurface == pRTSurface)
				CurrRT[0] = (CD3D9RenderTarget*)(SC.BackBufferRT.GetUnsafe());
			if (pCurrRTSurface) pCurrRTSurface->Release();
		}

		if (IsFullscreenNow) break;
	}

	EventSrv->FireEvent(CStrID("OnRenderDeviceReset"));

	IsInsideFrame = false;

	//???!!!resize DS!? (if auto-resize enabled or DS is attached to an SC)

	if (Failed)
	{
		// Partially failed, can handle specifically
		FAIL;
	}

	OK;
}
//---------------------------------------------------------------------

void CD3D9GPUDriver::Release()
{
	if (!pD3DDevice) return;

	UNSUBSCRIBE_EVENT(OnPaint);

	//!!!UnbindD3D9Resources();
	//!!!can call the same event as on lost device!

	CurrRT.SetSize(0);

	//!!!if code won't be reused in Reset(), call DestroySwapChain()!
	for (int i = 0; i < SwapChains.GetCount() ; ++i)
	{
		SwapChains[i].Release();
		SwapChains[i].TargetDisplay = NULL;
		SwapChains[i].TargetWindow = NULL;
	}

	//for (int i = 1; i < MaxRenderTargetCount; i++)
	//	pD3DDevice->SetRenderTarget(i, NULL);
	pD3DDevice->SetDepthStencilSurface(NULL); //???need when auto depth stencil?

	//EventSrv->FireEvent(CStrID("OnRenderDeviceRelease"));

	//!!!ReleaseQueries();

	pD3DDevice->Release();
	pD3DDevice = NULL;

	IsInsideFrame = false;
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::CheckCaps(ECaps Cap)
{
	n_assert(pD3DDevice);

	switch (Cap)
	{
		case Caps_VSTexFiltering_Linear:
			return (D3DCaps.VertexTextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR) && (D3DCaps.VertexTextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR);
		case Caps_VSTex_L16:
			return SUCCEEDED(D3D9DrvFactory->GetDirect3D9()->CheckDeviceFormat(	AdapterID,
																				GetD3DDriverType(Type),
																				D3DFMT_UNKNOWN, //D3DPresentParams.BackBufferFormat,
																				D3DUSAGE_QUERY_VERTEXTEXTURE,
																				D3DRTYPE_TEXTURE,
																				D3DFMT_L16));
		default: FAIL;
	}
}
//---------------------------------------------------------------------

void CD3D9GPUDriver::FillD3DPresentParams(const CRenderTargetDesc& BackBufferDesc, const CSwapChainDesc& SwapChainDesc,
										  const Sys::COSWindow* pWindow, D3DPRESENT_PARAMETERS& D3DPresentParams) const
{
	DWORD BackBufferCount = SwapChainDesc.BackBufferCount ? SwapChainDesc.BackBufferCount : 1;

	switch (SwapChainDesc.SwapMode)
	{
		case SwapMode_CopyPersist:	D3DPresentParams.SwapEffect = BackBufferCount > 1 ? D3DSWAPEFFECT_FLIP : D3DSWAPEFFECT_COPY; break;
		//case SwapMode_FlipPersist:	// D3DSWAPEFFECT_FLIPEX, but it is available only in D3D9Ex
		case SwapMode_CopyDiscard:
		default:					D3DPresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD; break; // Allows runtime to select the best
	}

	if (D3DPresentParams.SwapEffect == D3DSWAPEFFECT_DISCARD)
	{
		// NB: It is recommended to use non-MSAA swap chain, render to MSAA RT and then resolve it to the back buffer
		GetD3DMSAAParams(BackBufferDesc.MSAAQuality, CD3D9DriverFactory::PixelFormatToD3DFormat(BackBufferDesc.Format),
						 D3DPresentParams.MultiSampleType, D3DPresentParams.MultiSampleQuality);
	}
	else
	{
		// MSAA not supported for swap effects other than D3DSWAPEFFECT_DISCARD
		D3DPresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
		D3DPresentParams.MultiSampleQuality = 0;
	}

	D3DPresentParams.hDeviceWindow = pWindow->GetHWND();
	D3DPresentParams.BackBufferCount = BackBufferCount; //!!!N3 always sets 1 in windowed mode! why?
	D3DPresentParams.EnableAutoDepthStencil = FALSE;
	D3DPresentParams.AutoDepthStencilFormat = D3DFMT_UNKNOWN;

	// D3DPRESENT_INTERVAL_ONE - as _DEFAULT, but improves VSync quality at a little cost of processing time (uses another timer)
	D3DPresentParams.PresentationInterval = SwapChainDesc.Flags.Is(SwapChain_VSync) ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
	D3DPresentParams.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

//!!!TEST IT! (fails on debug at least)
#ifndef _DEBUG
	D3DPresentParams.Flags |= D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
#endif

#if DEM_RENDER_DEBUG
	D3DPresentParams.Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
#endif
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::GetCurrD3DPresentParams(const CD3D9SwapChain& SC, D3DPRESENT_PARAMETERS& D3DPresentParams) const
{
	FillD3DPresentParams(SC.BackBufferRT->GetDesc(), SC.Desc, SC.TargetWindow, D3DPresentParams);

	if (SC.IsFullscreen())
	{
		CDisplayMode Mode;
		if (!SC.TargetDisplay->GetCurrentDisplayMode(Mode)) FAIL;
		D3DPresentParams.Windowed = FALSE;
		D3DPresentParams.BackBufferWidth = Mode.Width;
		D3DPresentParams.BackBufferHeight = Mode.Height;
		D3DPresentParams.BackBufferFormat = CD3D9DriverFactory::PixelFormatToD3DFormat(Mode.PixelFormat);
		//D3DPresentParams.FullScreen_RefreshRateInHz = Mode.RefreshRate.GetIntRounded();
	}
	else
	{
		const CRenderTargetDesc& BBDesc = SC.BackBufferRT->GetDesc();
		D3DPresentParams.Windowed = TRUE;
		D3DPresentParams.BackBufferWidth = BBDesc.Width;
		D3DPresentParams.BackBufferHeight = BBDesc.Height;
		D3DPresentParams.BackBufferFormat = D3DFMT_UNKNOWN; // Uses current desktop mode
	}

	OK;
}
//---------------------------------------------------------------------

D3DDEVTYPE CD3D9GPUDriver::GetD3DDriverType(EGPUDriverType DriverType)
{
	switch (DriverType)
	{
		case GPU_AutoSelect:	Sys::Error("CD3D9GPUDriver::GetD3DDriverType() > GPU_AutoSelect isn't an actual GPU driver type"); return D3DDEVTYPE_HAL;
		case GPU_Hardware:		return D3DDEVTYPE_HAL;
		case GPU_Reference:		return D3DDEVTYPE_REF;
		case GPU_Software:		return D3DDEVTYPE_SW;
		case GPU_Null:			return D3DDEVTYPE_NULLREF;
		default:				Sys::Error("CD3D9GPUDriver::GetD3DDriverType() > invalid GPU driver type"); return D3DDEVTYPE_HAL;
	};
}
//---------------------------------------------------------------------

//???bool Windowed as parameter and re-check available MSAA on fullscreen transitions?
bool CD3D9GPUDriver::GetD3DMSAAParams(EMSAAQuality MSAA, D3DFORMAT Format, D3DMULTISAMPLE_TYPE& OutType, DWORD& OutQuality) const
{
#if DEM_RENDER_DEBUG
	OutType = D3DMULTISAMPLE_NONE;
	OutQuality = 0;
	OK;
#else
	switch (MSAA)
	{
		case MSAA_None:	OutType = D3DMULTISAMPLE_NONE; break;
		case MSAA_2x:	OutType = D3DMULTISAMPLE_2_SAMPLES; break;
		case MSAA_4x:	OutType = D3DMULTISAMPLE_4_SAMPLES; break;
		case MSAA_8x:	OutType = D3DMULTISAMPLE_8_SAMPLES; break;
	};

	DWORD QualLevels = 0;
	HRESULT hr = D3D9DrvFactory->GetDirect3D9()->CheckDeviceMultiSampleType(AdapterID,
																			GetD3DDriverType(Type),
																			Format,
																			FALSE, // Windowed
																			OutType,
																			&QualLevels);
	if (hr == D3DERR_NOTAVAILABLE)
	{
		OutType = D3DMULTISAMPLE_NONE;
		OutQuality = 0;
		FAIL;
	}
	n_assert(SUCCEEDED(hr));

	OutQuality = QualLevels ? QualLevels - 1 : 0;

	OK;
#endif
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::CreateD3DDevice(DWORD CurrAdapterID, EGPUDriverType CurrDriverType, D3DPRESENT_PARAMETERS D3DPresentParams)
{
	IDirect3D9* pD3D9 = D3D9DrvFactory->GetDirect3D9();

	D3DDEVTYPE D3DDriverType = GetD3DDriverType(CurrDriverType);

	memset(&D3DCaps, 0, sizeof(D3DCaps));
	n_assert(SUCCEEDED(pD3D9->GetDeviceCaps(CurrAdapterID, D3DDriverType, &D3DCaps)));

#if DEM_RENDER_DEBUG
	DWORD BhvFlags = D3DCREATE_FPU_PRESERVE | D3DCREATE_SOFTWARE_VERTEXPROCESSING;
#else
	DWORD BhvFlags = (D3DCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) ?
		D3DCREATE_HARDWARE_VERTEXPROCESSING :
		D3DCREATE_SOFTWARE_VERTEXPROCESSING;
#endif

	// NB: May fail if can't create requested number of backbuffers
	HRESULT hr = pD3D9->CreateDevice(CurrAdapterID,
									D3DDriverType,
									D3D9DrvFactory->GetFocusWindow()->GetHWND(),
									BhvFlags,
									&D3DPresentParams,
									&pD3DDevice);

	if (FAILED(hr))
	{
		Sys::Log("Failed to create Direct3D9 device object!\n");
		FAIL;
	}

	CurrRT.SetSize(D3DCaps.NumSimultaneousRTs);

	OK;
}
//---------------------------------------------------------------------

// If device exists, creates additional swap chain. If device does not exist, creates a device with an implicit swap chain.
int CD3D9GPUDriver::CreateSwapChain(const CRenderTargetDesc& BackBufferDesc, const CSwapChainDesc& SwapChainDesc, Sys::COSWindow* pWindow)
{
	n_assert2(!BackBufferDesc.UseAsShaderInput, "D3D9 backbuffer reading in shaders currently not supported!");

	Sys::COSWindow* pWnd = pWindow ? pWindow : D3D9DrvFactory->GetFocusWindow();
	n_assert(pWnd);

	//???check all the swap chains not to use this window?

	UINT BBWidth = BackBufferDesc.Width, BBHeight = BackBufferDesc.Height;
	PrepareWindowAndBackBufferSize(*pWnd, BBWidth, BBHeight);

	D3DPRESENT_PARAMETERS D3DPresentParams = { 0 };
	FillD3DPresentParams(BackBufferDesc, SwapChainDesc, pWnd, D3DPresentParams);

	D3DPresentParams.Windowed = TRUE;
	D3DPresentParams.BackBufferWidth = BBWidth;
	D3DPresentParams.BackBufferHeight = BBHeight;
	D3DPresentParams.BackBufferFormat = D3DFMT_UNKNOWN; // Uses current desktop mode

	CArray<CD3D9SwapChain>::CIterator ItSC = NULL;
	IDirect3DSurface9* pRTSurface = NULL;

	if (pD3DDevice)
	{
		// As device exists, implicit swap chain exists too, so create additional one.
		// NB: additional swap chains work only in windowed mode

		IDirect3DSwapChain9* pD3DSwapChain = NULL;
		HRESULT hr = pD3DDevice->CreateAdditionalSwapChain(&D3DPresentParams, &pD3DSwapChain);

		if (FAILED(hr))
		{
			Sys::Error("Failed to create additional Direct3D9 swap chain!\n");
			return ERR_CREATION_ERROR;
		}

		for (ItSC = SwapChains.Begin(); ItSC != SwapChains.End(); ++ItSC)
			if (!ItSC->IsValid()) break;

		if (ItSC == SwapChains.End()) ItSC = SwapChains.Reserve(1);
	
		ItSC->pSwapChain = pD3DSwapChain;

		n_assert(SUCCEEDED(pD3DSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pRTSurface)));
	}
	else
	{
		// There is no swap chain nor device, create D3D9 device with an implicit swap chain

		n_assert(!SwapChains.GetCount());

		bool DeviceCreated = false;
		if (AdapterID == Adapter_AutoSelect)
		{
			DWORD AdapterCount = D3D9DrvFactory->GetAdapterCount();
			for (DWORD CurrAdapterID = 0; CurrAdapterID < AdapterCount; ++CurrAdapterID)
			{
				DeviceCreated = CreateD3DDevice(CurrAdapterID, Type, D3DPresentParams);
				if (DeviceCreated)
				{
					AdapterID = CurrAdapterID;
					break;
				}
			}
		}
		else if (Type == GPU_AutoSelect)
		{
			DeviceCreated = CreateD3DDevice(AdapterID, GPU_Hardware, D3DPresentParams);
			if (DeviceCreated) Type = GPU_Hardware;
			else
			{
				DeviceCreated = CreateD3DDevice(AdapterID, GPU_Reference, D3DPresentParams);
				if (DeviceCreated) Type = GPU_Reference;
			}
		}
		else DeviceCreated = CreateD3DDevice(AdapterID, Type, D3DPresentParams);

		if (!DeviceCreated) return ERR_CREATION_ERROR;

		// Can setup W-buffer: D3DCaps.RasterCaps | D3DPRASTERCAPS_WBUFFER -> D3DZB_USEW

		pD3DDevice->SetRenderState(D3DRS_DITHERENABLE, FALSE);
		pD3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
		pD3DDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

		ItSC = SwapChains.IteratorAt(0); 
		ItSC->pSwapChain = NULL;

		n_assert(SUCCEEDED(pD3DDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pRTSurface)));
	}

	PD3D9RenderTarget RT = n_new(CD3D9RenderTarget);
	RT->Create(pRTSurface, NULL);

	// Default swap chain may be automatically set as a render target
	if (!ItSC->pSwapChain)
	{
		IDirect3DSurface9* pCurrRTSurface = NULL;
		if (SUCCEEDED(pD3DDevice->GetRenderTarget(0, &pCurrRTSurface)) && pCurrRTSurface == pRTSurface)
			CurrRT[0] = RT;
		if (pCurrRTSurface) pCurrRTSurface->Release();
	}

	ItSC->BackBufferRT = RT.GetUnsafe();
	ItSC->TargetWindow = pWnd;
	ItSC->LastWindowRect = pWnd->GetRect();
	ItSC->TargetDisplay = NULL;
	ItSC->Desc = SwapChainDesc;

	pWnd->Subscribe<CD3D9GPUDriver>(CStrID("OnToggleFullscreen"), this, &CD3D9GPUDriver::OnOSWindowToggleFullscreen, &ItSC->Sub_OnToggleFullscreen);
	pWnd->Subscribe<CD3D9GPUDriver>(CStrID("OnClosing"), this, &CD3D9GPUDriver::OnOSWindowClosing, &ItSC->Sub_OnClosing);
	if (SwapChainDesc.Flags.Is(SwapChain_AutoAdjustSize))
		pWnd->Subscribe<CD3D9GPUDriver>(CStrID("OnSizeChanged"), this, &CD3D9GPUDriver::OnOSWindowSizeChanged, &ItSC->Sub_OnSizeChanged);

	return SwapChains.IndexOf(ItSC);
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::DestroySwapChain(DWORD SwapChainID)
{
	if (!SwapChainExists(SwapChainID)) FAIL;
	
	CD3D9SwapChain& SC = SwapChains[SwapChainID];
	bool DefaultSC = !SC.pSwapChain;

	if (!DefaultSC)
	{
		for (DWORD i = 0; i < CurrRT.GetCount(); ++i)
			if (CurrRT[i].GetUnsafe() == SC.BackBufferRT.GetUnsafe())
				SetRenderTarget(i, NULL);
	}

	SC.Release();
	SC.TargetDisplay = NULL;
	SC.TargetWindow = NULL;

	// Default swap chain destroyed means device is destroyed too
	if (DefaultSC) Release();

	OK;
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::SwapChainExists(DWORD SwapChainID) const
{
	return SwapChainID < (DWORD)SwapChains.GetCount() && SwapChains[SwapChainID].IsValid();
}
//---------------------------------------------------------------------

// Does not resize an OS window, since often is called in response to an OS window resize
//???bool ResizeOSWindow?
bool CD3D9GPUDriver::ResizeSwapChain(DWORD SwapChainID, unsigned int Width, unsigned int Height)
{
	if (!SwapChainExists(SwapChainID)) FAIL;

	CD3D9SwapChain& SC = SwapChains[SwapChainID];
	const CRenderTargetDesc& BackBufDesc = SC.BackBufferRT->GetDesc();

	if ((!Width || BackBufDesc.Width == Width) && (!Height || BackBufDesc.Height == Height)) OK;

	//???for child window, assert that size passed is a window size?

	D3DPRESENT_PARAMETERS D3DPresentParams = { 0 };
	if (!GetCurrD3DPresentParams(SC, D3DPresentParams)) FAIL;

	if (SC.IsFullscreen())
	{
		CDisplayMode Mode(
			Width ? Width : D3DPresentParams.BackBufferWidth,
			Height ? Height : D3DPresentParams.BackBufferHeight,
			CD3D9DriverFactory::D3DFormatToPixelFormat(D3DPresentParams.BackBufferFormat));
		if (!SC.TargetDisplay->SupportsDisplayMode(Mode))
		{
			//!!!if (!SC.pTargetDisplay->GetClosestDisplayMode(inout Mode)) FAIL;!
			FAIL;
		}
		D3DPresentParams.BackBufferWidth = Mode.Width;
		D3DPresentParams.BackBufferHeight = Mode.Height;
	}
	else
	{
		if (Width) D3DPresentParams.BackBufferWidth = Width;
		if (Height) D3DPresentParams.BackBufferHeight = Height;
	}

	if (SC.pSwapChain)
	{
		SC.pSwapChain->Release();
		if (FAILED(pD3DDevice->CreateAdditionalSwapChain(&D3DPresentParams, &SC.pSwapChain))) FAIL;

		//!!!restore RT!
	}
	else if (!Reset(D3DPresentParams)) FAIL;

	OK;
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::SwitchToFullscreen(DWORD SwapChainID, CDisplayDriver* pDisplay, const CDisplayMode* pMode)
{
	if (!SwapChainExists(SwapChainID)) FAIL;
	if (pDisplay && AdapterID != pDisplay->GetAdapterID()) FAIL;

	CD3D9SwapChain& SC = SwapChains[SwapChainID];

	// Only one swap chain per adapter can be fullscreen in D3D9
	// Moreover, it is always an implicit swap chain, so fail on any additional one
	// MJP: I'm pretty sure the only way to go fullscreen is to use the implicit swap chain that's part of the device. (c)
	//!!!if any can be, check here there are no fullscreen swap chains currently!
	//if (SC.pSwapChain) FAIL;

	//???if (SC.TargetWindow->IsChild()) FAIL;

	PDisplayDriver DispDrv = NULL;
	if (!pDisplay)
	{
		// Only one output per adapter in a current implementation
		DispDrv = D3D9DrvFactory->CreateDisplayDriver(AdapterID, 0);
		pDisplay = DispDrv.Get();
	}

	CDisplayMode CurrentMode;
	if (!pMode)
	{
		if (!pDisplay->GetCurrentDisplayMode(CurrentMode)) FAIL;
		pMode = &CurrentMode;
	}

	D3DPRESENT_PARAMETERS D3DPresentParams = { 0 };
	FillD3DPresentParams(SC.BackBufferRT->GetDesc(), SC.Desc, SC.TargetWindow, D3DPresentParams);

	D3DPresentParams.Windowed = FALSE;
	D3DPresentParams.BackBufferWidth = pMode->Width;
	D3DPresentParams.BackBufferHeight = pMode->Height;
	D3DPresentParams.BackBufferFormat = CD3D9DriverFactory::PixelFormatToD3DFormat(pMode->PixelFormat);
	//D3DPresentParams.FullScreen_RefreshRateInHz = pMode->RefreshRate.GetIntRounded();

	// Resize window without resizing a swap chain
	CDisplayDriver::CMonitorInfo MonInfo;
	if (!pDisplay->GetDisplayMonitorInfo(MonInfo)) FAIL;
	SC.Sub_OnSizeChanged = NULL;
	SC.LastWindowRect = SC.TargetWindow->GetRect();
	SC.TargetWindow->SetRect(Data::CRect(MonInfo.Left, MonInfo.Top, pMode->Width, pMode->Height), true);
	//!!!check what system does with window on fullscreen transition!

	SC.TargetDisplay = pDisplay;

	if (!Reset(D3DPresentParams))
	{
		//???call SwitchToWindowed?
		SC.TargetWindow->SetRect(SC.LastWindowRect);
		if (SC.Desc.Flags.Is(SwapChain_AutoAdjustSize))
			SC.TargetWindow->Subscribe<CD3D9GPUDriver>(CStrID("OnSizeChanged"), this, &CD3D9GPUDriver::OnOSWindowSizeChanged, &SC.Sub_OnSizeChanged);
		FAIL;
	}

	SC.TargetWindow->SetInputFocus();

	OK;
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::SwitchToWindowed(DWORD SwapChainID, const Data::CRect* pWindowRect)
{
	if (!SwapChainExists(SwapChainID)) FAIL;

	CD3D9SwapChain& SC = SwapChains[SwapChainID];

	// Only one swap chain per adapter can be fullscreen in D3D9.
	// Moreover, it is always an implicit swap chain, so skip transition for
	// any additional swap chain, as it always is in a windowed mode.
	// Use ResizeSwapChain() to change the swap chain size.
	//if (SC.pSwapChain) OK;

	if (pWindowRect)
	{
		SC.LastWindowRect.X = pWindowRect->X;
		SC.LastWindowRect.Y = pWindowRect->Y;
		if (pWindowRect->W > 0) SC.LastWindowRect.W = pWindowRect->W;
		if (pWindowRect->H > 0) SC.LastWindowRect.H = pWindowRect->H;
	}

	D3DPRESENT_PARAMETERS D3DPresentParams = { 0 };
	FillD3DPresentParams(SC.BackBufferRT->GetDesc(), SC.Desc, SC.TargetWindow, D3DPresentParams);

	D3DPresentParams.Windowed = TRUE;
	D3DPresentParams.BackBufferWidth = SC.LastWindowRect.W;
	D3DPresentParams.BackBufferHeight = SC.LastWindowRect.H;
	D3DPresentParams.BackBufferFormat = D3DFMT_UNKNOWN; // Uses current desktop mode

	SC.TargetDisplay = NULL;
	SC.Sub_OnSizeChanged = NULL;
	SC.TargetWindow->SetRect(SC.LastWindowRect);

	return Reset(D3DPresentParams);
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::IsFullscreen(DWORD SwapChainID) const
{
	return SwapChainExists(SwapChainID) && SwapChains[SwapChainID].IsFullscreen();
}
//---------------------------------------------------------------------

PRenderTarget CD3D9GPUDriver::GetSwapChainRenderTarget(DWORD SwapChainID) const
{
	return SwapChainExists(SwapChainID) ? SwapChains[SwapChainID].BackBufferRT : PRenderTarget();
}
//---------------------------------------------------------------------

// NB: Present() should be called as late as possible after EndFrame()
// to improve parallelism between the GPU and the CPU
bool CD3D9GPUDriver::Present(DWORD SwapChainID)
{
	if (IsInsideFrame || !SwapChainExists(SwapChainID)) FAIL;

	CD3D9SwapChain& SC = SwapChains[SwapChainID];

	// For swap chain: Present will fail if called between BeginScene and EndScene pairs unless the
	// render target is not the current render target. //???so don't fail if IsInsideFrame?
	HRESULT hr = SC.pSwapChain ? SC.pSwapChain->Present(NULL, NULL, NULL, NULL, 0) : pD3DDevice->Present(NULL, NULL, NULL, NULL);
	if (FAILED(hr))
	{
		if (hr == D3DERR_DEVICELOST)
		{
			CD3D9SwapChain& ImplicitSC = SwapChains[0];

			D3DPRESENT_PARAMETERS D3DPresentParams = { 0 };
			if (!GetCurrD3DPresentParams(ImplicitSC, D3DPresentParams)) FAIL;
			if (!Reset(D3DPresentParams)) FAIL;
		}
		else if (hr == D3DERR_INVALIDCALL) FAIL;
		else // D3DERR_DRIVERINTERNALERROR, D3DERR_OUTOFVIDEOMEMORY, E_OUTOFMEMORY
		{
			// Destroy and recreate device and all swap chains
			Sys::Error("CD3D9GPUDriver::Present() > IMPLEMENT ME FAILED(hr)!!!");
			FAIL;
		}
	}
	else ++SC.FrameID;

	//// Sync CPU thread with GPU
	//// wait till gpu has finsihed rendering the previous frame
	//gpuSyncQuery[frameId % numSyncQueries]->Issue(D3DISSUE_END);                              
	//++FrameID; //???why here?
	//while (S_FALSE == gpuSyncQuery[frameId % numSyncQueries]->GetData(NULL, 0, D3DGETDATA_FLUSH)) ;

	OK;
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::BeginFrame()
{
	return pD3DDevice && SUCCEEDED(pD3DDevice->BeginScene());

//	n_assert(!IsInsideFrame);
//
//	PrimsRendered = 0;
//	DIPsRendered = 0;
//
//	//???where? once per frame shader change
//	if (!SharedShader.IsValid())
//	{
//		SharedShader = ShaderMgr.GetTypedResource(CStrID("Shared"));
//		n_assert(SharedShader->IsLoaded());
//		hLightAmbient = SharedShader->GetVarHandleByName(CStrID("LightAmbient"));
//		hEyePos = SharedShader->GetVarHandleByName(CStrID("EyePos"));
//		hViewProj = SharedShader->GetVarHandleByName(CStrID("ViewProjection"));
//	}
//
//	// CEGUI overwrites this value without restoring it, so restore each frame
//	pD3DDevice->SetRenderState(D3DRS_FILLMODE, Wireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
//
//	IsInsideFrame = SUCCEEDED(pD3DDevice->BeginScene());
//	return IsInsideFrame;
}
//---------------------------------------------------------------------

void CD3D9GPUDriver::EndFrame()
{
	pD3DDevice->EndScene();

//	n_assert(IsInsideFrame);
//	n_assert(SUCCEEDED(pD3DDevice->EndScene()));
//	IsInsideFrame = false;
//
//	//???is all below necessary? PIX requires it for debugging frame
//	for (int i = 0; i < MaxVertexStreamCount; ++i)
//		CurrVB[i] = NULL;
//	CurrVLayout = NULL;
//	CurrIB = NULL;
//	//!!!UnbindD3D9Resources()
//
//	CoreSrv->SetGlobal<int>("Render_Prim", PrimsRendered);
//	CoreSrv->SetGlobal<int>("Render_DIP", DIPsRendered);
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::SetRenderTarget(DWORD Index, CRenderTarget* pRT)
{
	if (Index >= CurrRT.GetCount()) FAIL;
	if (CurrRT[Index].GetUnsafe() == pRT) OK;

	if (!pRT && Index == 0)
	{
		// Invalid case for D3D9. Restore main RT to default backbuffer.
		for (int i = 0; i < SwapChains.GetCount(); ++i)
			if (!SwapChains[i].pSwapChain)
			{
				pRT = SwapChains[i].BackBufferRT.GetUnsafe();
				break;
			}
	}

	IDirect3DSurface9* pRTSurface = pRT ? ((CD3D9RenderTarget*)pRT)->GetD3DSurface() : NULL;

	n_assert(SUCCEEDED(pD3DDevice->SetRenderTarget(Index, pRTSurface)));
	CurrRT[Index] = (CD3D9RenderTarget*)pRT;

	OK;

//
//	// NB: DS can be set to NULL only by main RT (index 0)
//	//???mb set DS only from main RT?
//	//???doesn't this kill an auto DS surface?
//	IDirect3DSurface9* pDSSurface = pRT ? pRT->GetD3DDepthStencilSurface() : NULL;
//	if ((pDSSurface || Index == 0) && pDSSurface != pCurrDSSurface)
//	{
//		CurrDepthStencilFormat = pRT ? pRT->GetDepthStencilFormat() : D3DFMT_UNKNOWN;
//		SAFE_RELEASE(pCurrDSSurface);
//		pCurrDSSurface = pDSSurface;
//		n_assert(SUCCEEDED(pD3DDevice->SetDepthStencilSurface(pDSSurface)));
//	}
}
//---------------------------------------------------------------------

void CD3D9GPUDriver::Clear(DWORD Flags, DWORD Color, float Depth, uchar Stencil)
{
	if (!Flags) return;

	DWORD D3DFlags = 0;

	// if (M)RT bound
	if (Flags & Clear_Color) D3DFlags |= D3DCLEAR_TARGET;

	// If DS bound
	//if (CurrDepthStencilFormat != PixelFormat_Invalid)
	//{
	//	if (Flags & Clear_Depth) D3DFlags |= D3DCLEAR_ZBUFFER;
	//
	//	if (Flags & Clear_Stencil)
	//	{// If DS has stencil bits
	//		bool HasStencil =
	//			CurrDepthStencilFormat == D3DFMT_D24S8 ||
	//			CurrDepthStencilFormat == D3DFMT_D24X4S4 ||
	//			CurrDepthStencilFormat == D3DFMT_D24FS8 ||
	//			CurrDepthStencilFormat == D3DFMT_D15S1;
	//		if (HasStencil) D3DFlags |= D3DCLEAR_STENCIL;
	//	}
	//}

	n_assert(SUCCEEDED(pD3DDevice->Clear(0, NULL, D3DFlags, Color, Depth, Stencil)));
}
//---------------------------------------------------------------------

//???need mips (add to desc)?
//???allow 3D and cubes? will need RT.Create or CreateRenderTarget(Texture, SurfaceLocation)
PRenderTarget CD3D9GPUDriver::CreateRenderTarget(const CRenderTargetDesc& Desc)
{
	D3DFORMAT RTFmt = CD3D9DriverFactory::PixelFormatToD3DFormat(Desc.Format);
	D3DMULTISAMPLE_TYPE MSAAType;
	DWORD MSAAQuality;
	if (!GetD3DMSAAParams(Desc.MSAAQuality, RTFmt, MSAAType, MSAAQuality)) FAIL;

	IDirect3DSurface9* pSurface = NULL;
	IDirect3DTexture9* pTexture = NULL;

	// Using RT as a shader input requires a texture. Since MSAA textures are not supported in D3D9,
	// MSAA RT will be created as a separate surface anyway and then it will be resolved into a texture.

	if (Desc.UseAsShaderInput)
	{
		UINT Mips = 1;
		DWORD Usage = D3DUSAGE_RENDERTARGET;
		if (MSAAType == D3DMULTISAMPLE_NONE) // Can use texture as an RT and as a shader resource
		{
			//!!!only if requires Mips!
			Usage |= D3DUSAGE_AUTOGENMIPMAP; //???will work with RT texture? else must regenerate mips after rendering to this RT
			Mips = 0; // Autogenerate full mip chain //!!!or some other value if specified!
		}
		if (FAILED(pD3DDevice->CreateTexture(Desc.Width, Desc.Height, Mips, Usage, RTFmt, D3DPOOL_DEFAULT, &pTexture, NULL))) return NULL;
	}

	// Initialize RT surface, use texture level 0 when possible, else create separate surface

	HRESULT hr;
	bool UseTexSurface = (Desc.UseAsShaderInput && MSAAType == D3DMULTISAMPLE_NONE);
	if (UseTexSurface) hr = pTexture->GetSurfaceLevel(0, &pSurface);
	else hr = pD3DDevice->CreateRenderTarget(Desc.Width, Desc.Height, RTFmt, MSAAType, MSAAQuality, FALSE, &pSurface, NULL);

	if (FAILED(hr))
	{
		if (pTexture) pTexture->Release();
		return NULL;
	}

	PD3D9Texture Tex = NULL;
	if (pTexture)
	{
		Tex = n_new(CD3D9Texture);
		Tex->Create(pTexture);
	}

	PD3D9RenderTarget RT = n_new(CD3D9RenderTarget);
	RT->Create(pSurface, Tex);
	return RT.GetUnsafe();
}
//---------------------------------------------------------------------

PDepthStencilBuffer	CD3D9GPUDriver::CreateDepthStencilBuffer(const CRenderTargetDesc& Desc)
{
	D3DFORMAT DSFmt = CD3D9DriverFactory::PixelFormatToD3DFormat(Desc.Format);
	D3DMULTISAMPLE_TYPE MSAAType;
	DWORD MSAAQuality;
	if (!GetD3DMSAAParams(Desc.MSAAQuality, DSFmt, MSAAType, MSAAQuality)) FAIL;

	// Make sure the device supports a depth buffer specified
	HRESULT hr = D3D9DrvFactory->GetDirect3D9()->CheckDeviceFormat(	AdapterID,
																	GetD3DDriverType(Type),
																	D3DFMT_UNKNOWN,
																	D3DUSAGE_DEPTHSTENCIL,
																	D3DRTYPE_SURFACE,
																	DSFmt);
	if (FAILED(hr)) return NULL;

	IDirect3DSurface9* pSurface = NULL;
	if (FAILED(pD3DDevice->CreateDepthStencilSurface(Desc.Width, Desc.Height, DSFmt, MSAAType, MSAAQuality, TRUE, &pSurface, NULL))) return NULL;

	PD3D9DepthStencilBuffer DS = n_new(CD3D9DepthStencilBuffer);
	DS->Create(pSurface);
	return DS.GetUnsafe();

	//!!!COMPATIBILITY TEST! (separate method)
//!!!also test MSAA compatibility (all the same)!
	//// Check that the depth buffer format is compatible with the backbuffer format
	//hr = pD3D9->CheckDepthStencilMatch(	AdapterID,
	//									D3DDriverType,
	//									D3DPresentParams.BackBufferFormat,
	//									D3DPresentParams.BackBufferFormat,
	//									D3DPresentParams.AutoDepthStencilFormat);
	//if (FAILED(hr)) return NULL;
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::OnOSWindowClosing(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	Sys::COSWindow* pWnd = (Sys::COSWindow*)pDispatcher;
	for (int i = 0; i < SwapChains.GetCount(); ++i)
	{
		CD3D9SwapChain& SC = SwapChains[i];
		if (SC.TargetWindow.GetUnsafe() == pWnd)
		{
			DestroySwapChain(i);
			OK; // Only one swap chain is allowed for each window
		}
	}
	OK;
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::OnOSWindowSizeChanged(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	Sys::COSWindow* pWnd = (Sys::COSWindow*)pDispatcher;
	for (int i = 0; i < SwapChains.GetCount(); ++i)
	{
		CD3D9SwapChain& SC = SwapChains[i];
		if (SC.TargetWindow.GetUnsafe() == pWnd)
		{
			// May not fire in fullscreen mode by design, this assert checks it
			// If assertion failed, we should rewrite this handler and maybe some other code
			n_assert_dbg(!SC.IsFullscreen());

			ResizeSwapChain(i, pWnd->GetWidth(), pWnd->GetHeight());
			OK; // Only one swap chain is allowed for each window
		}
	}
	OK;
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::OnOSWindowToggleFullscreen(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	Sys::COSWindow* pWnd = (Sys::COSWindow*)pDispatcher;
	for (int i = 0; i < SwapChains.GetCount(); ++i)
	{
		CD3D9SwapChain& SC = SwapChains[i];
		if (SC.TargetWindow.GetUnsafe() == pWnd)
		{
//!!!DBG TMP!
Sys::DbgOut("Toggle %s: %s\n", SC.IsFullscreen() ? "Full -> Wnd": "Wnd -> Full", pWnd->GetTitle());
			if (SC.IsFullscreen()) n_assert(SwitchToWindowed(i));
			else n_assert(SwitchToFullscreen(i));
			OK;
		}
	}
	OK;
}
//---------------------------------------------------------------------

bool CD3D9GPUDriver::OnOSWindowPaint(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event)
{
	Sys::COSWindow* pWnd = (Sys::COSWindow*)pDispatcher;
	for (int i = 0; i < SwapChains.GetCount(); ++i)
	{
		CD3D9SwapChain& SC = SwapChains[i];
		if (SC.TargetWindow.GetUnsafe() == pWnd)
		{
			n_assert_dbg(SC.IsFullscreen());
			if (SC.pSwapChain) SC.pSwapChain->Present(NULL, NULL, NULL, NULL, 0);
			else pD3DDevice->Present(NULL, NULL, NULL, NULL);
			OK; // Only one swap chain is allowed for each window
		}
	}
	OK;
}
//---------------------------------------------------------------------

}
