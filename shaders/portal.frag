#version 330 core
in vec2 vTexCoord;
in vec4 vClipPos;
uniform sampler2D uPortalTexture;
uniform int uRenderMode;
uniform vec4 uPortalColor;
out vec4 FragColor;

void main() {
    if (uRenderMode == 0) {
        // Screen-space UV from clip coords
        vec2 screenUV = (vClipPos.xy / vClipPos.w) * 0.5 + 0.5;
        FragColor = texture(uPortalTexture, screenUV);
    } else {
        FragColor = uPortalColor;
    }
}
