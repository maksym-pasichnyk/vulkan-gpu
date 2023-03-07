#version 450

layout(location = 0) out vec2 TexCoordOutput;

vec2 QuadVertices[] = {
    vec2(-1.0, -1.0),
    vec2(+1.0, -1.0),
    vec2(-1.0, +1.0),
    vec2(+1.0, +1.0)
};

vec2 QuadTexCoords[] = {
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
};

uint QuadIndices[] = {
    0, 1, 2,
    2, 1, 3
};

void main() {
    gl_Position = vec4(QuadVertices[QuadIndices[gl_VertexIndex]], 0.0, 1.0);
    TexCoordOutput = QuadTexCoords[QuadIndices[gl_VertexIndex]];
}
