#pragma once

class IWhatever
{
public:
    virtual ~IWhatever() = 0;
    IWhatever();
};

inline IWhatever::~IWhatever() {}
inline IWhatever::IWhatever() {}