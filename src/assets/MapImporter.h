//
//  MapImporter.h
//  Cucumber
//
//  Импорт TrenchBroom .map (уже разобранного MapAsset) в LevelDocument. Односторонний: после перехода
//  на свой формат .map нужен только для импорта чужих карт.
//

#pragma once

#include "level/LevelDocument.h"

#include <string>
#include <vector>

struct MapAsset;

struct MapImportReport
{
    int brushesRead = 0;
    int brushesImported = 0;
    int brushesSkipped = 0;     // < 4 сторон, не образуют объём или не прошли проверку
    int facesDropped = 0;       // стороны, выродившиеся при построении тела
    int entitiesIgnored = 0;    // всё, кроме worldspawn (брашевые сущности не поддерживаются)
    std::vector<std::string> warnings;
};

// Идентификаторы детерминированы (хэш номера браша и стороны): повторный импорт той же карты даёт те же UID.
bool ImportMapAsset(const MapAsset& mapData, LevelDocument& out, MapImportReport& report, std::string& error);
