#pragma once

#include <hashid.h>

namespace Meshoui
{
    class Widget
    {
    public:
        virtual ~Widget();
        Widget();

        virtual void draw() = 0;
        HashId window;
    };
    inline Widget::~Widget() {}
    inline Widget::Widget() : window("Main window") {}
}
