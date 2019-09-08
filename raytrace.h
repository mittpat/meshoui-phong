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
