#version 150
precision highp float;

out vec4 fragment;

in vec3 vertex;
in vec3 normal;
in vec2 texcoord;
in mat3 TBN;

uniform sampler2D uniformTextureDiffuse;
uniform sampler2D uniformTextureNormal;
uniform sampler2D uniformTextureSpecular;

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

void alphaTest(inout vec4 color)
{
    if (color.a < 0.1)
        discard;
    color.a = 1.0;
}

void main()
{
    vec4 textureDiffuse = texture(uniformTextureDiffuse, vec2(texcoord.s, 1.0 - texcoord.t));
    alphaTest(textureDiffuse);

    vec3 ambient = (uniformAmbient + uniformLightAmbient) * uniformLightColor;
    textureDiffuse = uniformTextureDiffuseActive ? textureDiffuse : vec4(uniformDiffuse, 1.0);
    fragment = vec4(ambient * textureDiffuse.rgb, textureDiffuse.a);

    vec3 norm = normalize(TBN * (2.0 * normalize(texture(uniformTextureNormal, vec2(texcoord.s, 1.0 - texcoord.t)).rgb) - 1.0));
    norm = uniformTextureNormalActive ? norm : normal;
    vec3 lightDir = normalize(uniformLightPosition - vertex);
    float diffuseFactor = max(dot(norm, lightDir), 0.0);
    if (diffuseFactor > 0.0)
    {
        vec3 textureSpecular = texture(uniformTextureSpecular, vec2(texcoord.s, 1.0 - texcoord.t)).rgb;
        textureSpecular = uniformTextureSpecularActive ? textureSpecular : uniformSpecular;

        vec3 viewDir = normalize(uniformViewPosition - vertex);
        vec3 reflectDir = reflect(-lightDir, norm);
        float specularFactor = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);

        vec3 specular = textureSpecular * specularFactor * uniformLightColor;
        vec3 diffuse = diffuseFactor * uniformLightColor;

        fragment += vec4(diffuse * textureDiffuse.rgb + specular, 0.0);
    }
}
