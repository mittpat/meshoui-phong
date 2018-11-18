#version 310 es
precision mediump float;

layout(location = 0) out vec3 vertex;
layout(location = 1) out vec2 texcoord;
layout(location = 2) out vec3 normal;
layout(location = 3) out mat3 TBN;

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexcoord;
layout(location = 2) in vec3 vertexNormal;
layout(location = 3) in vec3 vertexTangent;
layout(location = 4) in vec3 vertexBitangent;

void main()
{
    //normal = vertexNormal;
    texcoord = vertexTexcoord;
    //vec3 T = vertexTangent;
    //vec3 B = vertexBitangent;
    //vec3 N = vertexNormal;
    //TBN = mat3(T, B, N);

    vertex = vec3(0.0021 * vertexPosition.x - 1.0, 0.0021 * vertexPosition.y - 1.0, vertexPosition.z);

    gl_Position = vec4(vertex, 1.0);
}
