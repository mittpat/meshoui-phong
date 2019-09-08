#version 450 core

#ifdef COMPILING_VERTEX
layout(location=0) out VertexData
{
    vec3 vertex;
    vec3 normal;
    vec2 texcoord;
    mat3 TBN;
} outData;
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexcoord;
layout(location = 2) in vec3 vertexNormal;
layout(location = 3) in vec3 vertexTangent;
layout(location = 4) in vec3 vertexBitangent;
layout(push_constant) uniform uPushConstant
{
    mat4 uniformModel;
    mat4 uniformView;
    mat4 uniformProjection;
} pc;

void main()
{
    outData.vertex = vec3(pc.uniformModel * vec4(vertexPosition, 1.0));
    outData.normal = normalize(mat3(transpose(inverse(pc.uniformModel))) * vertexNormal);
    outData.texcoord = vertexTexcoord;
    vec3 T = normalize(vec3(mat3(pc.uniformModel) * vertexTangent));
    vec3 B = normalize(vec3(mat3(pc.uniformModel) * vertexBitangent));
    vec3 N = normalize(vec3(mat3(pc.uniformModel) * vertexNormal));
    outData.TBN = mat3(T, B, N);
    gl_Position = pc.uniformProjection * pc.uniformView * pc.uniformModel * vec4(vertexPosition, 1.0);
}
#endif

#ifdef COMPILING_FRAGMENT

#extension GL_GOOGLE_include_directive : enable
#include "icosphere.h"
#include "raytrace.h"

layout(std430, set = 2, binding = 0) buffer BVHObjects
{
    MoTriangle pObjects[];
} inBVHObjects;
layout(std430, set = 2, binding = 1) buffer BVHSplitNodes
{
    MoBVHSplitNode pSplitNodes[];
} inBVHSplitNodes;
layout(std430, set = 2, binding = 2) buffer BVHSplitNodesUV
{
    MoBVHSplitNode pSplitNodes[];
} inBVHSplitNodesUV;


// Working set
MoBVHWorkingSet traversal[64];

bool moIntersectTriangleBVH(in MoRay ray, out MoIntersectResult result)
{
    result.distance = 1.0 / 0.0;
    int stackPtr = 0;

    traversal[stackPtr].index = 0;
    traversal[stackPtr].distance = -1.0 / 0.0;

    while (stackPtr >= 0)
    {
        uint index = traversal[stackPtr].index;
        float near = traversal[stackPtr].distance;
        stackPtr--;
        MoBVHSplitNode node = inBVHSplitNodes.pSplitNodes[index];

        if (near > result.distance)
        {
            continue;
        }

        if (node.offset == 0)
        {
            float t = 1.0 / 0.0;
            float u, v;
            MoTriangle triangle = inBVHObjects.pObjects[node.start];
            if (moRayTriangleIntersect(triangle, ray, t, u, v))
            {                
                if (t < result.distance)
                {
                    result.triangle = triangle;
                    result.barycentric = vec3(1.f - u - v, u, v);
                    result.distance = t;
                }
                if (t <= 0.0)
                {
                    return true;
                }
            }
        }
        else
        {
            uint closer = index + 1;
            uint other = index + node.offset;

            float bbhits[5];

            bool hitLeft = moIntersect(inBVHSplitNodes.pSplitNodes[index + 1].boundingBox, ray, bbhits[0], bbhits[1]);
            bool hitRight = moIntersect(inBVHSplitNodes.pSplitNodes[index + node.offset].boundingBox, ray, bbhits[2], bbhits[3]);

            if (hitLeft && hitRight)
            {
                if (bbhits[2] < bbhits[0])
                {
                    other = index + 1;
                    closer = index + node.offset;
                    // swap
                    bbhits[4] = bbhits[0];
                    bbhits[0] = bbhits[2];
                    bbhits[2] = bbhits[4];
                    // swap
                    bbhits[4] = bbhits[1];
                    bbhits[1] = bbhits[3];
                    bbhits[3] = bbhits[4];
                }

                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(other, bbhits[2]);
                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(closer, bbhits[0]);
            }
            else if (hitLeft)
            {
                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(closer, bbhits[0]);
            }
            else if (hitRight)
            {
                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(other, bbhits[2]);
            }
        }
    }

    return result.distance < (1.0 / 0.0);
}

bool moIntersectUVTriangleBVH(in MoRay ray, out MoIntersectResult result)
{
    result.distance = 1.0 / 0.0;
    float bbhits[5];

    int stackPtr = 0;

    traversal[stackPtr].index = 0;
    traversal[stackPtr].distance = -1.0 / 0.0;

    while (stackPtr >= 0)
    {
        uint index = traversal[stackPtr].index;
        float near = traversal[stackPtr].distance;
        stackPtr--;
        MoBVHSplitNode node = inBVHSplitNodesUV.pSplitNodes[index];

        if (near > result.distance)
        {
            continue;
        }

        if (node.offset == 0)
        {
            float t = 1.0 / 0.0;
            MoTriangle triangle = inBVHObjects.pObjects[node.start];
            if (moTexcoordInTriangleUV(triangle, ray.origin.xy))
            {
                t = 0.0;
            }
            if (t < result.distance)
            {
                result.triangle = triangle;
                result.distance = t;
            }
            if (t <= 0.0)
            {
                return true;
            }
        }
        else
        {
            uint closer = index + 1;
            uint other = index + node.offset;

            float bbhits[5];

            bool hitLeft = moIntersect(inBVHSplitNodesUV.pSplitNodes[index + 1].boundingBox, ray, bbhits[0], bbhits[1]);
            bool hitRight = moIntersect(inBVHSplitNodesUV.pSplitNodes[index + node.offset].boundingBox, ray, bbhits[2], bbhits[3]);

            if (hitLeft && hitRight)
            {
                if (bbhits[2] < bbhits[0])
                {
                    other = index + 1;
                    closer = index + node.offset;
                    // swap
                    bbhits[4] = bbhits[0];
                    bbhits[0] = bbhits[2];
                    bbhits[2] = bbhits[4];
                    // swap
                    bbhits[4] = bbhits[1];
                    bbhits[1] = bbhits[3];
                    bbhits[3] = bbhits[4];
                }

                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(other, bbhits[2]);
                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(closer, bbhits[0]);
            }
            else if (hitLeft)
            {
                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(closer, bbhits[0]);
            }
            else if (hitRight)
            {
                ++stackPtr;
                traversal[stackPtr] = MoBVHWorkingSet(other, bbhits[2]);
            }
        }
    }

    return result.distance < (1.0 / 0.0);
}

/// ORIGINAL PHONG
layout(location = 0) out vec4 fragment;
layout(location = 0) in VertexData
{
    vec3 vertex;
    vec3 normal;
    vec2 texcoord;
    mat3 TBN;
} inData;

const vec4 DefaultColor = vec4(1.0, 0.0, 1.0, 1.0);
const vec3 SurfaceBias = vec3(0.01);
const int SampleCount = 64*10+2;

//#define MO_NOISE
#ifdef MO_NOISE
const float PHI = 1.61803398874989484820459 * 00000.1; // Golden Ratio
const float PI  = 3.14159265358979323846264 * 00000.1; // PI
const float SQ2 = 1.41421356237309504880169 * 10000.0; // Square Root of Two

float goldNoise(in vec2 coordinate, in float seed)
{
    return fract(tan(distance(coordinate * (seed + PHI), vec2(PHI, PI))) * SQ2);
}
#endif

void main()
{
    vec3 origin = vec3(vec2(1.0, 1.0) - inData.texcoord, -1.0);

    MoRay ray;
    moInitRay(ray, origin, vec3(0.0, 0.0, 1.0));

    MoIntersectResult result;
    if (moIntersectUVTriangleBVH(ray, result))
    {
        vec3 surfacePoint, surfaceNormal;
        moGetSurface(result.triangle, moGetUVBarycentric(result.triangle, origin.xy), surfacePoint, surfaceNormal);
//#define COMPUTE_NORMALS
#ifdef COMPUTE_NORMALS
        fragment = vec4(fma(surfaceNormal, 0.5, vec3(0.5)), 1.0);
#else
        float value = 0.0;
        for (int j = 0; j < SampleCount; ++j)
        {
            vec3 nextDirection = MoIcosphereData25T[j];
            float diffuseFactor = dot(surfaceNormal, nextDirection);
            if (diffuseFactor > 0.f)
            {
                moInitRay(ray, fma(surfaceNormal, SurfaceBias, surfacePoint), nextDirection);

                MoIntersectResult nextResult;
                if (moIntersectTriangleBVH(ray, nextResult) && nextResult.distance < 1.0 /*lightSourceDistance*/)
                {
                    // light is occluded
                }
                else
                {
                    value += diffuseFactor * 1.0 * 2.0 * 2.0 /*white point*/ /*lightSourcePower*/;
                }
            }
        }
        fragment = vec4(vec3(value / SampleCount
#ifdef MO_NOISE
                             + goldNoise(origin.xy/0.01, 0.25) * 0.2
#endif
                        ), 1.0);
#endif
    }
    else
    {
        fragment = DefaultColor;
    }
}
#endif
