//
//  ViewportPicking.cpp
//  Cucumber
//

#include "ViewportPicking.h"

#include "EditorCamera.h"
#include "renderer/RenderTarget.h"
#include "renderer/ViewDesc.h"   // CameraData, ScreenPointToRay

#include <algorithm>
#include <cmath>

namespace
{
    bool ReadWorldPosition(const glm::mat4& inverseViewProj, const RenderTarget& target,
                           int pixelX, int pixelYFromTop, glm::vec3& position)
    {
        pixelX = std::clamp(pixelX, 0, target.pixelWidth() - 1);
        pixelYFromTop = std::clamp(pixelYFromTop, 0, target.pixelHeight() - 1);
        const int pixelY = target.pixelHeight() - 1 - pixelYFromTop;

        float depth = 1.0f;
        if (!target.readDepth(pixelX, pixelY, depth) || depth >= 1.0f ||
            !std::isfinite(depth)) {
            return false;
        }

        const float x = 2.0f * (static_cast<float>(pixelX) + 0.5f) /
                        static_cast<float>(target.pixelWidth()) - 1.0f;
        const float y = 2.0f * (static_cast<float>(pixelY) + 0.5f) /
                        static_cast<float>(target.pixelHeight()) - 1.0f;
        const float z = depth * 2.0f - 1.0f;
        const glm::vec4 world = inverseViewProj * glm::vec4(x, y, z, 1.0f);
        if (std::abs(world.w) <= 0.0001f) return false;

        position = glm::vec3(world) / world.w;
        return true;
    }
}

namespace ViewportPicking
{
    bool SurfaceAt(const EditorCamera& camera, const RenderTarget& target,
                   const glm::ivec2& viewportSize, const glm::vec2& mousePosition,
                   glm::vec3& position, glm::vec3& normal)
    {
        normal = glm::vec3(0.0f);
        const CameraData cameraData = camera.build(
            static_cast<float>(viewportSize.x) / static_cast<float>(viewportSize.y));
        const glm::mat4 inverseViewProj = glm::inverse(cameraData.viewProj());
        const int pixelX = static_cast<int>(mousePosition.x * target.pixelWidth() / viewportSize.x);
        const int pixelY = static_cast<int>(mousePosition.y * target.pixelHeight() / viewportSize.y);
        if (!ReadWorldPosition(inverseViewProj, target, pixelX, pixelY, position)) return false;

        glm::vec3 left, right, up, down;
        if (!ReadWorldPosition(inverseViewProj, target, pixelX - 1, pixelY, left) ||
            !ReadWorldPosition(inverseViewProj, target, pixelX + 1, pixelY, right) ||
            !ReadWorldPosition(inverseViewProj, target, pixelX, pixelY - 1, up) ||
            !ReadWorldPosition(inverseViewProj, target, pixelX, pixelY + 1, down)) return true;

        const glm::vec3 horizontal = right - left;
        const glm::vec3 vertical = down - up;
        const glm::vec3 cross = glm::cross(horizontal, vertical);
        if (glm::dot(cross, cross) < 1e-8f) return true;

        const float centerDepth = -glm::vec3(cameraData.view * glm::vec4(position, 1.0f)).z;
        const float edgeDepth = std::max({
            std::abs(-glm::vec3(cameraData.view * glm::vec4(left, 1.0f)).z - centerDepth),
            std::abs(-glm::vec3(cameraData.view * glm::vec4(right, 1.0f)).z - centerDepth),
            std::abs(-glm::vec3(cameraData.view * glm::vec4(up, 1.0f)).z - centerDepth),
            std::abs(-glm::vec3(cameraData.view * glm::vec4(down, 1.0f)).z - centerDepth)
        });
        if (edgeDepth > std::max(8.0f, std::abs(centerDepth) * 0.1f)) return true;

        normal = glm::normalize(cross);
        if (glm::dot(normal, -cameraData.forward) < 0.0f) {
            normal = -normal;
        }
        return true;
    }

    glm::vec3 CreationPosition(const EditorCamera& camera, const RenderTarget& target,
                               const glm::ivec2& viewportSize, const glm::vec2& mousePosition)
    {
        glm::vec3 position;
        glm::vec3 normal;
        if (SurfaceAt(camera, target, viewportSize, mousePosition, position, normal)) {
            return position;
        }

        const CameraData cameraData = camera.build(
            static_cast<float>(viewportSize.x) / static_cast<float>(viewportSize.y));

        glm::vec3 rayStart;
        glm::vec3 rayDirection;
        ScreenPointToRay(cameraData, viewportSize, mousePosition, rayStart, rayDirection);

        if (!cameraData.ortho) return cameraData.position + rayDirection * 64.0f;

        const float forwardDistance = glm::dot(cameraData.forward,
            cameraData.position + cameraData.forward * 64.0f - rayStart);
        const float rayForward = glm::dot(rayDirection, cameraData.forward);
        return rayStart + rayDirection * (forwardDistance / rayForward);
    }
}
