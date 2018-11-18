#version 450 core

layout(location = 0) out vec3 vertex;
layout(location = 1) out vec2 texcoord;
layout(location = 2) out vec3 normal;
layout(location = 3) out mat3 TBN;

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexcoord;
layout(location = 2) in vec3 vertexNormal;
layout(location = 3) in vec3 vertexTangent;
layout(location = 4) in vec3 vertexBitangent;

layout(push_constant) uniform uPushConstant{
    mat4 uniformModel;
    mat4 uniformView;
    mat4 uniformProjection;
} pc;

void main()
{
    vertex = vec3(pc.uniformModel * vec4(vertexPosition, 1.0));
    gl_Position = pc.uniformProjection * pc.uniformView * vec4(vertex, 1.0);
}
