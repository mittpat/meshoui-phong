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

#ifdef PARTICLES
in int gl_VertexID;
#endif

#ifdef PARTICLES
uniform vec2 uniformEffect; //rate of emission, speed
uniform vec2 uniformTime;
#endif

uniform mat4 uniformModel;
uniform mat4 uniformView;
uniform mat4 uniformProjection;
uniform vec3 uniformLightPosition;

#ifdef PARTICLES
const vec3 gravity = vec3(0., -9.8, 0.);
#endif

void main()
{
    normal = normalize(mat3(transpose(inverse(uniformModel))) * vertexNormal);
    texcoord = vertexTexcoord;
    vec3 T = normalize(vec3(mat3(uniformModel) * vertexTangent));
    vec3 B = normalize(vec3(mat3(uniformModel) * vertexBitangent));
    vec3 N = normalize(vec3(mat3(uniformModel) * vertexNormal));
    TBN = mat3(T, B, N);

    vertex = vec3(uniformModel * vec4(vertexPosition, 1.0));

#ifdef PARTICLES
    float time = gl_VertexID * 1.0 / uniformEffect.x + mod(uniformTime.x, 1.0 / uniformEffect.x);
    vertex += normal * uniformEffect.y * time + 0.5 * gravity * time * time;
#endif

    gl_Position = uniformProjection * uniformView * vec4(vertex, 1.0);
}
