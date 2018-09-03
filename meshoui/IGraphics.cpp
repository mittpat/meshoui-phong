#include "IGraphics.h"
#include "GraphicsPrivate.h"

void IGraphics::bind()
{
    d->bindGraphics(this);
}

void IGraphics::unbind()
{
    d->unbindGraphics(this);
}
