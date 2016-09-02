#pragma once
#ifndef __DEM_L1_UI_H__
#define __DEM_L1_UI_H__

// UI system declarations and helpers

namespace UI
{

enum EDrawMode
{
	DrawMode_Opaque			= 0x01,
	DrawMode_Transparent	= 0x02,
	DrawMode_All			= (DrawMode_Opaque | DrawMode_Transparent)
};

}

#endif