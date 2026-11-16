//
//  MaterialTable.h
//  Cucumber
//
//  Слоты материалов по имени текстуры. Общий для MapMesh и LevelEditMesh.
//

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "renderer/Mesh.h"
#include "editor/TextureLibrary.h"

class MaterialTable
{
public:
    uint32_t slot(const std::string& name)
    {
        const auto it = m_slotByName.find(name);
        if (it != m_slotByName.end()) return it->second;

        Material material;
        material.m_name = name;
        material.blendMode = BlendMode::Opaque;
        material.albedoMap = TextureLibrary::Get().info(name).handle;
        material.isEmbedded = false;

        const uint32_t newSlot = static_cast<uint32_t>(m_materials.size());
        m_materials.push_back(std::move(material));
        m_slotByName.emplace(name, newSlot);
        return newSlot;
    }

    // Адрес вектора стабилен, пока жива таблица: RenderScene хранит указатель на него
    const std::vector<Material>& materials() const { return m_materials; }

    void clear()
    {
        m_materials.clear();
        m_slotByName.clear();
    }

private:
    std::vector<Material> m_materials;
    std::unordered_map<std::string, uint32_t> m_slotByName;
};
