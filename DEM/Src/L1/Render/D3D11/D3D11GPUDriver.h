#pragma once
#ifndef __DEM_L1_RENDER_D3D11_GPU_DRIVER_H__
#define __DEM_L1_RENDER_D3D11_GPU_DRIVER_H__

#include <Render/GPUDriver.h>
#include <Render/D3D11/D3D11SwapChain.h>
#include <Data/FixedArray.h>

// Direct3D11 GPU device driver.

struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct D3D11_VIEWPORT;
typedef enum D3D_DRIVER_TYPE D3D_DRIVER_TYPE;
enum D3D11_USAGE;
typedef struct tagRECT RECT;

namespace Render
{
typedef Ptr<class CD3D11RenderTarget> PD3D11RenderTarget;
typedef Ptr<class CD3D11DepthStencilBuffer> PD3D11DepthStencilBuffer;
typedef Ptr<class CD3D11RenderState> PD3D11RenderState;

class CD3D11GPUDriver: public CGPUDriver
{
	__DeclareClass(CD3D11GPUDriver);

public:

	enum
	{
		GPU_Dirty_RT = 0x0001,	// Render target(s)
		GPU_Dirty_DS = 0x0002,	// Depth-stencil buffer
		GPU_Dirty_VP = 0x0004,	// Viewport(s)
		GPU_Dirty_SR = 0x0008,	// Scissor rect(s)

		GPU_Dirty_All = -1 // All bits set, for convenience in ApplyChanges() call
	};

protected:

	Data::CFlags					CurrDirtyFlags;
	CFixedArray<PD3D11RenderTarget>	CurrRT;
	PD3D11DepthStencilBuffer		CurrDS;
	DWORD							MaxViewportCount;
	D3D11_VIEWPORT*					CurrVP;
	RECT*							CurrSR;			//???SR corresp to VP, mb set in pairs and use all 32 bits each for a pair?
	Data::CFlags					VPSRSetFlags;	// 16 low bits indicate whether VP is set or not, same for SR in 16 high bits
	static const DWORD				VP_OR_SR_SET_FLAG_COUNT = 16;

	CArray<CD3D11SwapChain>			SwapChains;
	//bool							IsInsideFrame;
	//bool							Wireframe;

	ID3D11Device*					pD3DDevice;
	ID3D11DeviceContext*			pD3DImmContext;
	//???store also D3D11.1 interfaces? and use for 11.1 methods only.

	CArray<PD3D11RenderState>		RenderStates;

	CD3D11GPUDriver();

	bool			OnOSWindowClosing(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event);
	bool			OnOSWindowSizeChanged(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event);
	bool			OnOSWindowToggleFullscreen(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event);
	//bool			OnOSWindowPaint(Events::CEventDispatcher* pDispatcher, const Events::CEventBase& Event);

	bool			InitSwapChainRenderTarget(CD3D11SwapChain& SC);
	void			Release();

	static D3D_DRIVER_TYPE	GetD3DDriverType(EGPUDriverType DriverType);
	static EGPUDriverType	GetDEMDriverType(D3D_DRIVER_TYPE DriverType);
	static void				GetUsageAccess(DWORD InAccessFlags, bool InitDataProvided, D3D11_USAGE& OutUsage, UINT& OutCPUAccess);

	virtual PVertexLayout	InternalCreateVertexLayout();

	friend class CD3D11DriverFactory;

public:

	virtual ~CD3D11GPUDriver() { Release(); }

	virtual bool				Init(DWORD AdapterNumber, EGPUDriverType DriverType);
	virtual bool				CheckCaps(ECaps Cap);
	virtual DWORD				GetMaxTextureSize(ETextureType Type);
	virtual DWORD				GetMaxMultipleRenderTargetCount() { return CurrRT.GetCount(); }

	virtual int					CreateSwapChain(const CRenderTargetDesc& BackBufferDesc, const CSwapChainDesc& SwapChainDesc, Sys::COSWindow* pWindow);
	virtual bool				DestroySwapChain(DWORD SwapChainID);
	virtual bool				SwapChainExists(DWORD SwapChainID) const;
	virtual bool				ResizeSwapChain(DWORD SwapChainID, unsigned int Width, unsigned int Height);
	virtual bool				SwitchToFullscreen(DWORD SwapChainID, CDisplayDriver* pDisplay = NULL, const CDisplayMode* pMode = NULL);
	virtual bool				SwitchToWindowed(DWORD SwapChainID, const Data::CRect* pWindowRect = NULL);
	virtual bool				IsFullscreen(DWORD SwapChainID) const;
	virtual PRenderTarget		GetSwapChainRenderTarget(DWORD SwapChainID) const;
	//!!!get info, change info (or only recreate?)
	virtual bool				Present(DWORD SwapChainID);
	//virtual void				SaveScreenshot(DWORD SwapChainID, EImageFormat ImageFormat /*use image codec ref?*/, IO::CStream& OutStream);

	virtual bool				SetViewport(DWORD Index, const CViewport* pViewport); // NULL to reset
	virtual bool				GetViewport(DWORD Index, CViewport& OutViewport);
	virtual bool				SetScissorRect(DWORD Index, const Data::CRect* pScissorRect); // NULL to reset
	virtual bool				GetScissorRect(DWORD Index, Data::CRect& OutScissorRect);

	virtual bool				BeginFrame();
	virtual void				EndFrame();
	virtual bool				SetRenderTarget(DWORD Index, CRenderTarget* pRT);
	virtual bool				SetDepthStencilBuffer(CDepthStencilBuffer* pDS);
	virtual void				Clear(DWORD Flags, const vector4& ColorRGBA, float Depth, uchar Stencil);
	virtual void				ClearRenderTarget(CRenderTarget& RT, const vector4& ColorRGBA);

	//???virtual to unify interface? no-op where is not applicable. or only apply on draw etc here?
	DWORD						ApplyChanges(DWORD ChangesToUpdate = GPU_Dirty_All); // returns a combination of dirty flags where errors occured

	//!!!D3D11 vertex layout must be a key to a list of different layout interfaces, each for a particular shader input signature!
	//it will be resolved at a draw call, when both shader and VB are bound
	//???!!!support multichannel VBs?!
	virtual PVertexBuffer		CreateVertexBuffer(CVertexLayout& VertexLayout, DWORD VertexCount, DWORD AccessFlags, const void* pData = NULL);
	virtual PIndexBuffer		CreateIndexBuffer(EIndexType IndexType, DWORD IndexCount, DWORD AccessFlags, const void* pData = NULL);
	virtual PVertexLayout		CreateVertexLayout() { return NULL; } // Prefer GetVertexLayout() when possible
	virtual PRenderState		CreateRenderState(const Data::CParams& Desc);
	virtual PTexture			CreateTexture(const CTextureDesc& Desc, DWORD AccessFlags, const void* pData = NULL, bool MipDataProvided = false);
	virtual PRenderTarget		CreateRenderTarget(const CRenderTargetDesc& Desc);
	virtual PDepthStencilBuffer	CreateDepthStencilBuffer(const CRenderTargetDesc& Desc);

	//void					SetWireframe(bool Wire);
	//bool					IsWireframe() const { return Wireframe; }

	//IDirect3DDevice9*		GetD3DDevice() const { return pD3DDevice; }
};

typedef Ptr<CD3D11GPUDriver> PD3D11GPUDriver;

inline CD3D11GPUDriver::CD3D11GPUDriver():
	SwapChains(1, 1),
	pD3DDevice(NULL),
	pD3DImmContext(NULL),
	CurrVP(NULL),
	CurrSR(NULL),
	MaxViewportCount(0) /*, IsInsideFrame(false)*/
{
}
//---------------------------------------------------------------------

}

#endif
