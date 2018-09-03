#version 150
precision mediump float;

out vec3 vertex;
out vec3 normal;
out vec2 texcoord;
out mat3 TBN;

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexcoord;
in vec3 vertexTangent;
in vec3 vertexBitangent;

uniform mat4 uniformModel;
uniform mat4 uniformView;
uniform mat4 uniformProjection;
uniform vec3 uniformLightPosition;

void main()
{
    vertex = vec3(uniformModel * vec4(vertexPosition, 1.0));

    gl_Position = uniformProjection * uniformView * vec4(vertex, 1.0);

    normal = normalize(mat3(transpose(inverse(uniformModel))) * vertexNormal);
    texcoord = vertexTexcoord;
    vec3 T = normalize(vec3(mat3(uniformModel) * vertexTangent));
    vec3 B = normalize(vec3(mat3(uniformModel) * vertexBitangent));
    vec3 N = normalize(vec3(mat3(uniformModel) * vertexNormal));
    TBN = mat3(T, B, N);
}
