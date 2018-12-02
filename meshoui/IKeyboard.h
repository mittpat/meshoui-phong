#pragma once

namespace Meshoui
{
    class IKeyboard
    {
    public:
        virtual ~IKeyboard();
        IKeyboard();

        virtual void keyAction(void* window, int key, int scancode, int action, int mods);
        virtual void charAction(void* window, unsigned int c);
    };
    inline IKeyboard::IKeyboard() {}
    inline IKeyboard::~IKeyboard() {}
}
