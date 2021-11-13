#include "core.h"

Core* core;

extern "C" __declspec(dllexport) void init()
{
    core = new Core();
}

extern "C" __declspec(dllexport) void cleanup()
{
    delete core;
}

// VideoPort functions

extern "C" __declspec(dllexport) const int getVideoPortCount()
{
    return core->getVideoPortCount();
}

extern "C" __declspec(dllexport) const int activatePlayout(const int port)
{
    return core->activatePlayout(port);
}

extern "C" __declspec(dllexport) const int deactivatePlayout(const int port)
{
    return core->deactivatePlayout(port);
}

// Player functions

extern "C" __declspec(dllexport) const int64_t loadPortItem(const int port, const char* path, const bool canTake)
{
    return core->loadPortItem(port, path, canTake);
}

extern "C" __declspec(dllexport) const int changeFormat(const int port, const char* format)
{
    return core->changeFormat(port, format);
}

extern "C" __declspec(dllexport) const int64_t getCurrentPlayTime(const int port)
{
    return core->getCurrentPlayTime(port);
}

extern "C" __declspec(dllexport) const int64_t getCurrentTimeCode(const int port)
{
    return core->getCurrentTimeCode(port);
}

extern "C" __declspec(dllexport) const int recue(const int port, const bool loop)
{
    return core->recue(port, loop);
}

extern "C" __declspec(dllexport) const int take(const int port)
{
    return core->take(port);
}

extern "C" __declspec(dllexport) const int pause(const int port)
{
    return core->pause(port);
}

extern "C" __declspec(dllexport) const int dropMedia(const int port, const bool immediate)
{
    return core->dropMedia(port, immediate);
}

extern "C" __declspec(dllexport) const int seek(const int port, const int64_t pos)
{
    return core->seek(port, pos);
}

extern "C" __declspec(dllexport) const int nextFrame(const int port)
{
    return core->nextFrame(port);
}

extern "C" __declspec(dllexport) const int previousFrame(const int port)
{
    return core->previousFrame(port);
}

extern "C" __declspec(dllexport) const double forward(const int port)
{
    return core->forward(port);
}

extern "C" __declspec(dllexport) const double reverse(const int port)
{
    return core->reverse(port);
}
