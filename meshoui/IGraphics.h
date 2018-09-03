#pragma once

class GraphicsPrivate;
class IGraphics
{
public:
    virtual ~IGraphics() = 0;
    IGraphics();

    void bind();
    void unbind();

    GraphicsPrivate * d_ptr() const { return d; }

    bool bound;

private:
    friend class GraphicsPrivate;
    GraphicsPrivate * d;
};

inline IGraphics::~IGraphics() {}
inline IGraphics::IGraphics() : bound(false), d(nullptr) {}
