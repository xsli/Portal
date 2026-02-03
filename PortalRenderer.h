/**
 * PortalRenderer.h - 门户渲染器
 * 
 * 包含Portal结构体定义和渲染函数声明
 */

#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <functional>

#include "PortalMath.h"

namespace PortalRenderer {

// 渲染上下文：封装每帧渲染所需的信息
struct RenderContext {
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    glm::vec3 cameraPosition;
    glm::vec3 cameraForward;
    int screenWidth;
    int screenHeight;
};

// 前向声明Portal结构体供linkedPortal指针使用
struct Portal;

// Portal核心结构体
struct Portal {
    // 门户变换矩阵 (position/rotation encoded in this)
    glm::mat4 transform;
    
    // 门户几何尺寸
    float width = 2.0f;
    float height = 3.0f;
    
    // 链接的目标门户
    Portal* linkedPortal = nullptr;
    
    // OpenGL渲染资源
    GLuint meshVAO = 0;
    GLuint meshVBO = 0;
    GLuint meshEBO = 0;
    
    // 用于渲染到纹理的FBO和纹理
    GLuint renderFBO = 0;
    GLuint renderTexture = 0;
    GLuint renderDepthBuffer = 0;
    int textureWidth = 1024;
    int textureHeight = 1024;
    
    // 门户着色器程序
    GLuint shaderProgram = 0;
    
    // 是否激活
    bool isActive = true;
    
    // 辅助方法获取门户位置和法线
    glm::vec3 GetPosition() const {
        return glm::vec3(transform[3]);
    }
    
    glm::vec3 GetNormal() const {
        return glm::normalize(glm::vec3(transform[2])); // Z轴作为法线
    }
    
    glm::vec3 GetUp() const {
        return glm::normalize(glm::vec3(transform[1]));
    }
    
    glm::vec3 GetRight() const {
        return glm::normalize(glm::vec3(transform[0]));
    }
};

// ============================================================================
//                          常量定义
// ============================================================================
constexpr int MAX_PORTAL_RECURSION = 4;

// ============================================================================
//                          门户渲染资源创建
// ============================================================================

/**
 * 创建门户四边形网格的VAO/VBO/EBO
 */
inline void CreatePortalMesh(Portal* portal) {
    float hw = portal->width * 0.5f;
    float hh = portal->height * 0.5f;
    
    // 顶点数据: position (3), normal (3), uv (2)
    float vertices[] = {
        // pos              // normal     // uv
        -hw, -hh, 0.0f,     0, 0, 1,      0, 0,
         hw, -hh, 0.0f,     0, 0, 1,      1, 0,
         hw,  hh, 0.0f,     0, 0, 1,      1, 1,
        -hw,  hh, 0.0f,     0, 0, 1,      0, 1
    };
    
    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };
    
    glGenVertexArrays(1, &portal->meshVAO);
    glGenBuffers(1, &portal->meshVBO);
    glGenBuffers(1, &portal->meshEBO);
    
    glBindVertexArray(portal->meshVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, portal->meshVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, portal->meshEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // UV attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

/**
 * 创建门户渲染目标（FBO + Color Texture + Depth Buffer）
 */
inline void CreatePortalRenderTarget(Portal* portal, int width, int height) {
    portal->textureWidth = width;
    portal->textureHeight = height;
    
    // 创建FBO
    glGenFramebuffers(1, &portal->renderFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, portal->renderFBO);
    
    // 创建颜色纹理
    glGenTextures(1, &portal->renderTexture);
    glBindTexture(GL_TEXTURE_2D, portal->renderTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, portal->renderTexture, 0);
    
    // 创建深度/模板 Renderbuffer
    glGenRenderbuffers(1, &portal->renderDepthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, portal->renderDepthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, portal->renderDepthBuffer);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // 错误处理: FBO不完整
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
 * 销毁门户资源
 */
inline void DestroyPortal(Portal* portal) {
    if (portal->meshVAO) {
        glDeleteVertexArrays(1, &portal->meshVAO);
        glDeleteBuffers(1, &portal->meshVBO);
        glDeleteBuffers(1, &portal->meshEBO);
    }
    if (portal->renderFBO) {
        glDeleteFramebuffers(1, &portal->renderFBO);
        glDeleteTextures(1, &portal->renderTexture);
        glDeleteRenderbuffers(1, &portal->renderDepthBuffer);
    }
    if (portal->shaderProgram) {
        glDeleteProgram(portal->shaderProgram);
    }
}

// ============================================================================
//                          门户着色器
// ============================================================================

// 门户顶点着色器源码
inline const char* GetPortalVertexShaderSource() {
    return R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 vWorldPos;
out vec3 vNormal;
out vec4 vClipPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vClipPos = uProjection * uView * worldPos;
    gl_Position = vClipPos;
}
)";
}

// 门户片段着色器源码
inline const char* GetPortalFragmentShaderSource() {
    return R"(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec4 vClipPos;

out vec4 FragColor;

uniform sampler2D uPortalTexture;
uniform vec3 uCameraPos;
uniform float uTime;

void main() {
    // 计算屏幕空间UV (从clip坐标转换到0-1范围)
    vec2 screenUV = (vClipPos.xy / vClipPos.w) * 0.5 + 0.5;
    
    // 采样门户纹理
    vec4 portalColor = texture(uPortalTexture, screenUV);
    
    // 添加门户边缘效果
    float fresnel = 1.0 - abs(dot(normalize(vNormal), normalize(uCameraPos - vWorldPos)));
    vec3 edgeColor = vec3(0.2, 0.6, 1.0) * fresnel * fresnel;
    
    // 时间扭曲效果
    float wave = sin(uTime * 3.0 + length(screenUV - 0.5) * 20.0) * 0.5 + 0.5;
    edgeColor *= 1.0 + wave * 0.3;
    
    FragColor = portalColor + vec4(edgeColor * 0.3, 0.0);
}
)";
}

/**
 * 编译并链接着色器程序
 */
inline GLuint CompilePortalShader() {
    auto compileShader = [](GLenum type, const char* source) -> GLuint {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            // 错误日志输出
        }
        return shader;
    };
    
    GLuint vertShader = compileShader(GL_VERTEX_SHADER, GetPortalVertexShaderSource());
    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, GetPortalFragmentShaderSource());
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        // 链接错误日志
    }
    
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    
    return program;
}

// ============================================================================
//                   递归门户渲染核心逻辑
// ============================================================================

/**
 * 场景渲染回调函数类型
 * 用户提供此函数来渲染实际场景内容
 */
using SceneRenderCallback = std::function<void(const glm::mat4& view, const glm::mat4& projection)>;

/**
 * 递归渲染单个门户
 * 
 * @param portal          当前门户
 * @param context         渲染上下文
 * @param currentRecursion 当前递归深度
 * @param allPortals      所有门户列表（用于嵌套渲染）
 * @param renderScene     场景渲染回调
 */
inline void RenderPortalRecursive(
    Portal* portal,
    const RenderContext& context,
    int currentRecursion,
    std::vector<Portal*>& allPortals,
    const SceneRenderCallback& renderScene
) {
    if (currentRecursion >= MAX_PORTAL_RECURSION) return;
    if (!portal->linkedPortal || !portal->isActive) return;
    
    // ========================================================================
    // Step 1: 计算通过此门户观看的虚拟相机
    // ========================================================================
    glm::mat4 virtualView = PortalMath::CalculatePortalView(
        portal->transform,
        portal->linkedPortal->transform,
        context.viewMatrix
    );
    
    glm::vec3 virtualCameraPos = glm::vec3(glm::inverse(virtualView)[3]);
    
    // ========================================================================
    // Step 2: 使用斜切近平面的投影矩阵
    // ========================================================================
    // 目标门户平面作为裁剪平面 (在世界空间)
    glm::vec4 portalPlaneWorld = PortalMath::GetPortalPlane(portal->linkedPortal->transform);
    
    glm::mat4 obliqueProj = PortalMath::CalculateObliqueProjection(
        context.projectionMatrix,
        virtualView,
        portalPlaneWorld
    );
    
    // ========================================================================
    // Step 3: 设置模板缓冲区 - 只在门户区域内渲染
    // ========================================================================
    glEnable(GL_STENCIL_TEST);
    
    // 绘制门户形状来设置模板值
    glStencilFunc(GL_EQUAL, currentRecursion, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    
    // 渲染门户四边形到模板缓冲区
    glUseProgram(portal->shaderProgram);
    glBindVertexArray(portal->meshVAO);
    
    // 设置uniforms
    glUniformMatrix4fv(glGetUniformLocation(portal->shaderProgram, "uModel"), 1, GL_FALSE, &portal->transform[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(portal->shaderProgram, "uView"), 1, GL_FALSE, &context.viewMatrix[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(portal->shaderProgram, "uProjection"), 1, GL_FALSE, &context.projectionMatrix[0][0]);
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    // ========================================================================
    // Step 4: 清除门户区域的深度缓冲区
    // ========================================================================
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);
    glStencilFunc(GL_EQUAL, currentRecursion + 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    glDepthFunc(GL_LESS);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    
    // ========================================================================
    // Step 5: 递归渲染其他门户（在当前门户视角下）
    // ========================================================================
    RenderContext nestedContext = context;
    nestedContext.viewMatrix = virtualView;
    nestedContext.projectionMatrix = obliqueProj;
    nestedContext.cameraPosition = virtualCameraPos;
    
    for (Portal* otherPortal : allPortals) {
        if (otherPortal != portal && otherPortal->isActive) {
            RenderPortalRecursive(otherPortal, nestedContext, currentRecursion + 1, allPortals, renderScene);
        }
    }
    
    // ========================================================================
    // Step 6: 渲染通过门户看到的场景
    // ========================================================================
    glStencilFunc(GL_EQUAL, currentRecursion + 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    
    // 调用用户提供的场景渲染函数
    renderScene(virtualView, obliqueProj);
    
    // ========================================================================
    // Step 7: 恢复模板值（递减回原来的层级）
    // ========================================================================
    glStencilFunc(GL_EQUAL, currentRecursion + 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glBindVertexArray(0);
}

/**
 * 渲染所有门户
 */
inline void RenderPortals(
    std::vector<Portal*>& portals,
    const RenderContext& context,
    const SceneRenderCallback& renderScene
) {
    // 清除模板缓冲区为0
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    
    // 对每个门户进行递归渲染
    for (Portal* portal : portals) {
        RenderPortalRecursive(portal, context, 0, portals, renderScene);
    }
    
    // 最后绘制门户边框效果
    glDisable(GL_STENCIL_TEST);
    
    for (Portal* portal : portals) {
        if (!portal->isActive) continue;
        
        glUseProgram(portal->shaderProgram);
        glBindVertexArray(portal->meshVAO);
        
        // 绑定门户纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, portal->renderTexture);
        glUniform1i(glGetUniformLocation(portal->shaderProgram, "uPortalTexture"), 0);
        
        glUniformMatrix4fv(glGetUniformLocation(portal->shaderProgram, "uModel"), 1, GL_FALSE, &portal->transform[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(portal->shaderProgram, "uView"), 1, GL_FALSE, &context.viewMatrix[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(portal->shaderProgram, "uProjection"), 1, GL_FALSE, &context.projectionMatrix[0][0]);
        glUniform3fv(glGetUniformLocation(portal->shaderProgram, "uCameraPos"), 1, &context.cameraPosition[0]);
        
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

} // namespace PortalRenderer
