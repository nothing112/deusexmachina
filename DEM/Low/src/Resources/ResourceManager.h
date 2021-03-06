#pragma once
#ifndef __DEM_L1_RESOURCE_MANAGER_H__
#define __DEM_L1_RESOURCE_MANAGER_H__

#include <Data/RefCounted.h>	// For Ptr<>
#include <Data/StringID.h>
#include <Data/HashTable.h>
#include <Data/Singleton.h>
//#include <System/Allocators/PoolAllocator.h>
#include <Core/RTTI.h>

// Resource manager controls resource loading, lifetime, uniquity, and serves as
// a hub for accessing any types of assets in an abstract way.
// Resources are identified and located by URI (Uniform Resource Identifier).
// Resource ID is its URI expanded and made relative to the RootPath, if possible.
// It is necessary for file resources for two reasons:
// 1. We avoid wasting memory for an absolute path to data root in each resource ID.
// 2. We make resource IDs independent from data root location, so resource IDs
//    will always stay the same no matter from which place we run our application.
// You always can leave RootPath empty and use absolute resource IDs.

namespace Resources
{
typedef Ptr<class CResource> PResource;
typedef Ptr<class CResourceObject> PResourceObject;
typedef Ptr<class CResourceLoader> PResourceLoader;
typedef Ptr<class CResourceGenerator> PResourceGenerator;

#define ResourceMgr Resources::CResourceManager::Instance()

class CResourceManager
{
	__DeclareSingleton(CResourceManager);

protected:

	struct CLoaderKey
	{
		CStrID				Extension;
		const Core::CRTTI*	pRsrcType;

		//!!!use IsDerivedFrom or hierarchy depth!
		bool operator ==(const CLoaderKey& Other) const { return Extension == Other.Extension && pRsrcType == Other.pRsrcType; }
		bool operator >(const CLoaderKey& Other) const { return Extension > Other.Extension || (Extension == Other.Extension && pRsrcType > Other.pRsrcType); }
		bool operator <(const CLoaderKey& Other) const { return Extension < Other.Extension || (Extension == Other.Extension && pRsrcType < Other.pRsrcType); }
	};

	struct CLoaderRec
	{
		PResourceLoader	Loader;
		bool			CloneOnCreate;	// Set to true for loaders with state which can change per-resource
	};

	//!!!???CResource pool?! if pool, hash table can store weak ptrs. How to deallocate on destroy?

	CString								RootPath;
	CHashTable<CStrID, PResource>		Registry;
	CHashTable<CLoaderKey, CLoaderRec>	DefaultLoaders;

public:

	CResourceManager(UPTR HashTableCapacity = 256);
	~CResourceManager();

	bool			RegisterDefaultLoader(const char* pFmtExtension, const Core::CRTTI* pRsrcType, CResourceLoader* pLoader, bool CloneOnCreate = false);
	PResourceLoader	CreateDefaultLoader(const char* pFmtExtension, const Core::CRTTI* pRsrcType = NULL);
	template<class TRsrc>
	PResourceLoader	CreateDefaultLoaderFor(const char* pFmtExtension) { return CreateDefaultLoader(pFmtExtension, &TRsrc::RTTI); }

	PResource		RegisterResource(CStrID URI); //!!!use URI structure, pre-parsed! one string w/ptrs to parts
	PResource		RegisterResource(const char* pURI); //!!!use URI structure, pre-parsed! one string w/ptrs to parts

	void			LoadResourceSync(CResource& Rsrc, CResourceLoader& Loader);
	void			LoadResourceAsync(CResource& Rsrc, CResourceLoader& Loader);
	void			GenerateResourceSync(CResource& Rsrc, CResourceGenerator& Generator);
};

}

#endif
