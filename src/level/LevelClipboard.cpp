//
//  LevelClipboard.cpp
//  Cucumber
//

#include "LevelClipboard.h"

#include "SolidBuilder.h"

LevelClipboard& LevelClipboard::Get()
{
    static LevelClipboard clipboard;
    return clipboard;
}

void LevelClipboard::clear()
{
    m_materials.clear();
    m_solids.clear();
}

void LevelClipboard::assign(const LevelDocument& source, const std::vector<const Solid*>& solids)
{
    clear();

    for (const Solid* solid : solids)
    {
        Solid copy = *solid;
        for (Face& face : copy.faces)
        {
            const std::string& name = source.materials[face.material];

            size_t index = 0;
            while (index < m_materials.size() && m_materials[index] != name) ++index;
            if (index == m_materials.size()) m_materials.push_back(name);

            face.material = static_cast<uint32_t>(index);
        }
        m_solids.push_back(std::move(copy));
    }
}

AABB LevelClipboard::bounds() const
{
    AABB box;
    for (const Solid& solid : m_solids)
    {
        for (const glm::vec3& v : solid.vertices) box.expand(v);
    }
    return box;
}

std::vector<Solid> LevelClipboard::instantiate(LevelDocument& target, const glm::vec3& delta, bool textureLock) const
{
    std::vector<Solid> result;
    result.reserve(m_solids.size());

    for (const Solid& stored : m_solids)
    {
        Solid copy = CloneWithNewUids(stored);
        for (Face& face : copy.faces) face.material = target.materialIndex(m_materials[face.material]);
        result.push_back(TranslatedSolid(copy, delta, textureLock));
    }
    return result;
}
