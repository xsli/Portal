/**
 * PortalTeleporter.h - Portal穿越/传送逻辑
 */

#pragma once

#include "PortalMath.h"
#include "PortalRenderer.h"
#include <cmath>

namespace PortalTeleporter {

struct TeleportableEntity {
    glm::vec3 position;
    glm::vec3 previousPosition;
    glm::vec3 velocity;
    glm::mat4 transform;
    bool isNearPortal = false;
    float lastTeleportTime = 0.0f;
};

inline bool IsPointInPortalBounds(const glm::vec3& worldPoint, const glm::mat4& portalMatrix, float halfWidth, float halfHeight) {
    glm::mat4 portalInverse = glm::inverse(portalMatrix);
    glm::vec4 localPoint = portalInverse * glm::vec4(worldPoint, 1.0f);
    return (std::abs(localPoint.x) <= halfWidth && std::abs(localPoint.y) <= halfHeight);
}

// Helper to get current time - using static counter for portability
inline float GetCurrentTime() {
    static float s_time = 0.0f;
    return s_time;
}

inline void SetCurrentTime(float time) {
    static float s_time = 0.0f;
    s_time = time;
}

// 双面门户传送检测
// 支持从任意一面穿过门户进行传送
inline bool ShouldTeleport(TeleportableEntity& entity, const PortalRenderer::Portal* portal, float halfWidth, float halfHeight, float currentTime = 0.0f) {
    // Cooldown check - prevent rapid teleportation
    const float TELEPORT_COOLDOWN = 0.3f; // 300ms cooldown
    if (currentTime > 0.0f && (currentTime - entity.lastTeleportTime) < TELEPORT_COOLDOWN) {
        return false;
    }
    
    float prevDist = PortalMath::GetSignedDistanceToPortal(entity.previousPosition, portal->transform);
    float currDist = PortalMath::GetSignedDistanceToPortal(entity.position, portal->transform);
    
    // 双面门户：检测是否穿过门户平面（无论从哪一面）
    // 条件：前后帧的符号不同（或其中一个为0）
    bool crossedPortal = (prevDist > 0.0f && currDist <= 0.0f) ||  // 从正面穿到背面
                         (prevDist < 0.0f && currDist >= 0.0f);    // 从背面穿到正面
    
    if (!crossedPortal) return false;
    
    // Calculate the intersection point
    float t = prevDist / (prevDist - currDist);
    glm::vec3 crossPoint = glm::mix(entity.previousPosition, entity.position, t);
    
    if (IsPointInPortalBounds(crossPoint, portal->transform, halfWidth, halfHeight)) {
        entity.lastTeleportTime = currentTime;
        return true;
    }
    return false;
}

inline void TeleportEntity(TeleportableEntity& entity, const PortalRenderer::Portal* sourcePortal, const PortalRenderer::Portal* targetPortal) {
    entity.position = PortalMath::TeleportPosition(entity.position, sourcePortal->transform, targetPortal->transform);
    entity.previousPosition = PortalMath::TeleportPosition(entity.previousPosition, sourcePortal->transform, targetPortal->transform);
    entity.velocity = PortalMath::TeleportDirection(entity.velocity, sourcePortal->transform, targetPortal->transform) * glm::length(entity.velocity);
    entity.transform = PortalMath::TeleportMatrix(entity.transform, sourcePortal->transform, targetPortal->transform);
}

inline glm::mat4 CalculateCloneTransform(const glm::mat4& entityTransform, const PortalRenderer::Portal* portal) {
    if (!portal->linkedPortal) return entityTransform;
    return PortalMath::TeleportMatrix(entityTransform, portal->transform, portal->linkedPortal->transform);
}

inline bool ShouldRenderClone(const TeleportableEntity& entity, const PortalRenderer::Portal* portal, float threshold) {
    float dist = std::abs(PortalMath::GetSignedDistanceToPortal(entity.position, portal->transform));
    return dist < threshold;
}

} // namespace PortalTeleporter
