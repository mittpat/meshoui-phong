#version 310 es
precision mediump float;

layout(location = 0) out vec4 fragment;

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec2 texcoord;
layout(location = 2) in vec3 normal;
layout(location = 3) in mat3 TBN;

void main()
{
    fragment = vec4(1.0, 0.0, 0.0, 1.0);
}
