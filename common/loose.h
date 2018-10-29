#pragma once

#include <cstring>
#include <linalg.h>
#include <experimental/filesystem>

inline float degreesToRadians(float angle) { return angle * M_PI / 180.0; }
inline float radiansToDegrees(float rad) { return rad * 180.0 / M_PI; }

inline std::string sibling(const std::string & path, const std::string & other)
{
    std::experimental::filesystem::path parentpath(other);
    std::experimental::filesystem::path parentdirectory = parentpath.parent_path();
    return parentdirectory / path;
}

inline std::vector<std::string> split(const std::string & str, char sep, bool keepEmptyParts = false)
{
    std::vector<std::string> values;
    std::istringstream splitter(str);
    std::string s;
    while (std::getline(splitter, s, sep))
    {
        if (keepEmptyParts || !s.empty())
            values.push_back(s);
    }
    return values;
}

inline bool startsWith(const char *str, const char *match)
{
    bool ret = true;
    while (ret && *match != '\0')
    {
        ret = *str != '\0' && *str == *match;
        ++match;
        ++str;
    }
    return ret;
}

inline const char * remainder(const char *str, const char *match)
{
    bool ret = true;
    while (ret && *match != '\0')
    {
        ret = *str != '\0' && *str == *match;
        ++match;
        ++str;
    }
    return ret ? &str[strlen(match)] : nullptr;
}

struct AABB final
{
    AABB();
    void extend(linalg::aliases::float3 p);
    linalg::aliases::float3 center() const;
    linalg::aliases::float3 half() const;
    linalg::aliases::float3 lower;
    linalg::aliases::float3 upper;
};
inline AABB::AABB() : lower(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
                      upper(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()) {}
inline void AABB::extend(linalg::aliases::float3 p) { lower = linalg::min(lower, p); upper = linalg::max(upper, p); }
inline linalg::aliases::float3 AABB::center() const { return (lower + upper) * 0.5f; }
inline linalg::aliases::float3 AABB::half() const { return (upper - lower) * 0.5f; }

namespace conv
{
    inline float fastExp10(int n)
    {
        switch (n)
        {
        case -9: return 0.000000001f;
        case -8: return 0.00000001f;
        case -7: return 0.0000001f;
        case -6: return 0.000001f;
        case -5: return 0.00001f;
        case -4: return 0.0001f;
        case -3: return 0.001f;
        case -2: return 0.01f;
        case -1: return 0.1f;
        case  0: return 1.f;
        case  1: return 10.f;
        case  2: return 100.f;
        case  3: return 1000.f;
        case  4: return 10000.f;
        case  5: return 100000.f;
        case  6: return 1000000.f;
        case  7: return 10000000.f;
        case  8: return 100000000.f;
        case  9: return 1000000000.f;
        default:
            return pow(10.f, n);
        }
    }

    inline float stof(char *&p)
    {
        float r = 0.0f;
        bool neg = false;
        if (*p == '-')
        {
            neg = true;
            ++p;
        }
        while (*p >= '0' && *p <= '9')
        {
            r = (r * 10.0f) + (*p - '0');
            ++p;
        }
        if (*p == '.')
        {
            float f = 0.0f;
            int n = 0;
            ++p;
            while (*p >= '0' && *p <= '9')
            {
                f = (f * 10.0f) + (*p - '0');
                ++p;
                ++n;
            }
            r += f * fastExp10(-n);
        }
        if (*p == 'e')
        {
            ++p;
            bool negExp = false;
            if (*p == '-')
            {
                negExp = true;
                ++p;
            }
            int n = 0;
            while (*p >= '0' && *p <= '9')
            {
                n = (n * 10) + (*p - '0');
                ++p;
            }
            r *= fastExp10(negExp ? -n : n);
        }
        if (neg)
        {
            r = -r;
        }
        return r;
    }

    inline float stof(const char *p)
    {
        char *t = const_cast<char *>(p);
        return stof(t);
    }

    inline unsigned int stoui(char *&p)
    {
        unsigned int r = 0U;
        while (*p >= '0' && *p <= '9')
        {
            r = (r * 10U) + (*p - '0');
            ++p;
        }
        return r;
    }

    inline unsigned int stoui(const char *p)
    {
        char *t = const_cast<char *>(p);
        return stoui(t);
    }

    inline linalg::aliases::uint3 stoui3(char *&p, char = ' ')
    {
        linalg::aliases::uint3 ret = linalg::zero;
        ret.x = stoui(p); ++p;
        ret.y = stoui(p); ++p;
        ret.z = stoui(p);
        return ret;
    }

    inline linalg::aliases::uint3 stoui3(const char *p, char = ' ')
    {
        char *t = const_cast<char *>(p);
        return stoui3(t);
    }

    inline linalg::aliases::float3 stof3(char *&p, char = ' ')
    {
        linalg::aliases::float3 ret = linalg::zero;
        ret.x = stof(p); ++p;
        ret.y = stof(p); ++p;
        ret.z = stof(p);
        return ret;
    }

    inline linalg::aliases::float3 stof3(const char *p, char = ' ')
    {
        char *t = const_cast<char *>(p);
        return stof3(t);
    }

    inline linalg::aliases::float2 stof2(char *&p, char = ' ')
    {
        linalg::aliases::float2 ret = linalg::zero;
        ret.x = stof(p); ++p;
        ret.y = stof(p);
        return ret;
    }

    inline linalg::aliases::float2 stof2(const char *p, char = ' ')
    {
        char *t = const_cast<char *>(p);
        return stof2(t);
    }

    inline std::vector<float> stofa(const char *p, char = ' ')
    {
        char *t = const_cast<char *>(p);
        std::vector<float> ret;
        while (true)
        {
            if (*t != '\0')
                ret.push_back(stof(t));
            if (*t != '\0')
                ++t;
            else
                break;
        }
        return ret;
    }

    inline std::vector<unsigned int> stouia(const char *p, char = ' ')
    {
        char *t = const_cast<char *>(p);
        std::vector<unsigned int> ret;
        while (true)
        {
            ret.push_back(stoui(t));
            if (*t != '\0')
                ++t;
            else
                break;
        }
        return ret;
    }

    inline std::vector<linalg::aliases::uint3> stoui3a(const char *p, char = ' ', char sep2 = '/')
    {
        char *t = const_cast<char *>(p);
        std::vector<linalg::aliases::uint3> ret;
        while (true)
        {
            ret.push_back(stoui3(t, sep2));
            if (*t != '\0')
                ++t;
            else
                break;
        }
        return ret;
    }

    inline std::vector<linalg::aliases::float3> stof3a(const char *p, char = ' ', char sep2 = ' ')
    {
        char *t = const_cast<char *>(p);
        std::vector<linalg::aliases::float3> ret;
        while (true)
        {
            ret.push_back(stof3(t, sep2));
            if (*t != '\0')
                ++t;
            else
                break;
        }
        return ret;
    }

    inline std::vector<linalg::aliases::float2> stof2a(const char *p, char = ' ', char sep2 = ' ')
    {
        char *t = const_cast<char *>(p);
        std::vector<linalg::aliases::float2> ret;
        while (true)
        {
            ret.push_back(stof2(t, sep2));
            if (*t != '\0')
                ++t;
            else
                break;
        }
        return ret;
    }
}
