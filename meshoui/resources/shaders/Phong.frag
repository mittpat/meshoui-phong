#version 450 core

layout(location = 0) out vec4 fragment;

layout(location=0) in VertexData
{
    vec3 vertex;
    vec3 normal;
    vec2 texcoord;
    mat3 TBN;
} inData;

//layout(set=0, binding = 0) uniform Block
//{
//uniform vec3 uniformLightPosition;
//uniform vec3 uniformViewPosition;
//};

void main()
{
    vec3 uniformLightPosition = vec3(1.0, 2.0, 5.0);
    vec3 uniformViewPosition = vec3(0.0, 2.0, 5.0);

    float fragmentDist = distance(uniformViewPosition, inData.vertex);

    vec3 ambient = vec3(0.1,0.1,0.1);
    vec4 textureDiffuse = vec4(0.8,0.6,0.4, 1.0);
    fragment = vec4(ambient * textureDiffuse.rgb, textureDiffuse.a);

    vec3 norm = inData.normal;
    vec3 lightDir = normalize(uniformLightPosition - inData.vertex);
    float diffuseFactor = max(dot(norm, lightDir), 0.0);
    if (diffuseFactor > 0.0)
    {
        vec3 textureSpecular = vec3(0.5,0.5,0.5);

        vec3 viewDir = normalize(uniformViewPosition - inData.vertex);
        vec3 reflectDir = reflect(-lightDir, norm);
        float specularFactor = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);

        vec3 specular = textureSpecular * specularFactor;
        vec3 diffuse = vec3(diffuseFactor);

        fragment += vec4(diffuse * textureDiffuse.rgb + specular, 0.0);
    }
}
