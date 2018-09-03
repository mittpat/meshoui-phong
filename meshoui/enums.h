#pragma once

struct View
{
    enum Flag { None = 0x0, Rotation = 0x01, Vertical = 0x02, Horizontal = 0x04, Translation = 0x06, Scaling = 0x08, All = 0xFF };
    typedef int Flags;
};

struct Render
{
    enum Flag { None = 0x0, DepthTest = 0x01, Filtering = 0x02, Anisotropic = 0x04, Mipmap = 0x08, Blend = 0x10, BackFaceCulling = 0x20, DepthWrite = 0x40, Default = DepthTest | Filtering | Anisotropic | Mipmap | BackFaceCulling | DepthWrite, All = 0xFF };
    typedef int Flags;
};

struct Module
{
    enum Priority { Beginning = 10, Middle = 100, End = 1000, Default = Middle };
};
