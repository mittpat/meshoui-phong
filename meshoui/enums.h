#pragma once

struct View
{
    enum Flag { None = 0x0, Rotation = 0x01, Translation = 0x02, Scaling = 0x04, All = 0xFF };
    typedef int Flags;
};

struct Render
{
    enum Flag { None = 0x0, DepthTest = 0x01, Filtering = 0x02, Anisotropic = 0x04, Mipmap = 0x08, Blend = 0x10, BackFaceCulling = 0x20, DepthWrite = 0x40, Points = 0x80, Visible = 0x100, Collision = 0x200,
                Default = DepthTest | Filtering | Anisotropic | Mipmap | BackFaceCulling | DepthWrite | Visible, All = 0xFFFF };
    typedef int Flags;
};
