#include "CollisionShapeLoader.h"

#include <Physics/HeightfieldShape.h>
#include <Physics/BulletConv.h>
#include <Resources/Resource.h>
#include <IO/IOServer.h>
#include <IO/Streams/FileStream.h>
#include <IO/BinaryReader.h>
#include <IO/PathUtils.h>
#include <Math/AABB.h>
#include <Data/Buffer.h>
#include <Data/HRDParser.h>
#include <Core/Factory.h>

#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

//!!!NEED .bullet FILE LOADER!

#define DEFINE_STRID(Str) static const CStrID Str(#Str);

namespace Str
{
	DEFINE_STRID(Type)
	DEFINE_STRID(Sphere)
	DEFINE_STRID(Radius)
	DEFINE_STRID(Box)
	DEFINE_STRID(Size)
	DEFINE_STRID(Capsule)
	DEFINE_STRID(Height)
	DEFINE_STRID(StaticMesh)
	DEFINE_STRID(Heightfield)
	DEFINE_STRID(CDLODFile)
}

namespace Resources
{
__ImplementClass(Resources::CCollisionShapeLoader, 'CSLD', Resources::CResourceLoader);

const Core::CRTTI& CCollisionShapeLoader::GetResultType() const
{
	return Physics::CCollisionShape::RTTI;
}
//---------------------------------------------------------------------

PResourceObject CCollisionShapeLoader::Load(IO::CStream& Stream)
{
	void* pData = NULL;
	if (Stream.CanBeMapped()) pData = Stream.Map();
	bool Mapped = !!pData;
	if (!Mapped)
	{
		UPTR DataSize = Stream.GetSize();
		pData = n_malloc(DataSize);
		if (Stream.Read(pData, DataSize) != DataSize) return NULL;
	}

	//!!!two loaders or dynamic format detection! may use FOURCC 'PRMx' where 'x' is some non-printable character!
	const char* pURI = Resource.GetUID().CStr();
	const char* pExt = PathUtils::GetExtension(pURI);
	Data::PParams Desc;
	if (!n_stricmp(pExt, "hrd"))
	{
		Data::CBuffer Buffer;
		if (!IOSrv->LoadFileToBuffer(pURI, Buffer)) FAIL;
		Data::CHRDParser Parser;
		if (!Parser.ParseBuffer((const char*)Buffer.GetPtr(), Buffer.GetSize(), Desc)) FAIL;
	}
	else if (!n_stricmp(pExt, "prm"))
	{
		IO::PStream File = IOSrv->CreateStream(pURI);
		if (!File->Open(IO::SAM_READ, IO::SAP_SEQUENTIAL)) FAIL;
		IO::CBinaryReader Reader(*File);
		Desc = n_new(Data::CParams);
		if (!Reader.ReadParams(*Desc)) FAIL;
	}
	else FAIL;

	if (Mapped) Stream.Unmap();
	else n_free(pData);

	CStrID Type = Desc->Get<CStrID>(Str::Type);
	if (Type == Str::StaticMesh)
	{
		//Shape = 
		//create btStridingMeshInterface*
		//???can get interface pointer from the shape?
		NOT_IMPLEMENTED;
		FAIL;
	}
	else if (Type == Str::Heightfield)
	{
		CString FileName = Desc->Get<CString>(Str::CDLODFile, CString::Empty);
		if (!FileName.IsValid()) FAIL;
		FileName = "Terrain:" + FileName + ".cdlod";

		void* pHFData = NULL;
		U32 HFWidth, HFHeight;
		float HScale;
		CAABB AABB;

		if (PathUtils::CheckExtension(FileName, "cdlod"))
		{
			//!!!DUPLICATE CODE! See Scene::CTerrain!
			IO::PStream CDLODFile = IOSrv->CreateStream(FileName);
			if (!CDLODFile->Open(IO::SAM_READ, IO::SAP_SEQUENTIAL)) FAIL;
			IO::CBinaryReader Reader(*CDLODFile);

			n_assert(Reader.Read<U32>() == 'CDLD');	// Magic
			n_assert(Reader.Read<U32>() == 1);		// Version

			Reader.Read(HFWidth);
			Reader.Read(HFHeight);
			Reader.Read<U32>(); // PatchSize
			Reader.Read<U32>(); // LODCount
			Reader.Read<U32>(); // MinMaxDataSize
			Reader.Read(HScale);
			Reader.Read(AABB.Min.x);
			Reader.Read(AABB.Min.y);
			Reader.Read(AABB.Min.z);
			Reader.Read(AABB.Max.x);
			Reader.Read(AABB.Max.y);
			Reader.Read(AABB.Max.z);

			UPTR DataSize = HFWidth * HFHeight * sizeof(U16);
			pHFData = n_malloc(DataSize);
			CDLODFile->Read(pHFData, DataSize);

			// Convert to signed
			const unsigned short* pUData = ((unsigned short*)pHFData);
			const unsigned short* pUDataEnd = ((unsigned short*)pHFData) + HFWidth * HFHeight;
			short* pSData = ((short*)pHFData);
			while (pUData < pUDataEnd)
			{
				*pSData = *pUData - 32767;
				++pUData;
				++pSData;
			}
		}
		else FAIL;

		Physics::PHeightfieldShape HFShape = n_new(Physics::CHeightfieldShape);
		if (HFShape.IsValidPtr())
		{
			btHeightfieldTerrainShape* pBtShape =
				new btHeightfieldTerrainShape(HFWidth, HFHeight, pHFData, HScale, AABB.Min.y, AABB.Max.y, 1, PHY_SHORT, false);

			btVector3 LocalScaling((AABB.Max.x - AABB.Min.x) / (float)(HFWidth - 1), 1.f, (AABB.Max.z - AABB.Min.z) / (float)(HFHeight - 1));
			pBtShape->setLocalScaling(LocalScaling);

			vector3 Offset((AABB.Max.x - AABB.Min.x) * 0.5f, (AABB.Min.y + AABB.Max.y) * 0.5f, (AABB.Max.z - AABB.Min.z) * 0.5f);
			if (HFShape->Setup(pBtShape, pHFData, Offset))
			{
				Resource.Init(HFShape.GetUnsafe(), this);
				OK;
			}

			delete pBtShape;
		}

		n_free(pHFData);
	}
	else
	{
		btCollisionShape* pBtShape;

		if (Type == Str::Sphere)
		{
			pBtShape = new btSphereShape(Desc->Get<float>(Str::Radius, 1.f));
		}
		else if (Type == Str::Box)
		{
			vector3 Ext = Desc->Get(Str::Size, vector3(1.f, 1.f, 1.f));
			Ext *= 0.5f;
			pBtShape = new btBoxShape(VectorToBtVector(Ext));
		}
		else if (Type == Str::Capsule)
		{
			pBtShape = new btCapsuleShape(Desc->Get<float>(Str::Radius, 1.f), Desc->Get<float>(Str::Height, 1.f));
		}
		else FAIL;

		Physics::PCollisionShape Shape = n_new(Physics::CCollisionShape);
		if (Shape.IsValidPtr() && Shape->Setup(pBtShape))
		{
			Resource.Init(Shape.GetUnsafe(), this);
			OK;
		}

		delete pBtShape;
	}

	FAIL;
}
//---------------------------------------------------------------------

}