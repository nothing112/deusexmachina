#pragma once
#ifndef __DEM_L1_RENDER_SKIN_INFO_H__
#define __DEM_L1_RENDER_SKIN_INFO_H__

#include <Resources/ResourceObject.h>
#include <Data/FixedArray.h>
#include <Math/Matrix44.h>

// Shared bind pose data for skinning. Maps inverse skeleton-root-related bind
// pose transforms to skeleton bones.

namespace Render
{

struct CBoneInfo
{
	CStrID	ID;
	UPTR	ParentIndex;
};

class CSkinInfo: public Resources::CResourceObject
{
	__DeclareClass(CSkinInfo);

protected:

	matrix44*				pInvBindPose;
	CFixedArray<CBoneInfo>	Bones;
	//???root and terminal node indices?

public:

	CSkinInfo(): pInvBindPose(NULL) {}
	virtual ~CSkinInfo() { Destroy(); }

	bool				Create(UPTR BoneCount);
	void				Destroy();

	virtual bool		IsResourceValid() const { return !!pInvBindPose; }

	matrix44*			GetInvBindPoseData() { return pInvBindPose; }
	const matrix44&		GetInvBindPose(UPTR BoneIndex) const { return pInvBindPose[BoneIndex]; }
	CBoneInfo&			GetBoneInfoEditable(UPTR BoneIndex) { return Bones[BoneIndex]; }
	const CBoneInfo&	GetBoneInfo(UPTR BoneIndex) const { return Bones[BoneIndex]; }
	UPTR				GetBoneCount() const { return Bones.GetCount(); }
};

typedef Ptr<CSkinInfo> PSkinInfo;

}

#endif
