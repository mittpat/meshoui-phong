#version 440 core

layout(location = 0) out vec4 fragment;

layout(location = 0) in VertexData
{
    vec3 vertex;
} inData;

layout(std140, binding = 0) uniform Block
{
    uniform vec3 viewPosition;
    uniform vec3 lightPosition;
} uniformData;

const vec3 skyColor = vec3(0.4, 0.5, 0.75);
const vec3 sunColor = vec3(0.7, 0.45, 0.1);

void main()
{
    vec3 position = inData.vertex;
    position.y = abs(position.y);

    vec3 sunPosition = normalize(uniformData.lightPosition - uniformData.viewPosition);

    float sunCircle = max(1.0 - (1.0 + 10.0 * sunPosition.y + position.y) * length(inData.vertex.xyz - sunPosition), 0.0);
    sunCircle += 0.3 * pow(1.0 - position.y, 12.0) * (1.6 - sunPosition.y);

    fragment = vec4(mix(skyColor.rgb, sunColor.rgb, sunCircle)
        * ((0.5 + 1.0 * pow(sunPosition.y, 0.4)) * (1.5 - position.y) + pow(sunCircle, 5.2)
        * sunPosition.y * (5.0 + 15.0 * sunPosition.y)), 1.0);
}
