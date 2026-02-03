/**
 * main_example.cpp - Portal渲染系统使用示例
 */

#include "PortalMath.h"
#include "PortalRenderer.h"
#include "PortalTeleporter.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>

// ============================================================================
// RenderDoc Debug Markers
// ============================================================================
// 用于在 RenderDoc 中显示渲染事件层级
void PushDebugGroup(const char* name) {
    if (glPushDebugGroup) {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
    }
}

void PopDebugGroup() {
    if (glPopDebugGroup) {
        glPopDebugGroup();
    }
}

// 带格式化的调试组
void PushDebugGroupF(const char* format, int value) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), format, value);
    PushDebugGroup(buffer);
}

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

std::vector<PortalRenderer::Portal*> g_Portals;
PortalTeleporter::TeleportableEntity g_Player;
glm::vec3 g_CameraPosition(0.0f, 1.7f, 5.0f);
float g_CameraYaw = -90.0f;
float g_CameraPitch = 0.0f;

const float PORTAL_WIDTH = 2.0f;
const float PORTAL_HEIGHT = 3.0f;

// Scene shader for demo
static GLuint g_SceneShader = 0;
static GLuint g_CubeVAO = 0;

const char* SCENE_VS = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

const char* SCENE_FS = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

// ============================================================================
// 天空盒着色器 - 带程序化云层
// ============================================================================
const char* SKYBOX_VS = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
out vec3 vTexCoord;
uniform mat4 uViewProj;
void main() {
    vTexCoord = aPos;
    vec4 pos = uViewProj * vec4(aPos, 1.0);
    gl_Position = pos.xyww;  // 保持z=w，确保天空盒始终在最远处
}
)";

const char* SKYBOX_FS = R"(
#version 330 core
in vec3 vTexCoord;
out vec4 FragColor;
uniform float uTime;

// 简单的噪声函数
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// FBM (Fractal Brownian Motion) 用于生成云层
float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < 5; i++) {
        value += amplitude * noise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    // 标准化方向向量
    vec3 dir = normalize(vTexCoord);
    
    // 基于高度的天空渐变
    float height = dir.y * 0.5 + 0.5;  // 0 到 1
    
    // 天空颜色渐变
    vec3 horizonColor = vec3(0.7, 0.85, 1.0);   // 地平线 - 浅蓝
    vec3 zenithColor = vec3(0.25, 0.55, 0.95);  // 天顶 - 深蓝
    vec3 nadirColor = vec3(0.15, 0.25, 0.35);   // 地下 - 深色（门户内背景）
    
    vec3 skyColor;
    if (dir.y >= 0.0) {
        // 天空部分
        float t = pow(height, 0.8);
        skyColor = mix(horizonColor, zenithColor, t);
    } else {
        // 地平线以下（用于门户背景）
        skyColor = mix(horizonColor, nadirColor, -dir.y);
    }
    
    // ============ 云层生成 ============
    if (dir.y > 0.0) {
        // 投影到云层平面 (假设云层在高处)
        float cloudHeight = 0.3;  // 云层开始的高度
        
        if (dir.y > 0.05) {
            // 计算云层采样坐标
            vec2 cloudUV = dir.xz / (dir.y + 0.3);
            cloudUV *= 2.0;  // 缩放
            
            // 动态偏移 - 云层飘动
            cloudUV.x += uTime * 0.02;  // 主方向风速
            cloudUV.y += uTime * 0.005; // 轻微的垂直风
            
            // 多层云
            float cloud1 = fbm(cloudUV * 1.0);
            float cloud2 = fbm(cloudUV * 2.0 + vec2(uTime * 0.01, 0.0));
            float cloud3 = fbm(cloudUV * 0.5 + vec2(uTime * 0.03, uTime * 0.01));
            
            // 组合云层
            float clouds = cloud1 * 0.5 + cloud2 * 0.3 + cloud3 * 0.2;
            
            // 云的密度阈值
            float cloudThreshold = 0.4;
            float cloudDensity = smoothstep(cloudThreshold, cloudThreshold + 0.3, clouds);
            
            // 云的颜色（白色到灰色）
            vec3 cloudColor = mix(vec3(1.0, 1.0, 1.0), vec3(0.85, 0.88, 0.92), cloud2);
            
            // 云的阴影
            float cloudShadow = 1.0 - cloud2 * 0.3;
            cloudColor *= cloudShadow;
            
            // 地平线处云渐隐
            float horizonFade = smoothstep(0.0, 0.3, dir.y);
            cloudDensity *= horizonFade;
            
            // 混合云层到天空
            skyColor = mix(skyColor, cloudColor, cloudDensity * 0.9);
        }
    }
    
    // 添加太阳光晕
    vec3 sunDir = normalize(vec3(0.5, 0.3, -0.8));
    float sunDot = max(dot(dir, sunDir), 0.0);
    float sunGlow = pow(sunDot, 64.0);
    float sunHalo = pow(sunDot, 8.0) * 0.3;
    
    skyColor += vec3(1.0, 0.95, 0.8) * sunGlow;
    skyColor += vec3(1.0, 0.8, 0.6) * sunHalo;
    
    // 轻微的大气散射效果
    float scatter = pow(1.0 - abs(dir.y), 3.0);
    skyColor = mix(skyColor, horizonColor * 1.1, scatter * 0.3);
    
    FragColor = vec4(skyColor, 1.0);
}
)";

// 天空盒全局变量
static GLuint g_SkyboxShader = 0;
static GLuint g_SkyboxVAO = 0;

// Scene geometry data
static GLuint g_FloorVAO = 0;
static GLuint g_WallVAO = 0;
static GLuint g_BoxVAO = 0;
static GLuint g_PillarVAO = 0;
static int g_FloorVertCount = 0;
static int g_WallVertCount = 0;
static int g_BoxVertCount = 0;
static int g_PillarVertCount = 0;

// Helper to create a colored quad
void AddQuad(std::vector<float>& verts, 
             glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
             glm::vec3 color) {
    // Triangle 1
    verts.insert(verts.end(), {p0.x, p0.y, p0.z, color.r, color.g, color.b});
    verts.insert(verts.end(), {p1.x, p1.y, p1.z, color.r, color.g, color.b});
    verts.insert(verts.end(), {p2.x, p2.y, p2.z, color.r, color.g, color.b});
    // Triangle 2
    verts.insert(verts.end(), {p0.x, p0.y, p0.z, color.r, color.g, color.b});
    verts.insert(verts.end(), {p2.x, p2.y, p2.z, color.r, color.g, color.b});
    verts.insert(verts.end(), {p3.x, p3.y, p3.z, color.r, color.g, color.b});
}

// Helper to create a box
void AddBox(std::vector<float>& verts, glm::vec3 center, glm::vec3 size, glm::vec3 color) {
    float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;
    glm::vec3 c = center;
    
    // Front face
    AddQuad(verts, 
        c + glm::vec3(-hx, -hy, hz), c + glm::vec3(hx, -hy, hz),
        c + glm::vec3(hx, hy, hz), c + glm::vec3(-hx, hy, hz), color);
    // Back face
    AddQuad(verts,
        c + glm::vec3(hx, -hy, -hz), c + glm::vec3(-hx, -hy, -hz),
        c + glm::vec3(-hx, hy, -hz), c + glm::vec3(hx, hy, -hz), color * 0.8f);
    // Left face
    AddQuad(verts,
        c + glm::vec3(-hx, -hy, -hz), c + glm::vec3(-hx, -hy, hz),
        c + glm::vec3(-hx, hy, hz), c + glm::vec3(-hx, hy, -hz), color * 0.9f);
    // Right face
    AddQuad(verts,
        c + glm::vec3(hx, -hy, hz), c + glm::vec3(hx, -hy, -hz),
        c + glm::vec3(hx, hy, -hz), c + glm::vec3(hx, hy, hz), color * 0.9f);
    // Top face
    AddQuad(verts,
        c + glm::vec3(-hx, hy, hz), c + glm::vec3(hx, hy, hz),
        c + glm::vec3(hx, hy, -hz), c + glm::vec3(-hx, hy, -hz), color * 1.1f);
    // Bottom face
    AddQuad(verts,
        c + glm::vec3(-hx, -hy, -hz), c + glm::vec3(hx, -hy, -hz),
        c + glm::vec3(hx, -hy, hz), c + glm::vec3(-hx, -hy, hz), color * 0.7f);
}

GLuint CreateVAOFromVertices(const std::vector<float>& vertices) {
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return VAO;
}

// Helper to add a double-sided quad (both front and back faces)
void AddDoubleSidedQuad(std::vector<float>& verts, 
                        glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
                        glm::vec3 colorFront, glm::vec3 colorBack) {
    // Front face
    AddQuad(verts, p0, p1, p2, p3, colorFront);
    // Back face (reversed winding order)
    AddQuad(verts, p3, p2, p1, p0, colorBack);
}

void CreateSceneGeometry() {
    // ============ FLOOR (Large checkered pattern) ============
    {
        std::vector<float> floorVerts;
        float tileSize = 2.0f;
        int gridSize = 50;  // 扩大地板范围：100x100单位
        for (int x = -gridSize; x < gridSize; x++) {
            for (int z = -gridSize; z < gridSize; z++) {
                bool isWhite = ((x + z) % 2 == 0);
                glm::vec3 color = isWhite ? glm::vec3(0.7f, 0.7f, 0.75f) : glm::vec3(0.3f, 0.3f, 0.35f);
                glm::vec3 p0(x * tileSize, 0.0f, z * tileSize);
                glm::vec3 p1((x + 1) * tileSize, 0.0f, z * tileSize);
                glm::vec3 p2((x + 1) * tileSize, 0.0f, (z + 1) * tileSize);
                glm::vec3 p3(x * tileSize, 0.0f, (z + 1) * tileSize);
                // 使用逆时针顺序 (p0->p3->p2->p1) 让法线朝上 (+Y)
                AddQuad(floorVerts, p0, p3, p2, p1, color);
            }
        }
        g_FloorVAO = CreateVAOFromVertices(floorVerts);
        g_FloorVertCount = (int)floorVerts.size() / 6;
    }
    
    // ============ WALLS (Double-sided) ============
    {
        std::vector<float> wallVerts;
        float wallHeight = 8.0f;
        float roomSize = 30.0f;
        glm::vec3 wallColor1(0.6f, 0.55f, 0.5f);
        glm::vec3 wallColor1Back(0.5f, 0.45f, 0.4f);  // 背面稍暗
        glm::vec3 wallColor2(0.5f, 0.6f, 0.55f);
        glm::vec3 wallColor2Back(0.4f, 0.5f, 0.45f);  // 背面稍暗
        
        // Room A walls (around portal A at -5, 1.5, 0)
        // Back wall - 双面
        AddDoubleSidedQuad(wallVerts,
            glm::vec3(-roomSize, 0, -15), glm::vec3(-2, 0, -15),
            glm::vec3(-2, wallHeight, -15), glm::vec3(-roomSize, wallHeight, -15), 
            wallColor1, wallColor1Back);
        // Left wall - 双面
        AddDoubleSidedQuad(wallVerts,
            glm::vec3(-roomSize, 0, 15), glm::vec3(-roomSize, 0, -15),
            glm::vec3(-roomSize, wallHeight, -15), glm::vec3(-roomSize, wallHeight, 15), 
            wallColor1 * 0.9f, wallColor1Back * 0.9f);
        // Front wall (with opening) - 双面
        AddDoubleSidedQuad(wallVerts,
            glm::vec3(-2, 0, 15), glm::vec3(-roomSize, 0, 15),
            glm::vec3(-roomSize, wallHeight, 15), glm::vec3(-2, wallHeight, 15), 
            wallColor1 * 0.85f, wallColor1Back * 0.85f);
            
        // Room B walls (around portal B at 5, 1.5, -10)
        // Back wall - 双面
        AddDoubleSidedQuad(wallVerts,
            glm::vec3(2, 0, -roomSize), glm::vec3(roomSize, 0, -roomSize),
            glm::vec3(roomSize, wallHeight, -roomSize), glm::vec3(2, wallHeight, -roomSize), 
            wallColor2, wallColor2Back);
        // Right wall - 双面
        AddDoubleSidedQuad(wallVerts,
            glm::vec3(roomSize, 0, -roomSize), glm::vec3(roomSize, 0, -5),
            glm::vec3(roomSize, wallHeight, -5), glm::vec3(roomSize, wallHeight, -roomSize), 
            wallColor2 * 0.9f, wallColor2Back * 0.9f);
        // Side wall - 双面
        AddDoubleSidedQuad(wallVerts,
            glm::vec3(2, 0, -5), glm::vec3(2, 0, -roomSize),
            glm::vec3(2, wallHeight, -roomSize), glm::vec3(2, wallHeight, -5), 
            wallColor2 * 0.85f, wallColor2Back * 0.85f);
        
        g_WallVAO = CreateVAOFromVertices(wallVerts);
        g_WallVertCount = (int)wallVerts.size() / 6;
    }
    
    // ============ DECORATIVE BOXES ============
    {
        std::vector<float> boxVerts;
        
        // Room A decorations (blue/cyan themed)
        AddBox(boxVerts, glm::vec3(-8, 0.5f, -5), glm::vec3(1, 1, 1), glm::vec3(0.2f, 0.5f, 0.8f));
        AddBox(boxVerts, glm::vec3(-10, 0.75f, 3), glm::vec3(1.5f, 1.5f, 1.5f), glm::vec3(0.3f, 0.6f, 0.9f));
        AddBox(boxVerts, glm::vec3(-12, 1.0f, -8), glm::vec3(2, 2, 2), glm::vec3(0.1f, 0.4f, 0.7f));
        AddBox(boxVerts, glm::vec3(-6, 0.4f, 8), glm::vec3(0.8f, 0.8f, 0.8f), glm::vec3(0.4f, 0.7f, 1.0f));
        // Stacked boxes
        AddBox(boxVerts, glm::vec3(-15, 0.5f, 0), glm::vec3(1, 1, 1), glm::vec3(0.25f, 0.55f, 0.85f));
        AddBox(boxVerts, glm::vec3(-15, 1.5f, 0), glm::vec3(0.8f, 1, 0.8f), glm::vec3(0.3f, 0.6f, 0.9f));
        AddBox(boxVerts, glm::vec3(-15, 2.4f, 0), glm::vec3(0.6f, 0.8f, 0.6f), glm::vec3(0.35f, 0.65f, 0.95f));
        
        // Room B decorations (orange/red themed)
        AddBox(boxVerts, glm::vec3(8, 0.5f, -12), glm::vec3(1, 1, 1), glm::vec3(0.9f, 0.4f, 0.2f));
        AddBox(boxVerts, glm::vec3(12, 0.75f, -15), glm::vec3(1.5f, 1.5f, 1.5f), glm::vec3(0.95f, 0.5f, 0.25f));
        AddBox(boxVerts, glm::vec3(15, 1.0f, -20), glm::vec3(2, 2, 2), glm::vec3(0.85f, 0.35f, 0.15f));
        AddBox(boxVerts, glm::vec3(6, 0.4f, -18), glm::vec3(0.8f, 0.8f, 0.8f), glm::vec3(1.0f, 0.55f, 0.3f));
        // Stacked boxes
        AddBox(boxVerts, glm::vec3(20, 0.5f, -15), glm::vec3(1, 1, 1), glm::vec3(0.9f, 0.45f, 0.2f));
        AddBox(boxVerts, glm::vec3(20, 1.5f, -15), glm::vec3(0.8f, 1, 0.8f), glm::vec3(0.95f, 0.5f, 0.25f));
        AddBox(boxVerts, glm::vec3(20, 2.4f, -15), glm::vec3(0.6f, 0.8f, 0.6f), glm::vec3(1.0f, 0.55f, 0.3f));
        
        // Central area (green themed)
        AddBox(boxVerts, glm::vec3(0, 0.6f, 5), glm::vec3(1.2f, 1.2f, 1.2f), glm::vec3(0.3f, 0.7f, 0.3f));
        AddBox(boxVerts, glm::vec3(3, 0.5f, 3), glm::vec3(1, 1, 1), glm::vec3(0.35f, 0.75f, 0.35f));
        
        g_BoxVAO = CreateVAOFromVertices(boxVerts);
        g_BoxVertCount = (int)boxVerts.size() / 6;
    }
    
    // ============ PILLARS ============
    {
        std::vector<float> pillarVerts;
        glm::vec3 pillarColor(0.65f, 0.6f, 0.55f);
        
        // Room A pillars
        AddBox(pillarVerts, glm::vec3(-20, 4, -10), glm::vec3(1.5f, 8, 1.5f), pillarColor);
        AddBox(pillarVerts, glm::vec3(-20, 4, 10), glm::vec3(1.5f, 8, 1.5f), pillarColor);
        AddBox(pillarVerts, glm::vec3(-10, 4, -10), glm::vec3(1.5f, 8, 1.5f), pillarColor * 0.95f);
        AddBox(pillarVerts, glm::vec3(-10, 4, 10), glm::vec3(1.5f, 8, 1.5f), pillarColor * 0.95f);
        
        // Room B pillars
        AddBox(pillarVerts, glm::vec3(10, 4, -25), glm::vec3(1.5f, 8, 1.5f), pillarColor);
        AddBox(pillarVerts, glm::vec3(25, 4, -25), glm::vec3(1.5f, 8, 1.5f), pillarColor);
        AddBox(pillarVerts, glm::vec3(10, 4, -10), glm::vec3(1.5f, 8, 1.5f), pillarColor * 0.95f);
        AddBox(pillarVerts, glm::vec3(25, 4, -10), glm::vec3(1.5f, 8, 1.5f), pillarColor * 0.95f);
        
        g_PillarVAO = CreateVAOFromVertices(pillarVerts);
        g_PillarVertCount = (int)pillarVerts.size() / 6;
    }
    
    // Keep original simple VAO for compatibility
    g_CubeVAO = g_BoxVAO;
}

void RenderScene(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    glUseProgram(g_SceneShader);
    glm::mat4 mvp = projectionMatrix * viewMatrix;
    glUniformMatrix4fv(glGetUniformLocation(g_SceneShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
    
    // Draw floor
    glBindVertexArray(g_FloorVAO);
    glDrawArrays(GL_TRIANGLES, 0, g_FloorVertCount);
    
    // Draw walls
    glBindVertexArray(g_WallVAO);
    glDrawArrays(GL_TRIANGLES, 0, g_WallVertCount);
    
    // Draw boxes
    glBindVertexArray(g_BoxVAO);
    glDrawArrays(GL_TRIANGLES, 0, g_BoxVertCount);
    
    // Draw pillars
    glBindVertexArray(g_PillarVAO);
    glDrawArrays(GL_TRIANGLES, 0, g_PillarVertCount);
    
    glBindVertexArray(0);
}

void SetupPortals() {
    // 创建门户A - 位于玩家初始位置的左前方
    // 门户正面朝向 +Z（朝向玩家）
    PortalRenderer::Portal* portalA = new PortalRenderer::Portal();
    portalA->transform = glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 1.5f, 0.0f));
    // 旋转使门户正面朝向 +Z（朝向玩家初始位置方向）
    portalA->transform = glm::rotate(portalA->transform, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    portalA->width = PORTAL_WIDTH;
    portalA->height = PORTAL_HEIGHT;
    portalA->isActive = true;
    PortalRenderer::CreatePortalMesh(portalA);
    PortalRenderer::CreatePortalRenderTarget(portalA, WINDOW_WIDTH, WINDOW_HEIGHT);
    portalA->shaderProgram = PortalRenderer::CompilePortalShader();
    
    // 创建门户B - 位于另一个区域
    // 门户正面朝向 -X 方向（旋转90度后）
    PortalRenderer::Portal* portalB = new PortalRenderer::Portal();
    portalB->transform = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 1.5f, -10.0f));
    // 旋转使门户正面朝向 -X 方向
    portalB->transform = glm::rotate(portalB->transform, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    portalB->width = PORTAL_WIDTH;
    portalB->height = PORTAL_HEIGHT;
    portalB->isActive = true;
    PortalRenderer::CreatePortalMesh(portalB);
    PortalRenderer::CreatePortalRenderTarget(portalB, WINDOW_WIDTH, WINDOW_HEIGHT);
    portalB->shaderProgram = PortalRenderer::CompilePortalShader();
    
    // 链接门户
    portalA->linkedPortal = portalB;
    portalB->linkedPortal = portalA;
    
    g_Portals.push_back(portalA);
    g_Portals.push_back(portalB);
}

void SetupPlayer() {
    g_Player.position = g_CameraPosition;
    g_Player.previousPosition = g_CameraPosition;
    g_Player.velocity = glm::vec3(0.0f);
    g_Player.transform = glm::translate(glm::mat4(1.0f), g_CameraPosition);
}

void UpdatePlayer(float deltaTime, float currentTime) {
    g_Player.previousPosition = g_Player.position;
    g_Player.position = g_CameraPosition;
    
    for (PortalRenderer::Portal* portal : g_Portals) {
        if (!portal->isActive || !portal->linkedPortal) continue;
        if (PortalTeleporter::ShouldTeleport(g_Player, portal, PORTAL_WIDTH / 2.0f, PORTAL_HEIGHT / 2.0f, currentTime)) {
            // 传送位置
            PortalTeleporter::TeleportEntity(g_Player, portal, portal->linkedPortal);
            g_CameraPosition = g_Player.position;
            
            // 传送相机朝向 - 计算新的Yaw角度
            // 当前视线方向
            glm::vec3 currentForward;
            currentForward.x = cos(glm::radians(g_CameraYaw)) * cos(glm::radians(g_CameraPitch));
            currentForward.y = sin(glm::radians(g_CameraPitch));
            currentForward.z = sin(glm::radians(g_CameraYaw)) * cos(glm::radians(g_CameraPitch));
            currentForward = glm::normalize(currentForward);
            
            // 传送后的视线方向
            glm::vec3 newForward = PortalMath::TeleportDirection(currentForward, portal->transform, portal->linkedPortal->transform);
            
            // 从新方向计算Yaw和Pitch
            g_CameraYaw = glm::degrees(atan2(newForward.z, newForward.x));
            g_CameraPitch = glm::degrees(asin(glm::clamp(newForward.y, -1.0f, 1.0f)));
            
            std::cout << "Teleported! New position: (" << g_Player.position.x << ", " << g_Player.position.y << ", " << g_Player.position.z << ")" << std::endl;
            std::cout << "New camera direction: Yaw=" << g_CameraYaw << ", Pitch=" << g_CameraPitch << std::endl;
            break;
        }
    }
}

// Portal frame VAO for visual representation
static GLuint g_PortalFrameVAO = 0;
static int g_PortalFrameVertCount = 0;
static GLuint g_PortalSurfaceVAO = 0;       // 正面发光效果
static int g_PortalSurfaceVertCount = 0;
static GLuint g_PortalBackVAO = 0;          // 背面不透明遮挡
static int g_PortalBackVertCount = 0;

void CreatePortalVisuals() {
    // Portal frame (outline around each portal)
    std::vector<float> frameVerts;
    float w = PORTAL_WIDTH / 2.0f;
    float h = PORTAL_HEIGHT / 2.0f;
    float frameThickness = 0.15f;
    glm::vec3 frameColorA(0.1f, 0.5f, 1.0f);  // Blue portal
    glm::vec3 frameColorB(1.0f, 0.5f, 0.1f);  // Orange portal
    
    // Create frame geometry (for Portal A - will be transformed)
    // Top bar
    AddBox(frameVerts, glm::vec3(0, h + frameThickness/2, 0), 
           glm::vec3(w * 2 + frameThickness * 2, frameThickness, frameThickness), frameColorA);
    // Bottom bar
    AddBox(frameVerts, glm::vec3(0, -h - frameThickness/2, 0), 
           glm::vec3(w * 2 + frameThickness * 2, frameThickness, frameThickness), frameColorA);
    // Left bar
    AddBox(frameVerts, glm::vec3(-w - frameThickness/2, 0, 0), 
           glm::vec3(frameThickness, h * 2, frameThickness), frameColorA);
    // Right bar
    AddBox(frameVerts, glm::vec3(w + frameThickness/2, 0, 0), 
           glm::vec3(frameThickness, h * 2, frameThickness), frameColorA);
    
    g_PortalFrameVAO = CreateVAOFromVertices(frameVerts);
    g_PortalFrameVertCount = (int)frameVerts.size() / 6;
    
    // ============ 门户表面（用于模板标记和深度清除）============
    // 双面门户：需要从两面都能看到，所以添加正反两面
    std::vector<float> surfaceVerts;
    glm::vec3 surfaceColor(0.3f, 0.6f, 0.9f);
    // 正面 (朝向 +Z)
    AddQuad(surfaceVerts,
        glm::vec3(-w, -h, 0.0f), glm::vec3(w, -h, 0.0f),
        glm::vec3(w, h, 0.0f), glm::vec3(-w, h, 0.0f), surfaceColor);
    // 背面 (朝向 -Z) - 用于双面门户
    AddQuad(surfaceVerts,
        glm::vec3(w, -h, 0.0f), glm::vec3(-w, -h, 0.0f),
        glm::vec3(-w, h, 0.0f), glm::vec3(w, h, 0.0f), surfaceColor);
    
    g_PortalSurfaceVAO = CreateVAOFromVertices(surfaceVerts);
    g_PortalSurfaceVertCount = (int)surfaceVerts.size() / 6;
    
    // ============ 背面不透明遮挡（朝向 -Z）============
    std::vector<float> backVerts;
    glm::vec3 backColor(0.15f, 0.15f, 0.2f);
    AddQuad(backVerts,
        glm::vec3(w, -h, -0.02f), glm::vec3(-w, -h, -0.02f),
        glm::vec3(-w, h, -0.02f), glm::vec3(w, h, -0.02f), backColor);
    
    g_PortalBackVAO = CreateVAOFromVertices(backVerts);
    g_PortalBackVertCount = (int)backVerts.size() / 6;
}

// Shader for portal surface with animated effect
static GLuint g_PortalSurfaceShader = 0;

const char* PORTAL_SURFACE_VS = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vColor;
out vec3 vLocalPos;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
    vLocalPos = aPos;
}
)";

const char* PORTAL_SURFACE_FS = R"(
#version 330 core
in vec3 vColor;
in vec3 vLocalPos;
out vec4 FragColor;
uniform float uTime;
uniform vec3 uPortalColor;

void main() {
    // Create swirling effect
    vec2 uv = vLocalPos.xy;
    float dist = length(uv);
    float angle = atan(uv.y, uv.x);
    
    // Animated ripples
    float ripple = sin(dist * 8.0 - uTime * 3.0) * 0.5 + 0.5;
    float swirl = sin(angle * 3.0 + uTime * 2.0 + dist * 4.0) * 0.5 + 0.5;
    
    // Edge glow
    float edgeFactor = smoothstep(0.8, 1.0, dist / 1.5);
    
    // Combine effects
    vec3 color = uPortalColor * (0.5 + 0.3 * ripple + 0.2 * swirl);
    color += uPortalColor * 0.5 * edgeFactor;
    
    // Add some brightness variation
    float brightness = 0.8 + 0.2 * sin(uTime * 5.0 + dist * 10.0);
    color *= brightness;
    
    // Semi-transparent
    float alpha = 0.7 + 0.2 * ripple;
    
    FragColor = vec4(color, alpha);
}
)";

void CreatePortalSurfaceShader() {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &PORTAL_SURFACE_VS, nullptr);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &PORTAL_SURFACE_FS, nullptr);
    glCompileShader(fs);
    
    g_PortalSurfaceShader = glCreateProgram();
    glAttachShader(g_PortalSurfaceShader, vs);
    glAttachShader(g_PortalSurfaceShader, fs);
    glLinkProgram(g_PortalSurfaceShader);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
}

// ============================================================================
// 天空盒创建和渲染
// ============================================================================
void CreateSkybox() {
    // 创建天空盒着色器
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &SKYBOX_VS, nullptr);
    glCompileShader(vs);
    
    // 检查编译错误
    GLint success;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vs, 512, nullptr, infoLog);
        std::cerr << "Skybox VS compile error: " << infoLog << std::endl;
    }
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &SKYBOX_FS, nullptr);
    glCompileShader(fs);
    
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fs, 512, nullptr, infoLog);
        std::cerr << "Skybox FS compile error: " << infoLog << std::endl;
    }
    
    g_SkyboxShader = glCreateProgram();
    glAttachShader(g_SkyboxShader, vs);
    glAttachShader(g_SkyboxShader, fs);
    glLinkProgram(g_SkyboxShader);
    
    glGetProgramiv(g_SkyboxShader, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(g_SkyboxShader, 512, nullptr, infoLog);
        std::cerr << "Skybox shader link error: " << infoLog << std::endl;
    }
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    // 创建天空盒立方体顶点
    float skyboxVertices[] = {
        // 背面 (-Z)
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        // 左面 (-X)
        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        // 右面 (+X)
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        // 正面 (+Z)
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        // 顶面 (+Y)
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        // 底面 (-Y)
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };
    
    GLuint VBO;
    glGenVertexArrays(1, &g_SkyboxVAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(g_SkyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void RenderSkybox(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, float time) {
    // 禁用深度写入，但保持深度测试
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    
    glUseProgram(g_SkyboxShader);
    
    // 移除视图矩阵的平移分量（天空盒应该始终围绕相机）
    glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(viewMatrix));
    glm::mat4 viewProj = projectionMatrix * viewNoTranslation;
    
    glUniformMatrix4fv(glGetUniformLocation(g_SkyboxShader, "uViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform1f(glGetUniformLocation(g_SkyboxShader, "uTime"), time);
    
    glBindVertexArray(g_SkyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    // 恢复状态
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

// ============================================================================
// 递归门户渲染系统
// ============================================================================

// 最大递归深度（门户中看门户的层数）
const int MAX_PORTAL_RECURSION = 4;

// 计算斜裁剪投影矩阵 - 确保只渲染门户平面后面的内容
glm::mat4 ComputeObliqueProjection(const glm::mat4& projection, const glm::vec4& clipPlane) {
    glm::mat4 result = projection;
    
    // 计算裁剪平面在投影空间中的位置
    glm::vec4 q;
    q.x = (glm::sign(clipPlane.x) + result[2][0]) / result[0][0];
    q.y = (glm::sign(clipPlane.y) + result[2][1]) / result[1][1];
    q.z = -1.0f;
    q.w = (1.0f + result[2][2]) / result[3][2];
    
    // 计算缩放后的裁剪平面
    glm::vec4 c = clipPlane * (2.0f / glm::dot(clipPlane, q));
    
    // 修改投影矩阵的第三行
    result[0][2] = c.x;
    result[1][2] = c.y;
    result[2][2] = c.z + 1.0f;
    result[3][2] = c.w;
    
    return result;
}

// 检查门户是否在视锥体内（简化版：检查门户中心是否在摄像机前方）
bool IsPortalVisible(PortalRenderer::Portal* portal, const glm::vec3& cameraPos, const glm::vec3& cameraForward) {
    glm::vec3 portalPos = glm::vec3(portal->transform[3]);
    glm::vec3 toPortal = portalPos - cameraPos;
    
    // 门户必须在摄像机前方
    if (glm::dot(toPortal, cameraForward) < -1.0f) {
        return false;
    }
    
    // 门户距离不能太远（优化性能）
    if (glm::length(toPortal) > 100.0f) {
        return false;
    }
    
    return true;
}

// 检查是否从门户正面观察（双面门户不需要这个判断，但返回哪一面）
// 返回 1 表示正面，-1 表示背面
int GetPortalViewingSide(PortalRenderer::Portal* portal, const glm::vec3& cameraPos) {
    glm::vec3 portalPos = glm::vec3(portal->transform[3]);
    glm::vec3 portalNormal = glm::vec3(portal->transform * glm::vec4(0, 0, 1, 0));
    glm::vec3 toCamera = cameraPos - portalPos;
    return glm::dot(toCamera, portalNormal) > 0.0f ? 1 : -1;
}

// 前向声明
void RenderPortalFramesExcluding(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, 
                                  float time, PortalRenderer::Portal* excludePortal);

// 渲染单个门户的内容（递归，支持双面门户）
// viewingSide: 1 = 从正面观察, -1 = 从背面观察
void RenderPortalContent(PortalRenderer::Portal* portal, 
                         const glm::mat4& viewMatrix, 
                         const glm::mat4& projectionMatrix,
                         const glm::vec3& cameraPos,
                         int recursionLevel,
                         int stencilValue,
                         float currentTime,
                         int viewingSide = 1);

// 递归渲染所有门户（双面门户版本）
// excludePortal: 排除的门户（正在通过的门户，避免在递归中重复渲染）
void RenderPortalsRecursive(const glm::mat4& viewMatrix, 
                            const glm::mat4& projectionMatrix,
                            const glm::vec3& cameraPos,
                            const glm::vec3& cameraForward,
                            int recursionLevel,
                            int stencilValue,
                            float currentTime,
                            PortalRenderer::Portal* excludePortal = nullptr) {
    if (recursionLevel >= MAX_PORTAL_RECURSION) {
        return;
    }
    
    for (size_t i = 0; i < g_Portals.size(); i++) {
        PortalRenderer::Portal* portal = g_Portals[i];
        
        if (!portal->isActive || !portal->linkedPortal) continue;
        
        // 关键修复：排除当前正在通过的门户对
        // 当通过门户A看时，不应该再在L1中渲染门户A或门户B
        // 因为我们已经"在"门户B的出口了，再次渲染会导致位置错误
        if (excludePortal != nullptr) {
            if (portal == excludePortal || portal == excludePortal->linkedPortal) {
                continue;
            }
        }
        
        // 从当前视图矩阵提取实际相机位置（支持递归层级）
        glm::vec3 actualCameraPos = glm::vec3(glm::inverse(viewMatrix)[3]);
        
        // 检查门户可见性（使用实际相机位置）
        if (!IsPortalVisible(portal, actualCameraPos, cameraForward)) continue;
        
        // 双面门户：从两面都可以看到对面场景
        // 获取当前观察的是哪一面（使用实际相机位置）
        int viewingSide = GetPortalViewingSide(portal, actualCameraPos);
        
        // 为这个门户渲染内容（传递观察方向，并传递排除门户以供下一层递归使用）
        RenderPortalContent(portal, viewMatrix, projectionMatrix, actualCameraPos, 
                           recursionLevel, stencilValue + (int)i + 1, currentTime, viewingSide);
    }
}

// 调试标志 - 每秒只输出一次
static float g_LastDebugTime = 0.0f;
static bool g_DebugThisFrame = false;

void RenderPortalContent(PortalRenderer::Portal* portal, 
                         const glm::mat4& viewMatrix, 
                         const glm::mat4& projectionMatrix,
                         const glm::vec3& cameraPos,
                         int recursionLevel,
                         int stencilValue,
                         float currentTime,
                         int viewingSide) {
    
    // RenderDoc 调试标记
    char debugName[128];
    glm::vec3 portalPos = glm::vec3(portal->transform[3]);
    snprintf(debugName, sizeof(debugName), "Portal L%d @ (%.1f, %.1f, %.1f) Stencil=%d", 
             recursionLevel, portalPos.x, portalPos.y, portalPos.z, stencilValue);
    PushDebugGroup(debugName);
    
    PortalRenderer::Portal* destPortal = portal->linkedPortal;
    
    // 双面门户：根据观察方向调整入口门户的变换
    // 如果从背面观察，我们需要翻转门户的法线
    glm::mat4 effectiveSrcTransform = portal->transform;
    if (viewingSide < 0) {
        // 从背面观察：翻转门户（绕Y轴旋转180度）
        effectiveSrcTransform = portal->transform * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0, 1, 0));
    }
    
    // 调试输出（所有递归层级）
    if (g_DebugThisFrame) {
        glm::vec3 portalPos = glm::vec3(portal->transform[3]);
        glm::vec3 destPos = glm::vec3(destPortal->transform[3]);
        glm::vec3 portalNormal = glm::vec3(portal->transform * glm::vec4(0, 0, 1, 0));
        std::cout << "[Portal Debug L" << recursionLevel << "] Rendering portal at (" << portalPos.x << ", " << portalPos.y << ", " << portalPos.z << ")" << std::endl;
        std::cout << "  -> Normal: (" << portalNormal.x << ", " << portalNormal.y << ", " << portalNormal.z << ")" << std::endl;
        std::cout << "  -> Viewing side: " << (viewingSide > 0 ? "FRONT" : "BACK") << std::endl;
        std::cout << "  -> Destination: (" << destPos.x << ", " << destPos.y << ", " << destPos.z << ")" << std::endl;
        std::cout << "  -> Camera pos (passed): (" << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")" << std::endl;
        std::cout << "  -> Stencil value: " << stencilValue << std::endl;
        // 从view matrix提取相机位置
        glm::vec3 camFromView = glm::vec3(glm::inverse(viewMatrix)[3]);
        std::cout << "  -> Camera pos (from viewMatrix): (" << camFromView.x << ", " << camFromView.y << ", " << camFromView.z << ")" << std::endl;
    }
    
    // ========== 第1步：使用模板缓冲标记门户区域 ==========
    glEnable(GL_STENCIL_TEST);
    
    // 配置模板测试：只在当前递归层级的区域内绘制
    if (recursionLevel == 0) {
        glStencilFunc(GL_ALWAYS, stencilValue, 0xFF);
    } else {
        glStencilFunc(GL_EQUAL, stencilValue - 1, 0xFF);
    }
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilMask(0xFF);  // 确保模板可写
    
    // 禁用颜色和深度写入，只写入模板
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    
    // 禁用背面剔除，确保门户quad可以被绘制
    glDisable(GL_CULL_FACE);
    
    // 绘制门户形状到模板缓冲
    glUseProgram(g_SceneShader);
    glm::mat4 portalMVP = projectionMatrix * viewMatrix * portal->transform;
    glUniformMatrix4fv(glGetUniformLocation(g_SceneShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(portalMVP));
    
    glBindVertexArray(g_PortalSurfaceVAO);
    glDrawArrays(GL_TRIANGLES, 0, g_PortalSurfaceVertCount);
    
    // 恢复状态
    glEnable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    
    // ========== 第2步：计算虚拟相机 ==========
    // 使用 PortalMath 库计算正确的虚拟视图矩阵
    // 关键：从入口门户看进去，应该看到出口门户背后的场景
    glm::mat4 virtualViewMatrix = PortalMath::CalculatePortalViewMatrix(
        viewMatrix, portal->transform, destPortal->transform);
    
    // 计算门户变换矩阵（用于将任何位置从入口侧映射到出口侧）
    glm::mat4 portalTransform = PortalMath::ComputePortalTransform(portal->transform, destPortal->transform);
    
    // 从当前视图矩阵中提取相机位置（更准确，支持递归）
    glm::vec3 currentCameraPos = glm::vec3(glm::inverse(viewMatrix)[3]);
    
    // 计算虚拟相机位置（用于可见性检测和递归）
    glm::vec3 virtualCameraPos = glm::vec3(portalTransform * glm::vec4(currentCameraPos, 1.0f));
    
    // 调试：输出虚拟相机位置
    if (g_DebugThisFrame && recursionLevel == 0) {
        glm::vec3 origCamPos = glm::vec3(glm::inverse(viewMatrix)[3]);
        glm::vec3 virtCamPos = glm::vec3(glm::inverse(virtualViewMatrix)[3]);
        std::cout << "  -> Original camera pos: (" << origCamPos.x << ", " << origCamPos.y << ", " << origCamPos.z << ")" << std::endl;
        std::cout << "  -> Virtual camera pos: (" << virtCamPos.x << ", " << virtCamPos.y << ", " << virtCamPos.z << ")" << std::endl;
    }
    
    // ========== 第3步：计算斜裁剪投影矩阵 ==========
    // 使用出口门户平面作为近裁剪平面，避免渲染门户背后的物体
    // 这是门户渲染的关键：只渲染出口门户前方的内容
    glm::mat4 virtualProjection = projectionMatrix;
    
    // 获取出口门户的裁剪平面（在虚拟相机视图空间中）
    // 裁剪平面应该位于出口门户的位置，法线指向门户正面
    glm::vec3 destPortalPos = glm::vec3(destPortal->transform[3]);
    glm::vec3 destPortalNormal = glm::normalize(glm::vec3(destPortal->transform * glm::vec4(0, 0, 1, 0)));
    
    // 将裁剪平面变换到虚拟相机的视图空间
    glm::vec3 clipPosView = glm::vec3(virtualViewMatrix * glm::vec4(destPortalPos, 1.0f));
    glm::vec3 clipNormalView = glm::normalize(glm::vec3(virtualViewMatrix * glm::vec4(destPortalNormal, 0.0f)));
    
    // 确保裁剪平面法线指向相机（即我们要保留平面前方的内容）
    // 如果法线背对相机，翻转它
    if (clipNormalView.z > 0.0f) {
        clipNormalView = -clipNormalView;
    }
    
    // 构建裁剪平面方程 (Ax + By + Cz + D = 0)
    // D = -dot(normal, point)
    float clipD = -glm::dot(clipNormalView, clipPosView);
    glm::vec4 clipPlane(clipNormalView.x, clipNormalView.y, clipNormalView.z, clipD);
    
    // 轻微向后偏移裁剪平面，避免精度问题导致的闪烁
    clipPlane.w -= 0.01f;
    
    // 检查裁剪平面是否有效 - 当从极端角度观察时，裁剪平面可能会导致数值问题
    // 如果裁剪平面的 z 分量太小（几乎平行于视线），需要特殊处理
    bool useObliqueClipping = true;
    bool skipPortalRender = false;
    
    // 检查1：裁剪平面几乎平行于视线（z分量太小）
    if (glm::abs(clipNormalView.z) < 0.05f) {
        // 极端角度 - 尝试使用基于距离的近平面
        useObliqueClipping = false;
    }
    
    // 检查2：裁剪平面在相机后方或太近
    // clipD 正值表示相机在平面的正面（平面法线指向的一侧）
    // 由于我们翻转了法线使其指向相机，clipD 应该是负值（平面在相机前方）
    // clipD = -dot(normal, pos)，如果 pos 在 normal 方向上，则为负
    if (clipD > -0.01f) {
        // 门户平面在相机后方或相机正好在平面上
        // 不能使用斜裁剪，也不能正确渲染
        skipPortalRender = true;
    }
    
    // 如果需要跳过渲染，直接退出
    if (skipPortalRender) {
        glDisable(GL_STENCIL_TEST);
        return;
    }
    
    // 应用裁剪策略
    if (useObliqueClipping) {
        // 正常情况：使用斜裁剪投影矩阵
        virtualProjection = PortalMath::CalculateObliqueProjectionMatrix(projectionMatrix, clipPlane);
    } else {
        // 极端角度但门户仍在前方：使用基于门户距离的近平面
        // 门户平面距离（在视图空间中，z轴负方向为前方）
        float portalDist = -clipPosView.z;  // 转换为正值（门户在前方时 z < 0）
        
        if (portalDist < 0.1f) {
            // 门户太近或在后方，跳过渲染
            glDisable(GL_STENCIL_TEST);
            return;
        }
        
        // 使用门户距离作为近平面（稍微减小以避免裁剪门户本身）
        float safeNearPlane = glm::max(0.01f, portalDist * 0.9f);
        
        // 从原投影矩阵提取参数
        float fov = 2.0f * glm::atan(1.0f / projectionMatrix[1][1]);
        float aspect = projectionMatrix[1][1] / projectionMatrix[0][0];
        float farPlane = 100.0f;
        
        virtualProjection = glm::perspective(fov, aspect, safeNearPlane, farPlane);
    }
    
    if (g_DebugThisFrame && recursionLevel == 0) {
        std::cout << "  -> Clip plane (view space): (" << clipPlane.x << ", " << clipPlane.y << ", " << clipPlane.z << ", " << clipPlane.w << ")" << std::endl;
    }
    
    // ========== 第4步：清除门户区域的深度和颜色缓冲 ==========
    glStencilFunc(GL_EQUAL, stencilValue, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0x00);  // 不修改模板值
    
    // 4a: 先清除深度缓冲（将深度设为远平面）
    glDisable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);
    glDepthRange(1.0, 1.0);  // 强制写入远平面深度
    
    glUseProgram(g_SceneShader);
    glm::mat4 clearMVP = projectionMatrix * viewMatrix * portal->transform;
    glUniformMatrix4fv(glGetUniformLocation(g_SceneShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(clearMVP));
    
    glBindVertexArray(g_PortalSurfaceVAO);
    glDrawArrays(GL_TRIANGLES, 0, g_PortalSurfaceVertCount);
    
    // 恢复状态
    glDepthRange(0.0, 1.0);
    glDepthFunc(GL_LESS);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_CULL_FACE);
    glStencilMask(0xFF);
    
    // ========== 第5步：渲染门户另一侧的场景（先天空盒，后几何体）==========
    // 关键：必须保持模板测试启用，确保只渲染到门户区域内
    // 模板测试已经在第4步设置为 GL_EQUAL stencilValue
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, stencilValue, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0x00);  // 不修改模板值
    
    // 先渲染天空盒作为背景
    RenderSkybox(virtualViewMatrix, virtualProjection, currentTime);
    
    // 再渲染场景几何体
    RenderScene(virtualViewMatrix, virtualProjection);
    
    // 渲染门户边框（作为场景的一部分，使用虚拟视图矩阵）
    // 但要排除当前正在通过的门户对的边框
    RenderPortalFramesExcluding(virtualViewMatrix, virtualProjection, currentTime, portal);
    
    // ========== 第6步：递归渲染更深层的门户 ==========
    // 从虚拟视图矩阵中提取相机前向向量（视图矩阵的第三行取反）
    glm::mat4 invVirtualView = glm::inverse(virtualViewMatrix);
    glm::vec3 virtualCameraForward = -glm::normalize(glm::vec3(invVirtualView[2]));
    
    // 关键修复：在递归时排除当前门户对
    // 当通过门户A->B时，在L1层级不应该再渲染A或B
    // 这样可以避免在不存在门户的位置看到错误的门户
    RenderPortalsRecursive(virtualViewMatrix, virtualProjection, virtualCameraPos, 
                          virtualCameraForward, recursionLevel + 1, stencilValue, currentTime, portal);
    
    // ========== 第6.5步：封住门户深度 ==========
    // 关键修复：渲染完门户内容后，将门户表面的深度写入深度缓冲
    // 这样后续渲染的其他门户（如Portal 2）就不会覆盖当前门户（Portal 1）
    // 门户表面作为一个"实体"存在于场景中，后面的内容会被它遮挡
    PushDebugGroup("Seal Portal Depth");
    
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, stencilValue, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0x00);  // 不修改模板值
    
    // 只写入深度，不写入颜色
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);  // 强制写入深度
    glDisable(GL_CULL_FACE);
    
    // 使用当前视图矩阵（不是虚拟视图矩阵）绘制门户表面
    // 这样深度值对应门户在主场景中的真实位置
    glUseProgram(g_SceneShader);
    glm::mat4 sealMVP = projectionMatrix * viewMatrix * portal->transform;
    glUniformMatrix4fv(glGetUniformLocation(g_SceneShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(sealMVP));
    
    glBindVertexArray(g_PortalSurfaceVAO);
    glDrawArrays(GL_TRIANGLES, 0, g_PortalSurfaceVertCount);
    
    // 恢复状态
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glStencilMask(0xFF);
    
    PopDebugGroup();  // Seal Portal Depth
    
    // ========== 第7步：恢复模板状态 ==========
    glStencilFunc(GL_EQUAL, recursionLevel == 0 ? 0 : stencilValue - 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    
    glDisable(GL_STENCIL_TEST);
    
    PopDebugGroup(); // Portal content
}

// 渲染门户边框（排除特定门户对）
// 用于递归渲染时，避免在门户内部看到正在穿越的门户对的边框
void RenderPortalFramesExcluding(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, 
                                  float time, PortalRenderer::Portal* excludePortal) {
    glUseProgram(g_SceneShader);
    
    int renderedCount = 0;
    for (size_t i = 0; i < g_Portals.size(); i++) {
        PortalRenderer::Portal* portal = g_Portals[i];
        
        // 排除当前门户对（入口门户及其链接的出口门户）
        if (excludePortal != nullptr) {
            if (portal == excludePortal || portal == excludePortal->linkedPortal) {
                if (g_DebugThisFrame) {
                    glm::vec3 pos = glm::vec3(portal->transform[3]);
                    std::cout << "  [FramesExcluding] Skipping portal at (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                }
                continue;
            }
        }
        
        glm::mat4 mvp = projectionMatrix * viewMatrix * portal->transform;
        glUniformMatrix4fv(glGetUniformLocation(g_SceneShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        
        // 渲染门户边框
        glBindVertexArray(g_PortalFrameVAO);
        glDrawArrays(GL_TRIANGLES, 0, g_PortalFrameVertCount);
        renderedCount++;
    }
    
    if (g_DebugThisFrame && excludePortal != nullptr) {
        std::cout << "  [FramesExcluding] Rendered " << renderedCount << " portal frames" << std::endl;
    }
    
    glBindVertexArray(0);
}

// 渲染门户边框（在所有递归渲染完成后）
// 双面门户：不再渲染背面遮挡板，两面都可以看到对面场景
void RenderPortalFrames(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, float time) {
    // 关键修复：门框应该只渲染在主场景区域（模板值为0）
    // 不应该渲染在门户内部区域（模板值非0），否则会覆盖门户内容
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 0, 0xFF);  // 只在模板值为0的区域渲染
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);  // 不修改模板值
    glStencilMask(0x00);
    
    RenderPortalFramesExcluding(viewMatrix, projectionMatrix, time, nullptr);
    
    glDisable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
}

void RenderFrame() {
    PushDebugGroup("Frame");
    
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    
    // 计算相机方向
    glm::vec3 front;
    front.x = cos(glm::radians(g_CameraYaw)) * cos(glm::radians(g_CameraPitch));
    front.y = sin(glm::radians(g_CameraPitch));
    front.z = sin(glm::radians(g_CameraYaw)) * cos(glm::radians(g_CameraPitch));
    front = glm::normalize(front);
    
    glm::mat4 viewMatrix = glm::lookAt(g_CameraPosition, g_CameraPosition + front, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(60.0f), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 1000.0f);
    
    float currentTime = (float)glfwGetTime();
    
    // 设置调试标志 - 每2秒输出一次
    g_DebugThisFrame = (currentTime - g_LastDebugTime > 2.0f);
    if (g_DebugThisFrame) {
        g_LastDebugTime = currentTime;
        std::cout << "\n=== Frame Debug @ " << currentTime << "s ===" << std::endl;
        std::cout << "Camera: (" << g_CameraPosition.x << ", " << g_CameraPosition.y << ", " << g_CameraPosition.z << ")" << std::endl;
        std::cout << "Looking: (" << front.x << ", " << front.y << ", " << front.z << ")" << std::endl;
    }
    
    // ============ 第1步：渲染主场景 ============
    PushDebugGroup("1. Main Scene");
    RenderScene(viewMatrix, projectionMatrix);
    PopDebugGroup();
    
    // ============ 第2步：渲染天空盒（作为背景，在场景之后渲染）============
    // 使用 GL_LEQUAL 深度测试，天空盒只渲染在没有场景几何体的地方
    PushDebugGroup("2. Main Skybox");
    RenderSkybox(viewMatrix, projectionMatrix, currentTime);
    PopDebugGroup();
    
    // ============ 第3步：递归渲染门户内容 ============
    // 使用模板缓冲实现真正的"透视"效果
    PushDebugGroup("3. Portal Recursive Rendering");
    RenderPortalsRecursive(viewMatrix, projectionMatrix, g_CameraPosition, front, 0, 0, currentTime);
    PopDebugGroup();
    
    // ============ 第4步：渲染门户边框 ============
    PushDebugGroup("4. Portal Frames (Main View)");
    RenderPortalFrames(viewMatrix, projectionMatrix, currentTime);
    PopDebugGroup();
    
    PopDebugGroup(); // Frame
}

void Cleanup() {
    for (PortalRenderer::Portal* portal : g_Portals) {
        PortalRenderer::DestroyPortal(portal);
        delete portal;
    }
    g_Portals.clear();
}

// Input handling
double lastX = WINDOW_WIDTH / 2.0, lastY = WINDOW_HEIGHT / 2.0;
bool firstMouse = true;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = (float)(xpos - lastX);
    float yoffset = (float)(lastY - ypos);
    lastX = xpos; lastY = ypos;
    g_CameraYaw += xoffset * 0.1f;
    g_CameraPitch += yoffset * 0.1f;
    if (g_CameraPitch > 89.0f) g_CameraPitch = 89.0f;
    if (g_CameraPitch < -89.0f) g_CameraPitch = -89.0f;
}

void processInput(GLFWwindow* window, float deltaTime) {
    float speed = 5.0f * deltaTime;
    glm::vec3 front;
    front.x = cos(glm::radians(g_CameraYaw));
    front.y = 0.0f;
    front.z = sin(glm::radians(g_CameraYaw));
    front = glm::normalize(front);
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) g_CameraPosition += front * speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) g_CameraPosition -= front * speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) g_CameraPosition -= right * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) g_CameraPosition += right * speed;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
}

int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed!" << std::endl; return -1; }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Portal Rendering Demo", nullptr, nullptr);
    if (!window) { std::cerr << "Window creation failed!" << std::endl; glfwTerminate(); return -1; }
    
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "GLEW init failed!" << std::endl; return -1; }
    
    // Create scene shader
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &SCENE_VS, nullptr);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &SCENE_FS, nullptr);
    glCompileShader(fs);
    g_SceneShader = glCreateProgram();
    glAttachShader(g_SceneShader, vs);
    glAttachShader(g_SceneShader, fs);
    glLinkProgram(g_SceneShader);
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    CreateSceneGeometry();
    CreatePortalVisuals();
    CreatePortalSurfaceShader();
    CreateSkybox();
    SetupPortals();
    SetupPlayer();
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    float lastTime = (float)glfwGetTime();
    
    std::cout << "Controls: WASD to move, Mouse to look, ESC to exit" << std::endl;
    
    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        
        glfwPollEvents();
        processInput(window, deltaTime);
        UpdatePlayer(deltaTime, currentTime);
        RenderFrame();
        glfwSwapBuffers(window);
    }
    
    Cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
