#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;
out vec2 vTexCoord;
out vec4 vClipPos;
void main() {
    vec4 worldPos = uModelMatrix * vec4(aPosition, 1.0);
    vec4 clipPos = uProjectionMatrix * uViewMatrix * worldPos;
    gl_Position = clipPos;
    vTexCoord = aTexCoord;
    vClipPos = clipPos;
}
