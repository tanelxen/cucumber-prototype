//
//  LevelEditState.cpp
//  Cucumber
//

#include "LevelEditState.h"

#include "trenchbroom/LevelEditMesh.h"
#include "level/LevelEditSession.h"

LevelEditState::LevelEditState(LevelDocument& level)
    : m_mesh(std::make_unique<LevelEditMesh>())
{
    m_mesh->build(level);
    m_session = std::make_unique<LevelEditSession>(level, [this, &level](const ChangeSet& changes)
    {
        m_changed = true;
        m_mesh->apply(level, changes);
    });
}

LevelEditState::~LevelEditState() = default;
