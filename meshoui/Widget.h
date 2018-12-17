#pragma once

#include <string>

namespace Meshoui
{
    class Widget
    {
    public:
        virtual ~Widget();
        Widget();

        virtual void draw() = 0;
        std::string window;
    };
    inline Widget::~Widget() {}
    inline Widget::Widget() : window("Main window") {}
}
