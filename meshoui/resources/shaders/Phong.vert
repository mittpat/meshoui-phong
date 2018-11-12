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

layout(set=0, binding = 0) uniform Block
{
uniform mat4 uniformModel;
uniform mat4 uniformView;
uniform mat4 uniformProjection;
uniform vec3 uniformLightPosition;
};

void main()
{
    normal = normalize(mat3(transpose(inverse(uniformModel))) * vertexNormal);
    texcoord = vertexTexcoord;
    vec3 T = normalize(vec3(mat3(uniformModel) * vertexTangent));
    vec3 B = normalize(vec3(mat3(uniformModel) * vertexBitangent));
    vec3 N = normalize(vec3(mat3(uniformModel) * vertexNormal));
    TBN = mat3(T, B, N);

    vertex = vec3(uniformModel * vec4(vertexPosition, 1.0));

    gl_Position = uniformProjection * uniformView * vec4(vertex, 1.0);
}
