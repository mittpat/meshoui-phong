#version 450 core

#ifdef COMPILING_VERTEX
layout(location=0) out VertexData
{
    vec3 vertex;
    vec3 normal;
    vec2 texcoord;
    mat3 TBN;
} outData;
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexcoord;
layout(location = 2) in vec3 vertexNormal;
layout(location = 3) in vec3 vertexTangent;
layout(location = 4) in vec3 vertexBitangent;
layout(push_constant) uniform uPushConstant
{
    mat4 uniformModel;
    mat4 uniformView;
    mat4 uniformProjection;
} pc;

void main()
{
    outData.vertex = vec3(pc.uniformModel * vec4(vertexPosition, 1.0));
    outData.normal = normalize(mat3(transpose(inverse(pc.uniformModel))) * vertexNormal);
    outData.texcoord = vertexTexcoord;
    vec3 T = normalize(vec3(mat3(pc.uniformModel) * vertexTangent));
    vec3 B = normalize(vec3(mat3(pc.uniformModel) * vertexBitangent));
    vec3 N = normalize(vec3(mat3(pc.uniformModel) * vertexNormal));
    outData.TBN = mat3(T, B, N);
    gl_Position = pc.uniformProjection * pc.uniformView * pc.uniformModel * vec4(vertexPosition, 1.0);
}
#endif

#ifdef COMPILING_FRAGMENT
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
    uniform float lightPower;
} uniformData;
layout(set = 1, binding = 0) uniform sampler2D uniformTextureAmbient;
layout(set = 1, binding = 1) uniform sampler2D uniformTextureDiffuse;
layout(set = 1, binding = 2) uniform sampler2D uniformTextureNormal;
layout(set = 1, binding = 3) uniform sampler2D uniformTextureSpecular;
layout(set = 1, binding = 4) uniform sampler2D uniformTextureEmissive;

void main()
{
    vec2 texcoord = vec2(inData.texcoord.s, inData.texcoord.t);
    vec4 textureAmbient = texture(uniformTextureAmbient, texcoord);
    vec4 textureDiffuse = texture(uniformTextureDiffuse, texcoord);
    fragment = vec4(textureAmbient.rgb * textureDiffuse.rgb, textureDiffuse.a);

    vec4 textureNormal = texture(uniformTextureNormal, texcoord);
    // discard textureNormal when ~= (0,0,0)
    vec3 textureNormal_worldspace = length(textureNormal) > 0.1 ? normalize(inData.TBN * (2.0 * textureNormal.rgb - 1.0)) : inData.normal;
    vec3 lightDirection_worldspace = normalize(uniformData.lightPosition - inData.vertex);
    float diffuseFactor = dot(textureNormal_worldspace, lightDirection_worldspace);
    if (diffuseFactor > 0.0)
    {
        vec3 eyeDirection_worldspace = normalize(uniformData.viewPosition - inData.vertex);
        vec3 reflectDirection_worldspace = reflect(-lightDirection_worldspace, textureNormal_worldspace);

        float specularFactor = pow(max(dot(eyeDirection_worldspace, reflectDirection_worldspace), 0.0), 8.0);
        vec4 textureSpecular = texture(uniformTextureSpecular, texcoord);
        fragment += vec4(uniformData.lightPower * diffuseFactor * textureDiffuse.rgb + specularFactor * textureSpecular.rgb, 0.0);
    }
}
#endif
