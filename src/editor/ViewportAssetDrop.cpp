//
//  ViewportAssetDrop.cpp
//  Cucumber
//

#include "ViewportAssetDrop.h"

#include "AssetsPanel.h"          // AssetDragPayload, AssetType_*
#include "EntityFactory.h"

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace
{
    // false, если путь в payload не завершён нулём
    bool ReadAssetPath(const AssetDragPayload& payload, std::filesystem::path& out)
    {
        const char* pathEnd = std::find(payload.path, payload.path + sizeof(payload.path), '\0');
        if (pathEnd == payload.path + sizeof(payload.path))
        {
            std::cerr << "[Viewport] Cannot drop asset: path is not terminated.\n";
            return false;
        }

        out = std::filesystem::path(payload.path);
        return true;
    }
}

namespace ViewportAssetDrop
{
    Entity* Handle(WorldScene& scene, const ViewportImageRect& image,
                   const std::function<glm::vec3(const glm::vec2&)>& positionAt, float gridStep)
    {
        if (!ImGui::BeginDragDropTarget()) return nullptr;

        Entity* created = nullptr;

        const ImGuiPayload* currentPayload = ImGui::GetDragDropPayload();
        const auto* asset = currentPayload &&
            currentPayload->IsDataType("ASSET_ITEM") &&
            currentPayload->DataSize == sizeof(AssetDragPayload)
            ? static_cast<const AssetDragPayload*>(currentPayload->Data)
            : nullptr;

        if (asset && (asset->type == AssetType_Mesh || asset->type == AssetType_Prefab))
        {
            const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("ASSET_ITEM", ImGuiDragDropFlags_AcceptBeforeDelivery);
            if (payload)
            {
                const ImVec2 mousePosition = ImGui::GetIO().MousePos;
                const bool isMesh = asset->type == AssetType_Mesh;

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddCircle(mousePosition, 9.0f, IM_COL32(255, 210, 80, 255), 20, 2.0f);
                drawList->AddText(ImVec2(mousePosition.x + 14.0f, mousePosition.y + 8.0f),
                                  IM_COL32(255, 255, 255, 230),
                                  isMesh ? "Drop mesh" : "Drop prefab");

                if (payload->IsDelivery())
                {
                    const glm::vec2 dropPosition = image.clampLocal(image.toLocal(mousePosition.x, mousePosition.y));
                    const glm::vec3 position = positionAt(dropPosition);

                    std::filesystem::path assetPath;
                    if (ReadAssetPath(*asset, assetPath))
                    {
                        created = isMesh
                            ? EntityFactory::CreateFromMesh(scene, assetPath, position, gridStep)
                            : EntityFactory::CreateFromPrefab(scene, assetPath, position, gridStep);
                    }
                }
            }
        }

        ImGui::EndDragDropTarget();
        return created;
    }
}
