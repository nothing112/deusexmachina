#include "MocapClip.h"

#include <Animation/NodeControllerMocap.h>

namespace Anim
{
__ImplementResourceClass(Anim::CMocapClip, 'MCLP', Anim::CAnimClip);

bool CMocapClip::Setup(const nArray<CMocapTrack>& _Tracks, const nArray<CStrID>& TrackMapping,
					   const nArray<CEventTrack>* _EventTracks, vector4* _pKeys,
					   DWORD _KeysPerCurve, DWORD _KeyStride, float _KeyTime)
{
	n_assert(_pKeys);

	if (State == Resources::Rsrc_Loaded) Unload();

	pKeys = _pKeys;
	Tracks = _Tracks;
	if (_EventTracks) EventTracks = *_EventTracks;
	else EventTracks.Clear();

	KeysPerCurve = _KeysPerCurve;
	KeyStride = _KeyStride;
	InvKeyTime = 1.f / _KeyTime;
	Duration = (KeysPerCurve - 1) * _KeyTime;

	for (int i = 0; i < Tracks.GetCount(); ++i)
	{
		Tracks[i].pOwnerClip = this;
		CSampler& Sampler = Samplers.GetOrAdd(TrackMapping[i]);
		switch (Tracks[i].Channel)
		{
			case Scene::Chnl_Translation:	Sampler.pTrackT = &Tracks[i]; break;
			case Scene::Chnl_Rotation:		Sampler.pTrackR = &Tracks[i]; break;
			case Scene::Chnl_Scaling:		Sampler.pTrackS = &Tracks[i]; break;
			default: n_error("CMocapClip::Setup() -> Unsupported channel for an SRT sampler track!");
		};
	}

	State = Resources::Rsrc_Loaded;
	OK;
}
//---------------------------------------------------------------------

void CMocapClip::Unload()
{
	State = Resources::Rsrc_NotLoaded;

	//!!!sampler pointers will become invalid! use smartptrs?
	// or in controller store clip ptr and clear sampler if clip becomes unloaded

	Samplers.Clear();
	Tracks.Clear();
	SAFE_DELETE_ARRAY(pKeys);
}
//---------------------------------------------------------------------

Scene::PNodeController CMocapClip::CreateController(DWORD SamplerIdx) const
{
	Anim::PNodeControllerMocap Ctlr = n_new(Anim::CNodeControllerMocap);
	Ctlr->SetSampler(&Samplers.ValueAt(SamplerIdx));
	return Ctlr.GetUnsafe();
}
//---------------------------------------------------------------------

}
