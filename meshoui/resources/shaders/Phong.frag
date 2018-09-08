precision highp float;

out vec4 fragment;

in vec3 vertex;
in vec3 normal;
in vec2 texcoord;
in mat3 TBN;

#ifdef PARTICLES
in vec2 gl_PointCoord;
#endif

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

#ifdef FOG
uniform vec3 uniformFogColor;
uniform vec2 uniformFog;// (100.0, 50.0)
#endif

void main()
{
    float fragmentDist = distance(uniformViewPosition, vertex);

#ifdef PARTICLES
    vec2 texcoorda = gl_PointCoord * max(1.0, sqrt(fragmentDist));
#else
    vec2 texcoorda = texcoord;
#endif

    vec4 textureDiffuse = texture(uniformTextureDiffuse, vec2(texcoorda.s, 1.0 - texcoorda.t));

#ifndef PARTICLES
    if (textureDiffuse.a < 0.1)
        discard;
    textureDiffuse.a = 1.0;
#endif

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

#ifdef FOG
    vec3 fogColor = mix(ambient, uniformFogColor, pow(max(dot(vec3(0.0,1.0,0.0), lightDir), 0.0), 0.25));
    fragment = mix(fragment, vec4(fogColor, fragment.a), clamp((fragmentDist - uniformFog.x) / uniformFog.y, 0.0, 1.0));
#endif
}
