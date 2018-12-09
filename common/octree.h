#pragma once

#include <vector>

template<typename T>
class Octree
{
public:
    Octree(const float3 & origin, const float3 & halfDimension)
        : origin(origin)
        , halfDimension(halfDimension)
        , data()
    {
        for (int i = 0; i < 8; ++i)
            children[i] = nullptr;
    }

    Octree(const Octree & copy)
        : origin(copy.origin)
        , halfDimension(copy.halfDimension)
        , data(copy.data)
    {}

    ~Octree()
    {
        for (int i = 0; i < 8; ++i)
            delete children[i];
    }

    int getOctantContainingPoint(const float3 & point) const
    {
        int oct = 0;
        if (point.x >= origin.x) oct |= 4;
        if (point.y >= origin.y) oct |= 2;
        if (point.z >= origin.z) oct |= 1;
        return oct;
    }

    bool isLeafNode() const
    {
        return children[0] == nullptr;
    }

    void insert(const T & point)
    {
        if (isLeafNode())
        {
            if (data.empty() || point.position() == data.front().position())
            {
                data.push_back(point);
                return;
            }
            else
            {
                float3 nextHalfDimension = halfDimension*.5f;
                if (nextHalfDimension.x == 0.f || nextHalfDimension.y == 0.f || nextHalfDimension.z == 0.f)
                {
                    data.push_back(point);
                }
                else
                {
                    for (int i = 0; i < 8; ++i)
                    {
                        float3 newOrigin = origin;
                        newOrigin.x += halfDimension.x * (i&4 ? .5f : -.5f);
                        newOrigin.y += halfDimension.y * (i&2 ? .5f : -.5f);
                        newOrigin.z += halfDimension.z * (i&1 ? .5f : -.5f);
                        children[i] = new Octree(newOrigin, nextHalfDimension);
                    }
                    for (const auto & oldPoint : data)
                    {
                        children[getOctantContainingPoint(oldPoint.position())]->insert(oldPoint);
                    }
                    data.clear();
                    children[getOctantContainingPoint(point.position())]->insert(point);
                }
            }
        }
        else
        {
            int octant = getOctantContainingPoint(point.position());
            children[octant]->insert(point);
        }
    }

    void getPointsInsideBox(const float3 & bmin, const float3 & bmax, std::vector<T> & results)
    {
        if (isLeafNode())
        {
            if (!data.empty())
            {
                for (const auto & point : data)
                {
                    const float3 p = point.position();
                    if (p.x > bmax.x || p.y > bmax.y || p.z > bmax.z) return;
                    if (p.x < bmin.x || p.y < bmin.y || p.z < bmin.z) return;
                    results.push_back(point);
                }
            }
        }
        else
        {
            for (int i = 0; i < 8; ++i)
            {
                const float3 cmax = children[i]->origin + children[i]->halfDimension;
                const float3 cmin = children[i]->origin - children[i]->halfDimension;
                if (cmax.x < bmin.x || cmax.y < bmin.y || cmax.z < bmin.z) continue;
                if (cmin.x > bmax.x || cmin.y > bmax.y || cmin.z > bmax.z) continue;
                children[i]->getPointsInsideBox(bmin, bmax, results);
            }
        }
    }

private:
    float3 origin;
    float3 halfDimension;
    Octree * children[8];
    std::vector<T> data;
};
