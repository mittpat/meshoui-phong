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

/// RAY
struct MoRay
{
    vec3 origin;
    vec3 direction;
    vec3 oneOverDirection;
};

void moInitRay(inout MoRay self, in vec3 origin, in vec3 direction)
{
    self.origin = origin;
    self.direction = direction;
    self.oneOverDirection = 1.0 / self.direction;
}

/// BBOX
struct MoBBox
{
    vec3 min;
    vec3 max;
};

bool moIntersect(in MoBBox self, in MoRay ray, out float t_near, out float t_far)
{
    vec3 t1 = (self.min - ray.origin) * ray.oneOverDirection;
    vec3 t2 = (self.max - ray.origin) * ray.oneOverDirection;

    t_near = max(max(min(t1.x, t2.x), min(t1.y, t2.y)), min(t1.z, t2.z));
    t_far = min(min(max(t1.x, t2.x), max(t1.y, t2.y)), max(t1.z, t2.z));

    return t_far >= t_near;
}

/// TRIANGLE
struct MoTriangle
{
    vec3 v0, v1, v2;
    vec2 uv0, uv1, uv2;
    vec3 n0, n1, n2;
};

vec3 moGetUVBarycentric(in MoTriangle self, in vec2 uv)
{
    vec4 x = vec4(uv.x, self.uv0.x, self.uv1.x, self.uv2.x);
    vec4 y = vec4(uv.y, self.uv0.y, self.uv1.y, self.uv2.y);

    float d  = fma(y[2] - y[3], x[1] - x[3], (x[3] - x[2]) * (y[1] - y[3]));
    float l1 = fma(y[2] - y[3], x[0] - x[3], (x[3] - x[2]) * (y[0] - y[3])) / d;
    float l2 = fma(y[3] - y[1], x[0] - x[3], (x[1] - x[3]) * (y[0] - y[3])) / d;
    float l3 = 1 - l1 - l2;

    return vec3(l1, l2, l3);
}

void moGetSurface(in MoTriangle self, in vec3 barycentricCoordinates, out vec3 point, out vec3 normal)
{
    point = mat3(self.v0, self.v1, self.v2) * barycentricCoordinates;
    normal = mat3(self.n0, self.n1, self.n2) * barycentricCoordinates;
}

bool moRayTriangleIntersect(in MoTriangle self, in MoRay ray, out float t, out float u, out float v)
{
    float EPSILON = 0.0000001f;
    vec3 edge1 = self.v1 - self.v0;
    vec3 edge2 = self.v2 - self.v0;
    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);
    if (a > -EPSILON && a < EPSILON)
    {
        // This ray is parallel to this triangle.
        return false;
    }

    float f = 1.0/a;
    vec3 s = ray.origin - self.v0;
    u = f * dot(s, h);
    if (u < 0.0 || u > 1.0)
    {
        return false;
    }

    vec3 q = cross(s, edge1);
    v = f * dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0)
    {
        return false;
    }

    // At this stage we can compute t to find out where the intersection point is on the line.
    t = f * dot(edge2, q);
    if (t > EPSILON)
    {
        // ray intersection
        return true;
    }

    // This means that there is a line intersection but not a ray intersection.
    return false;
}

/// BVH
struct MoBVHSplitNode
{
    MoBBox boundingBox;
    uint start;
    uint offset;
};

struct MoBVHWorkingSet
{
    uint index;
    float distance;
};

struct MoIntersectResult
{
    MoTriangle triangle;
    vec3 barycentric;
    float distance;
};

void swap(inout float left, inout float right)
{
    float temp = left;
    left = right;
    right = temp;
}

void swap(inout uint left, inout uint right)
{
    uint temp = left;
    left = right;
    right = temp;
}

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

bool moTexcoordInTriangleUV(in MoTriangle self, in vec2 tex)
{
    float s = self.uv0.y * self.uv2.x - self.uv0.x * self.uv2.y + (self.uv2.y - self.uv0.y) * tex.x + (self.uv0.x - self.uv2.x) * tex.y;
    float t = self.uv0.x * self.uv1.y - self.uv0.y * self.uv1.x + (self.uv0.y - self.uv1.y) * tex.x + (self.uv1.x - self.uv0.x) * tex.y;

    if ((s < 0) != (t < 0))
        return false;

    float area = -self.uv1.y * self.uv2.x + self.uv0.y * (self.uv2.x - self.uv1.x) + self.uv0.x * (self.uv1.y - self.uv2.y) + self.uv1.x * self.uv2.y;

    return area < 0 ?
            (s <= 0 && s + t >= area) :
            (s >= 0 && s + t <= area);
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

const float PHI = 1.61803398874989484820459 * 00000.1; // Golden Ratio
const float PI  = 3.14159265358979323846264 * 00000.1; // PI
const float SQ2 = 1.41421356237309504880169 * 10000.0; // Square Root of Two

float goldNoise(in vec2 coordinate, in float seed)
{
    return fract(tan(distance(coordinate * (seed + PHI), vec2(PHI, PI))) * SQ2);
}

vec3 moNextSphericalSample(in vec2 coordinate, in float seed, bool direction)
{
    float length2;
    vec3 vect;
    do
    {
        vect.x = fma(goldNoise(coordinate, seed++), 2.0, -1.0);
        vect.y = fma(goldNoise(coordinate, seed++), 2.0, -1.0);
        vect.z = fma(goldNoise(coordinate, seed++), 2.0, -1.0);
        length2 = dot(vect, vect);
    }
    while (length2 > 1.f);
    if (direction)
    {
        vect /= sqrt(length2);
    }
    return vect;
}

const vec4 DefaultColor = vec4(1.0, 0.0, 1.0, 1.0);
const vec3 SurfaceBias = vec3(0.01f);
const int SampleCount = 256;

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
            vec3 nextDirection = moNextSphericalSample(/*origin.xy*/vec2(0.5), 3.0 * j, true);
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
        fragment = vec4(vec3(value / SampleCount), 1.0);
#endif
    }
    else
    {
        fragment = DefaultColor;
    }
}
#endif
