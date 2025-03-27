#pragma once

enum EViewModeIndex
{
    VMI_Lit,
    VMI_Unlit,
    VMI_Wireframe,
};


enum ELevelViewportType
{
    LVT_Perspective = 0,
    /** Top */
    LVT_OrthoXY = 1,
    /** Bottom */
    LVT_OrthoNegativeXY,
    /** Left */
    LVT_OrthoYZ,
    /** Right */
    LVT_OrthoNegativeYZ,
    /** Front */
    LVT_OrthoXZ,
    /** Back */
    LVT_OrthoNegativeXZ,
    LVT_MAX,
    LVT_None = 255,
};
