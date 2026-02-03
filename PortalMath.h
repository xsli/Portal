/**
 * PortalMath.h
 * 
 * 非欧几里得空间传送门核心数学库
 * 
 * 数学约定：
 * - 使用列主序矩阵（OpenGL标准）
 * - 右手坐标系：X右，Y上，Z朝向观察者
 * - Portal的正面（可见面）朝向其局部 +Z 方向（即玩家应该从 +Z 方向看向门户）
 * - 当玩家从正面穿过门户时触发传送
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace PortalMath {

/**
 * 获取门户的正面法线（世界空间）
 * 门户正面指向 +Z 方向（局部空间）
 */
inline glm::vec3 GetPortalForward(const glm::mat4& portalMatrix) {
    return glm::normalize(glm::vec3(portalMatrix[2]));
}

/**
 * 计算Portal视图矩阵
 * 
 * 核心思想：当玩家站在 SourcePortal 前观察时，需要看到 TargetPortal 背后的场景。
 * VirtualCamera = TargetPortal × Rotate180 × Inverse(SourcePortal) × PlayerCamera
 * PortalViewMatrix = Inverse(VirtualCamera)
 */
inline glm::mat4 CalculatePortalViewMatrix(
    const glm::mat4& playerViewMatrix,
    const glm::mat4& sourcePortalMatrix,
    const glm::mat4& targetPortalMatrix)
{
    // 180度Y轴旋转（Portal A 和 Portal B 是"对视"的关系）
    glm::mat4 rotate180 = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    
    // 从视图矩阵恢复摄像机的世界变换
    glm::mat4 playerCameraMatrix = glm::inverse(playerViewMatrix);
    
    // 计算虚拟摄像机的世界位置
    glm::mat4 sourcePortalInverse = glm::inverse(sourcePortalMatrix);
    glm::mat4 virtualCameraMatrix = targetPortalMatrix * rotate180 * sourcePortalInverse * playerCameraMatrix;
    
    return glm::inverse(virtualCameraMatrix);
}

/**
 * 从Portal变换矩阵提取裁剪平面（视图空间）
 */
inline glm::vec4 GetPortalClipPlane(
    const glm::mat4& portalMatrix,
    const glm::mat4& viewMatrix)
{
    glm::vec3 portalNormal = -glm::vec3(portalMatrix[2]);
    glm::vec3 portalPosition = glm::vec3(portalMatrix[3]);
    float d = -glm::dot(portalNormal, portalPosition);
    glm::vec4 worldPlane = glm::vec4(portalNormal, d);
    
    glm::mat4 viewInverseTranspose = glm::transpose(glm::inverse(viewMatrix));
    return viewInverseTranspose * worldPlane;
}

/**
 * 计算斜裁剪投影矩阵（Eric Lengyel算法）
 */
inline glm::mat4 CalculateObliqueProjectionMatrix(
    const glm::mat4& projectionMatrix,
    const glm::vec4& clipPlane)
{
    glm::mat4 obliqueProjMatrix = projectionMatrix;
    
    glm::vec4 q;
    q.x = (glm::sign(clipPlane.x) + projectionMatrix[2][0]) / projectionMatrix[0][0];
    q.y = (glm::sign(clipPlane.y) + projectionMatrix[2][1]) / projectionMatrix[1][1];
    q.z = -1.0f;
    q.w = (1.0f + projectionMatrix[2][2]) / projectionMatrix[3][2];
    
    glm::vec4 c = clipPlane * (2.0f / glm::dot(clipPlane, q));
    
    obliqueProjMatrix[0][2] = c.x - obliqueProjMatrix[0][3];
    obliqueProjMatrix[1][2] = c.y - obliqueProjMatrix[1][3];
    obliqueProjMatrix[2][2] = c.z - obliqueProjMatrix[2][3];
    obliqueProjMatrix[3][2] = c.w - obliqueProjMatrix[3][3];
    
    return obliqueProjMatrix;
}

/**
 * 检测点到Portal的有符号距离
 */
inline float GetSignedDistanceToPortal(
    const glm::vec3& point,
    const glm::mat4& portalMatrix)
{
    glm::vec3 portalNormal = -glm::vec3(portalMatrix[2]);
    glm::vec3 portalPosition = glm::vec3(portalMatrix[3]);
    return glm::dot(point - portalPosition, portalNormal);
}

/**
 * 传送位置
 */
inline glm::vec3 TeleportPosition(
    const glm::vec3& worldPosition,
    const glm::mat4& sourcePortalMatrix,
    const glm::mat4& targetPortalMatrix)
{
    glm::mat4 rotate180 = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 teleportMatrix = targetPortalMatrix * rotate180 * glm::inverse(sourcePortalMatrix);
    glm::vec4 result = teleportMatrix * glm::vec4(worldPosition, 1.0f);
    return glm::vec3(result);
}

/**
 * 传送方向向量
 */
inline glm::vec3 TeleportDirection(
    const glm::vec3& worldDirection,
    const glm::mat4& sourcePortalMatrix,
    const glm::mat4& targetPortalMatrix)
{
    glm::mat4 rotate180 = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 teleportMatrix = targetPortalMatrix * rotate180 * glm::inverse(sourcePortalMatrix);
    glm::vec4 result = teleportMatrix * glm::vec4(worldDirection, 0.0f);
    return glm::normalize(glm::vec3(result));
}

/**
 * 传送完整变换矩阵
 */
inline glm::mat4 TeleportMatrix(
    const glm::mat4& worldMatrix,
    const glm::mat4& sourcePortalMatrix,
    const glm::mat4& targetPortalMatrix)
{
    glm::mat4 rotate180 = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 teleportMatrix = targetPortalMatrix * rotate180 * glm::inverse(sourcePortalMatrix);
    return teleportMatrix * worldMatrix;
}

/**
 * CalculatePortalView - 兼容别名
 * 与 CalculatePortalViewMatrix 相同功能，参数顺序调整
 */
inline glm::mat4 CalculatePortalView(
    const glm::mat4& sourcePortalMatrix,
    const glm::mat4& targetPortalMatrix,
    const glm::mat4& playerViewMatrix)
{
    return CalculatePortalViewMatrix(playerViewMatrix, sourcePortalMatrix, targetPortalMatrix);
}

/**
 * ComputePortalTransform - 计算从入口门户到出口门户的变换矩阵
 * 用于将世界空间中的位置/方向从入口门户变换到出口门户
 */
inline glm::mat4 ComputePortalTransform(
    const glm::mat4& sourcePortalMatrix,
    const glm::mat4& targetPortalMatrix)
{
    glm::mat4 rotate180 = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    return targetPortalMatrix * rotate180 * glm::inverse(sourcePortalMatrix);
}

/**
 * GetPortalPlane - 获取世界空间中的Portal平面方程
 * 返回 (A, B, C, D) 满足 Ax + By + Cz + D = 0
 */
inline glm::vec4 GetPortalPlane(const glm::mat4& portalMatrix)
{
    glm::vec3 portalNormal = -glm::vec3(portalMatrix[2]);
    glm::vec3 portalPosition = glm::vec3(portalMatrix[3]);
    float d = -glm::dot(portalNormal, portalPosition);
    return glm::vec4(portalNormal, d);
}

/**
 * CalculateObliqueProjection - 兼容别名
 * 从世界空间裁剪平面和视图矩阵计算斜裁剪投影
 */
inline glm::mat4 CalculateObliqueProjection(
    const glm::mat4& projectionMatrix,
    const glm::mat4& viewMatrix,
    const glm::vec4& worldPlane)
{
    // 将世界空间平面变换到视图空间
    glm::mat4 viewInverseTranspose = glm::transpose(glm::inverse(viewMatrix));
    glm::vec4 viewSpacePlane = viewInverseTranspose * worldPlane;
    
    // 调用核心斜裁剪算法
    return CalculateObliqueProjectionMatrix(projectionMatrix, viewSpacePlane);
}

} // namespace PortalMath
