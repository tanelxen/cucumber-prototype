//
//  TexturePaste.h
//  Cucumber
//
//  Буфер и настройки переноса текстурирования между гранями. Отдельный заголовок, потому что ими пользуются
//  и LevelEditSession (хранит их), и FaceTexturing (работает с ними), а FaceTexturing сам включает LevelEditSession.
//  Лежит рядом с FaceTexturing.h (папка level).
//

#pragma once

#include "LevelDocument.h"

#include <glm/glm.hpp>
#include <string>

namespace FaceTexturing
{
    struct FaceClipboard
    {
        bool valid = false;
        std::string materialName;
        FaceUV uv;

        // Плоскость исходной грани на момент копирования: по ней Wrap продолжает рисунок на других гранях
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        float planeOffset = 0.0f;
    };

    enum class UVMode
    {
        Wrap,    // продолжать рисунок через ребро, как "align to face" в Hammer
        World    // та же мировая проекция как есть: годится для параллельных граней
    };

    struct PasteOptions
    {
        bool material = true;
        bool uv = true;
        bool wrap = true;     // UV: Wrap (true) или World (false)
        bool chain = false;   // после переноса грань-цель становится источником: рисунок идёт по цепочке граней

        UVMode mode() const { return wrap ? UVMode::Wrap : UVMode::World; }
    };
}
