#version 310 es
precision mediump float;

layout(location = 0) out vec4 fragment;

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in mat3 TBN;

layout(binding = 0) uniform sampler2D uniformTextureDiffuse;
layout(binding = 1) uniform sampler2D uniformTextureNormal;
layout(binding = 2) uniform sampler2D uniformTextureSpecular;

layout(set=0, binding = 0) uniform Block
{
uniform bool uniformTextureDiffuseActive;
uniform bool uniformTextureNormalActive;
uniform bool uniformTextureSpecularActive;

uniform vec3 uniformLightPosition;
uniform vec3 uniformViewPosition;
uniform vec3 uniformLightAmbient;
uniform vec3 uniformLightColor;
uniform vec3 uniformAmbient;
uniform vec3 uniformDiffuse;
uniform vec3 uniformSpecular;
uniform vec3 uniformEmissive;
};

void main()
{
    float fragmentDist = distance(uniformViewPosition, vertex);

    vec2 texcoorda = texcoord;

    vec4 textureDiffuse = texture(uniformTextureDiffuse, vec2(texcoorda.s, 1.0 - texcoorda.t));

    vec3 ambient = (uniformAmbient + uniformLightAmbient) * uniformLightColor;
    textureDiffuse = uniformTextureDiffuseActive ? textureDiffuse : vec4(uniformDiffuse, 1.0);
    fragment = vec4(ambient * textureDiffuse.rgb, textureDiffuse.a);

    vec3 norm = normalize(TBN * (2.0 * normalize(texture(uniformTextureNormal, vec2(texcoorda.s, 1.0 - texcoorda.t)).rgb) - 1.0));
    norm = uniformTextureNormalActive ? norm : normal;
    vec3 lightDir = normalize(uniformLightPosition - vertex);
    float diffuseFactor = max(dot(norm, lightDir), 0.0);
    if (diffuseFactor > 0.0)
    {
        vec3 textureSpecular = texture(uniformTextureSpecular, vec2(texcoorda.s, 1.0 - texcoorda.t)).rgb;
        textureSpecular = uniformTextureSpecularActive ? textureSpecular : uniformSpecular;

        vec3 viewDir = normalize(uniformViewPosition - vertex);
        vec3 reflectDir = reflect(-lightDir, norm);
        float specularFactor = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);

        vec3 specular = textureSpecular * specularFactor * uniformLightColor;
        vec3 diffuse = diffuseFactor * uniformLightColor;

        fragment += vec4(diffuse * textureDiffuse.rgb + specular, 0.0);
    }
}
