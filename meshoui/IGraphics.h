#pragma once

class RendererPrivate;
class IGraphics
{
public:
    virtual ~IGraphics() = 0;
    IGraphics();

    void bind();
    void unbind();

    RendererPrivate * d_ptr() const { return d; }

    bool bound;

private:
    friend class RendererPrivate;
    RendererPrivate * d;
};

inline IGraphics::~IGraphics() {}
inline IGraphics::IGraphics() : bound(false), d(nullptr) {}
