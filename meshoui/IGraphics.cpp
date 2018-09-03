#include "IGraphics.h"
#include "RendererPrivate.h"

void IGraphics::bind()
{
    d->bindGraphics(this);
}

void IGraphics::unbind()
{
    d->unbindGraphics(this);
}
