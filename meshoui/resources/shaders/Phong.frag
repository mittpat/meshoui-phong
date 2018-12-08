#version 450 core

layout(location = 0) out vec4 fragment;

layout(location = 0) in VertexData
{
    vec3 vertex;
    vec3 normal;
    vec2 texcoord;
    mat3 TBN;
} inData;

layout(std140, binding = 0) uniform Block
{
    uniform vec3 viewPosition;
    uniform vec3 lightPosition;
} uniformData;
/*
layout(std140, binding = 1) uniform Block2
{
    uniform vec3 ambient;
    uniform vec3 diffuse;
    uniform vec3 specular;
    uniform vec3 emissive;
} materialData;
*/
layout(set=1, binding=0) uniform sampler2D uniformTextureDiffuse;
layout(set=1, binding=1) uniform sampler2D uniformTextureNormal;
layout(set=1, binding=2) uniform sampler2D uniformTextureSpecular;
layout(set=1, binding=3) uniform sampler2D uniformTextureEmissive;

void main()
{
    float fragmentDist = distance(uniformData.viewPosition, inData.vertex);

    vec3 ambient = vec3(0.0, 0.0, 0.0);//materialData.ambient;
    vec4 textureDiffuse = texture(uniformTextureDiffuse, vec2(inData.texcoord.s, 1.0 - inData.texcoord.t));
    fragment = vec4(ambient * textureDiffuse.rgb, textureDiffuse.a);

    vec3 norm = normalize(inData.TBN * (2.0 * normalize(texture(uniformTextureNormal, vec2(inData.texcoord.s, 1.0 - inData.texcoord.t)).rgb) - 1.0));
    vec3 lightDir = normalize(uniformData.lightPosition - inData.vertex);
    float diffuseFactor = max(dot(norm, lightDir), 0.0);
    if (diffuseFactor > 0.0)
    {
        vec3 textureSpecular = texture(uniformTextureSpecular, vec2(inData.texcoord.s, 1.0 - inData.texcoord.t)).rgb;

        vec3 viewDir = normalize(uniformData.viewPosition - inData.vertex);
        vec3 reflectDir = reflect(-lightDir, norm);
        float specularFactor = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);

        vec3 specular = textureSpecular * specularFactor;
        vec3 diffuse = vec3(diffuseFactor);

        fragment += vec4(diffuse * textureDiffuse.rgb + specular, 0.0);
    }
}
