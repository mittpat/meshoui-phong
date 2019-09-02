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
    vec3 extent;
};

void moInitBBox(inout MoBBox self, in vec3 _min, in vec3 _max)
{
    self.min = _min;
    self.max = _max;
    self.extent = self.max - self.min;
}

void moInitBBox(inout MoBBox self, in vec3 point)
{
    self.min = point;
    self.max = point;
    self.extent = self.max - self.min;
}

uint moLongestSide(in MoBBox self)
{
    uint dimension = 0;
    if (self.extent.y > self.extent.x)
    {
        dimension = 1;
        if (self.extent.z > self.extent.y)
        {
            dimension = 2;
        }
    }
    else if (self.extent.z > self.extent.x)
    {
        dimension = 2;
    }
    return dimension;
}

bool moIntersect(in MoBBox self, in MoRay ray, out float t_near, out float t_far)
{
    float tx1 = (self.min.x - ray.origin.x) * ray.oneOverDirection.x;
    float tx2 = (self.max.x - ray.origin.x) * ray.oneOverDirection.x;

    t_near = min(tx1, tx2);
    t_far = max(tx1, tx2);

    float ty1 = (self.min.y - ray.origin.y) * ray.oneOverDirection.y;
    float ty2 = (self.max.y - ray.origin.y) * ray.oneOverDirection.y;

    t_near = max(t_near, min(ty1, ty2));
    t_far = min(t_far, max(ty1, ty2));

    float tz1 = (self.min.z - ray.origin.z) * ray.oneOverDirection.z;
    float tz2 = (self.max.z - ray.origin.z) * ray.oneOverDirection.z;

    t_near = max(t_near, min(tz1, tz2));
    t_far = min(t_far, max(tz1, tz2));

    return t_far >= t_near;
}

void moExpandToInclude(inout MoBBox self, in vec3 point)
{
    self.min.x = min(self.min.x, point.x);
    self.min.y = min(self.min.y, point.y);
    self.min.z = min(self.min.z, point.z);
    self.max.x = max(self.max.x, point.x);
    self.max.y = max(self.max.y, point.y);
    self.max.z = max(self.max.z, point.z);
    self.extent = self.max - self.min;
}

void moExpandToInclude(inout MoBBox self, in MoBBox box)
{
    self.min.x = min(self.min.x, box.min.x);
    self.min.y = min(self.min.y, box.min.y);
    self.min.z = min(self.min.z, box.min.z);
    self.max.x = max(self.max.x, box.max.x);
    self.max.y = max(self.max.y, box.max.y);
    self.max.z = max(self.max.z, box.max.z);
    self.extent = self.max - self.min;
}

/// TRIANGLE
struct MoTriangle
{
    vec3 v0, v1, v2;
    vec2 uv0, uv1, uv2;
    vec3 n0, n1, n2;
};

MoBBox moGetBoundingBox(in MoTriangle self)
{
    MoBBox bb;
    moInitBBox(bb, self.v0);
    moExpandToInclude(bb, self.v1);
    moExpandToInclude(bb, self.v2);
    return bb;
}

vec3 moGetCentroid(in MoTriangle self)
{
    return (self.v0 + self.v1 + self.v2) / 3.0f;
}

MoBBox moGetUVBoundingBox(in MoTriangle self)
{
    MoBBox bb;
    moInitBBox(bb, vec3(self.uv0, 0.f));
    moExpandToInclude(bb, vec3(self.uv1, 0.f));
    moExpandToInclude(bb, vec3(self.uv2, 0.f));
    return bb;
}

vec3 moGetUVCentroid(in MoTriangle self)
{
    return vec3((self.uv0 + self.uv1 + self.uv2) / 3.0f, 0.f);
}

vec3 moGetUVBarycentric(in MoTriangle self, in vec2 uv)
{
    vec4 x = vec4(uv.x, self.uv0.x, self.uv1.x, self.uv2.x);
    vec4 y = vec4(uv.y, self.uv0.y, self.uv1.y, self.uv2.y);

    float d = (y[2] - y[3]) * (x[1] - x[3]) + (x[3] - x[2]) * (y[1] - y[3]);
    float l1 = ((y[2] - y[3]) * (x[0] - x[3]) + (x[3] - x[2]) * (y[0] - y[3]))
            / d;
    float l2 = ((y[3] - y[1]) * (x[0] - x[3]) + (x[1] - x[3]) * (y[0] - y[3]))
            / d;
    float l3 = 1 - l1 - l2;

#if 0
    vec2 test = l1 * self.uv0 + l2 * self.uv1 + l3 * self.uv2;
#endif

    return vec3(l1, l2, l3);
}

void moGetSurface(in MoTriangle self, in vec3 barycentricCoordinates, out vec3 point, out vec3 normal)
{
    point = self.v0 * barycentricCoordinates[0]
          + self.v1 * barycentricCoordinates[1]
          + self.v2 * barycentricCoordinates[2];

    normal = self.n0 * barycentricCoordinates[0]
           + self.n1 * barycentricCoordinates[1]
           + self.n2 * barycentricCoordinates[2];
    normal = normalize(normal);
}

bool moRayTriangleIntersect(in MoTriangle self, in MoRay ray, out float t, out float u, out float v)
{
    float EPSILON = 0.0000001f;
    vec3 vertex0 = self.v0;
    vec3 vertex1 = self.v1;
    vec3 vertex2 = self.v2;
    vec3 edge1 = vertex1 - vertex0;
    vec3 edge2 = vertex2 - vertex0;
    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);
    if (a > -EPSILON && a < EPSILON)
    {
        // This ray is parallel to this triangle.
        return false;
    }

    float f = 1.0/a;
    vec3 s = ray.origin - vertex0;
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
    uint start;
    uint count;
    uint offset;
    MoBBox boundingBox;
};

struct MBVHWorkingSet
{
    uint index;
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

layout(std430, set = 2, binding = 0) buffer anotherLayoutName
{
    MoBVHSplitNode pSplitNodes[];
} inBVHSplitNodes;

layout(std430, set = 2, binding = 1) buffer yetAnotherLayoutName
{
    MoTriangle pObjects[];
} inBVHObjects;

bool moIntersectTriangleBVH(in MoRay ray)
{
    float distance = 1.0 / 0.0;
    float bbhits[4];
    uint closer, other;

    // Working set
    MBVHWorkingSet traversal[64];
    int stackPtr = 0;

    traversal[stackPtr].index = 0;
    traversal[stackPtr].distance = -1.0 / 0.0;

    while (stackPtr >= 0)
    {
        uint index = traversal[stackPtr].index;
        float near = traversal[stackPtr].distance;
        stackPtr--;
        MoBVHSplitNode node = inBVHSplitNodes.pSplitNodes[index];

        if (near > distance)
        {
            continue;
        }

        if (node.offset == 0)
        {
            for (uint i = 0; i < node.count; ++i)
            {
                float currentDistance;
                float u, v;
                MoTriangle triangle = inBVHObjects.pObjects[node.start + i];
                if (moRayTriangleIntersect(triangle, ray, currentDistance, u, v))
                {
                    if (currentDistance <= 0.0)
                    {
                        //intersection.pTriangle = &triangle;
                        return true;
                    }
                    //if (currentDistance < intersection.distance)
                    if (currentDistance < distance)
                    {
                        //intersection.pTriangle = &triangle;
                        //intersection.barycentric = {1.f - u - v, u, v};
                        //intersection.distance = currentDistance;
                        distance = currentDistance;
                    }
                }
            }
        }
        else
        {
            bool hitLeft = moIntersect(inBVHSplitNodes.pSplitNodes[index + 1].boundingBox, ray, bbhits[0], bbhits[1]);
            bool hitRight = moIntersect(inBVHSplitNodes.pSplitNodes[index + node.offset].boundingBox, ray, bbhits[2], bbhits[3]);

            if (hitLeft && hitRight)
            {
                closer = index + 1;
                other = index + node.offset;

                if (bbhits[2] < bbhits[0])
                {
                    swap(bbhits[0], bbhits[2]);
                    swap(bbhits[1], bbhits[3]);
                    swap(closer, other);
                }

                ++stackPtr;
                traversal[stackPtr] = MBVHWorkingSet(other, bbhits[2]);
                ++stackPtr;
                traversal[stackPtr] = MBVHWorkingSet(closer, bbhits[0]);
            }
            else if (hitLeft)
            {
                ++stackPtr;
                traversal[stackPtr] = MBVHWorkingSet(index + 1, bbhits[0]);
            }
            else if (hitRight)
            {
                ++stackPtr;
                traversal[stackPtr] = MBVHWorkingSet(index + node.offset, bbhits[2]);
            }
        }
    }

    //return intersection.distance < (1.0 / 0.0);
    return distance < (1.0 / 0.0);
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
layout(std140, set = 0, binding = 0) uniform Block
{
    uniform vec3 viewPosition;
    uniform vec3 lightPosition;
    uniform float lightPower;
} uniformData;
layout(set = 1, binding = 0) uniform sampler2D uniformTextureAmbient;
layout(set = 1, binding = 1) uniform sampler2D uniformTextureDiffuse;
layout(set = 1, binding = 2) uniform sampler2D uniformTextureNormal;
layout(set = 1, binding = 3) uniform sampler2D uniformTextureSpecular;
layout(set = 1, binding = 4) uniform sampler2D uniformTextureEmissive;

void main()
{
#if 0
    /// OLD PHONG
    vec2 texcoord = vec2(inData.texcoord.s, inData.texcoord.t);
    vec4 textureAmbient = texture(uniformTextureAmbient, texcoord);
    vec4 textureDiffuse = texture(uniformTextureDiffuse, texcoord);
    fragment = vec4(textureAmbient.rgb * textureDiffuse.rgb, textureDiffuse.a);

    vec4 textureNormal = texture(uniformTextureNormal, texcoord);
    // discard textureNormal when ~= (0,0,0)
    vec3 textureNormal_worldspace = length(textureNormal) > 0.1 ? normalize(inData.TBN * (2.0 * textureNormal.rgb - 1.0)) : inData.normal;
    vec3 lightDirection_worldspace = normalize(uniformData.lightPosition - inData.vertex);
    float diffuseFactor = dot(textureNormal_worldspace, lightDirection_worldspace);
    if (diffuseFactor > 0.0)
    {
        vec3 eyeDirection_worldspace = normalize(uniformData.viewPosition - inData.vertex);
        vec3 reflectDirection_worldspace = reflect(-lightDirection_worldspace, textureNormal_worldspace);

        float specularFactor = pow(max(dot(eyeDirection_worldspace, reflectDirection_worldspace), 0.0), 8.0);
        vec4 textureSpecular = texture(uniformTextureSpecular, texcoord);
        fragment += vec4(uniformData.lightPower * diffuseFactor * textureDiffuse.rgb + specularFactor * textureSpecular.rgb, 0.0);
    }
#endif

    /// RAYTRACING HERE
    MoRay ray;
    moInitRay(ray, vec3(1, vec2(1,-1) * (inData.texcoord - vec2(0.5))) * 10.0, vec3(-1., 0., 0.));

    if (moIntersectTriangleBVH(ray))
    {
        fragment = vec4(1., 1., 1., 1.);
    }
    else
    {
        fragment = vec4(0., 0., 0., 1.);
    }
}
#endif
