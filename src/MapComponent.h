//
//  MapComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//
//  Компонент сцены, держащий статический уровень. Сам он теперь только связывает части:
//    LevelFiles          загрузка / сохранение / импорт .map          (trenchbroom/LevelFiles.h)
//    MapMesh             запечённая геометрия для рендера             (trenchbroom/MapMesh.h)
//    LightmapController  кэш лайтмапы, фоновый бейк                   (trenchbroom/LightmapController.h)
//    CollectBakeLights   лампы сцены для бейка                        (trenchbroom/BakeLights.h)
//    MapPhysicsBody      статическое тело в Jolt                      (trenchbroom/MapPhysicsBody.h)
//    LevelEditState      сессия правки и её меш                       (trenchbroom/LevelEditState.h)
//

#pragma once

#include "Component.h"
#include "trenchbroom/ISurfaceGeometry.h"
#include "trenchbroom/LevelEditMesh.h"      // не нужен самому заголовку, оставлен: на него могут опираться другие файлы
#include "LevelEditState.h"
#include "trenchbroom/LightmapController.h"
#include "MapPhysicsBody.h"
#include "level/LevelDocument.h"
#include "level/LevelEditSession.h"

#include <filesystem>
#include <memory>
#include <string>

class MapMesh;

struct MapComponent : public Component
{
    ~MapComponent();

    const std::string getTypeName() const override { return "MapComponent"; }

    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;

    // --- Лайтмапа ---
    bool bakeLighting();                            // запускает фоновый бейк и сразу возвращается
    void cancelBake()          { lightmap.cancel(); }
    bool isBaking() const      { return lightmap.isBaking(); }
    float bakeProgress() const { return lightmap.progress(); }
    bool isLightmapOutdated() const;
    bool rebuildLightmapUVs();      // из level, файл не читается

    // --- Файлы ---
    bool createEmptyLevel(const std::filesystem::path& relativePath);

    // Принимает и .map (импорт, если .cmap.json ещё нет), и .cmap.json. Имя оставлено, чтобы Inspector не менялся.
    bool loadTrenchBroomMap(const std::filesystem::path& filepath);
    bool loadLevel(const std::filesystem::path& filepath);                 // .cmap.json
    bool importTrenchBroomMap(const std::filesystem::path& mapFile, bool overwrite = false);
    bool reimportSource();
    bool saveLevel();

    // Меш + физика из level; вызывать после правок документа
    bool rebuildFromDocument();

    // level должен быть объявлен ДО m_edit: порядок уничтожения (сессия правки хранит ссылку на документ)
    LevelDocument level;

    // --- Правка ---
    bool beginEdit();
    void endEdit();
    bool isEditing() const { return m_edit != nullptr; }
    LevelEditSession* editSession() { return m_edit ? &m_edit->session() : nullptr; }
    bool isLevelDirty() const { return m_levelDirty || (m_edit && m_edit->session().history().isDirty()); }

    // Что рисовать как статику уровня: во время правки меш правки, иначе запечённый (или nullptr)
    const ISurfaceGeometry* renderGeometry() const;

    static constexpr const char* kNoMap = "<no map>";   // путь пустого компонента, у которого нет уровня

    std::string path = kNoMap;
    float lightmapTexelsPerUnit = 1.0f / 8.0f;

    std::string bakeStatus;
    LightmapController lightmap{bakeStatus};

    std::shared_ptr<MapMesh> mesh;

private:
    // Запрещает операции, меняющие документ, пока идёт правка (сессия держит ссылку на level)
    bool rejectIfEditing();

    bool rebuildMesh();
    std::filesystem::path lightmapCachePath() const;

    MapPhysicsBody m_physics;
    std::unique_ptr<LevelEditState> m_edit;

    bool m_levelDirty = false;
};
