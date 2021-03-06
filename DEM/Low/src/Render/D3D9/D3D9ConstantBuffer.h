#pragma once
#ifndef __DEM_L1_RENDER_D3D9_CONSTANT_BUFFER_H__
#define __DEM_L1_RENDER_D3D9_CONSTANT_BUFFER_H__

#include <Render/ConstantBuffer.h>
#include <Render/RenderFwd.h>
#include <Render/D3D9/D3D9Fwd.h>
#include <Data/Flags.h>

// A Direct3D9 implementation of a shader constant buffer.

typedef int BOOL;

namespace Render
{
struct CSM30BufferMeta;

class CD3D9ConstantBuffer: public CConstantBuffer
{
	__DeclareClass(CD3D9ConstantBuffer);

protected:

	enum
	{
		CB9_DirtyFloat4	= 0x01,
		CB9_DirtyInt4	= 0x02,
		CB9_DirtyBool	= 0x04
	};

	float*			pFloat4Data = nullptr; //???or one ptr and offsets ??on the fly?? ?
	UPTR			Float4Count = 0;

	int*			pInt4Data = nullptr;
	UPTR			Int4Count = 0;

	BOOL*			pBoolData = nullptr;
	UPTR			BoolCount = 0;

	HConstBuffer	Handle = INVALID_HANDLE; //!!!strictly bounds D3D9 CB to the host Shader! may copy relevant metadata instead, if inacceptable
	Data::CFlags	DirtyFlags;
	bool			Temporary = false;

	void InternalDestroy();

public:

	CD3D9ConstantBuffer();
	virtual ~CD3D9ConstantBuffer();

	bool			Create(const CSM30BufferMeta& Meta, const CD3D9ConstantBuffer* pInitData);
	virtual void	Destroy() { InternalDestroy(); /*CConstantBuffer::Destroy();*/ }
	virtual bool	IsValid() const { return pFloat4Data || pInt4Data || pBoolData; }

	//!!!for IConst can compute universal offset! anyway need to set proper dirty flag (can deduce from offset btw)!
	void			WriteData(ESM30RegisterSet RegSet, UPTR Offset, const void* pData, UPTR Size);
	bool			IsDirty() const { return DirtyFlags.IsAny(); }
	bool			IsDirtyFloat4() const { return DirtyFlags.Is(CB9_DirtyFloat4); }
	bool			IsDirtyInt4() const { return DirtyFlags.Is(CB9_DirtyInt4); }
	bool			IsDirtyBool() const { return DirtyFlags.Is(CB9_DirtyBool); }
	bool			IsTemporary() const { return Temporary; }
	HConstBuffer	GetHandle() const { return Handle; }
	const float*	GetFloat4Data() const { return pFloat4Data; }
	const int*		GetInt4Data() const { return pInt4Data; }
	const BOOL*		GetBoolData() const { return pBoolData; }

	void			OnCommit() { DirtyFlags.ClearAll(); }	// For internal use by the GPUDriver
	void			SetTemporary(bool TmpBuffer) { Temporary = TmpBuffer; }	// For internal use by the GPUDriver
};

typedef Ptr<CD3D9ConstantBuffer> PD3D9ConstantBuffer;

inline void CD3D9ConstantBuffer::WriteData(ESM30RegisterSet RegSet, UPTR Offset, const void* pData, UPTR Size)
{
	n_assert_dbg(pData && Size);

	char* pDest;
	switch (RegSet)
	{
		case Reg_Float4:
			pDest = (char*)pFloat4Data + Offset * sizeof(float) * 4;
			DirtyFlags.Set(CB9_DirtyFloat4);
			break;
		case Reg_Int4:
			pDest = (char*)pInt4Data + Offset * sizeof(int) * 4;
			DirtyFlags.Set(CB9_DirtyInt4);
			break;
		case Reg_Bool:
			pDest = (char*)pBoolData + Offset * sizeof(BOOL);
			DirtyFlags.Set(CB9_DirtyBool);
			break;
		default:
			pDest = NULL;
			break;
	};

	n_assert_dbg(pDest);
	n_assert_dbg(pDest + Size <= (char*)pFloat4Data + Float4Count * sizeof(float) * 4 + Int4Count * sizeof(int) * 4 + BoolCount * sizeof(BOOL));

	memcpy(pDest, pData, Size);
}
//---------------------------------------------------------------------

}

#endif
