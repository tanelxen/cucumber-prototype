//
//  SceneManager.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 05.10.26.
//

#include "SceneManager.h"
#include "World.h"
#include "SceneSerializer.h"
#include "UIManager.h"
#include "Entity.h"

#include "../vendor/json.hpp"

#include <iostream>

static nlohmann::json s_levelSnapshot;

SceneManager& SceneManager::Get()
{
    static SceneManager instance;
    return instance;
}

void SceneManager::createNewScene(const std::string& name)
{
    m_activeScene = std::make_unique<WorldScene>();
    m_activeScene->m_type = SceneType::Level;
    m_activeScene->m_filePath = name;
    m_activeScene->m_pAttachedViewport = g_UIManager->getMainViewport();
}

void SceneManager::openFile(const std::string& filePath)
{
    auto newScene = std::make_unique<WorldScene>();
    
    if (SceneSerializer::LoadFile(newScene.get(), filePath))
    {
        m_activeScene = std::move(newScene);
        m_activeScene->m_pAttachedViewport = g_UIManager->getMainViewport();
    }
}

void SceneManager::saveCurrent()
{
    if (!m_activeScene || m_activeScene->m_filePath.empty()) return;
    
    if (m_activeScene->isPrefabContext())
    {
        Entity* rootEntity = nullptr;
        
        for (const auto& ent : m_activeScene->m_entities)
        {
            if (!ent->getParent()) { rootEntity = ent.get(); break; }
        }
        
        if (rootEntity)
        {
            SceneSerializer::CreatePrefabFromEntity(m_activeScene.get(), rootEntity, m_activeScene->m_filePath);
        }
    }
    else
    {
        SceneSerializer::SaveScene(m_activeScene.get(), m_activeScene->m_filePath);
    }
}

// =================================================================
// РЕЖИМ РЕДАКТИРОВАНИЯ ПРЕФАБА
// =================================================================
void SceneManager::enterPrefabEditMode(const std::string& prefabPath)
{
    if (!m_activeScene || m_activeScene->isPrefabContext()) return;
    
    std::cout << "[SceneManager] Сохранение сцены уровня в снапшот и переход в префаб..." << std::endl;
    
    // 1. Запоминаем ВСЮ сцену уровня в JSON в памяти
    s_levelSnapshot.clear();
    SceneSerializer::SaveSceneToMemory(m_activeScene.get(), s_levelSnapshot);
    
    // 2. Загружаем префаб как изолированную сцену
    auto prefabScene = std::make_unique<WorldScene>();
    prefabScene->m_pAttachedViewport = g_UIManager->getMainViewport();
    
    if (SceneSerializer::LoadFile(prefabScene.get(), prefabPath))
    {
        m_savedLevelScene = std::move(m_activeScene);
        m_savedLevelScene->clear();
        
        m_activeScene = std::move(prefabScene);
    }
    
    g_UIManager->setEntity(nullptr);
}

void SceneManager::exitPrefabEditMode()
{
    if (!m_activeScene || !m_activeScene->isPrefabContext() || s_levelSnapshot.empty()) return;
    
    std::cout << "[SceneManager] Возврат на уровень и восстановление из снепшота" << std::endl;
    
    // 2. Магия: Мы разворачиваем уровень ИЗ СНАПШОТА памяти!
    if (SceneSerializer::LoadSceneFromMemory(m_savedLevelScene.get(), s_levelSnapshot))
    {
        m_activeScene = std::move(m_savedLevelScene);
        m_activeScene->m_pAttachedViewport = g_UIManager->getMainViewport();
    }
    
    s_levelSnapshot.clear();
    g_UIManager->setEntity(nullptr);
}


// =================================================================
// РЕЖИМ СИМУЛЯЦИИ (PLAY MODE)
// =================================================================
void SceneManager::startSimulation(class Viewport* viewport)
{
    if (!m_activeScene || m_activeScene->m_isSimulationMode) return;
    
    std::cout << "[SceneManager] Создание снапшота сцены и запуск симуляции..." << std::endl;
    
    // 1. Сериализуем ТЕКУЩУЮ сцену редактора в JSON-объект в памяти
    s_levelSnapshot.clear();
    SceneSerializer::SaveSceneToMemory(m_activeScene.get(), s_levelSnapshot);
    
    // 2. Включаем режим игры на текущей сцене
    m_activeScene->startSimulation(viewport);
}

void SceneManager::stopSimulation()
{
    if (!m_activeScene || !m_activeScene->m_isSimulationMode) return;
    
    std::cout << "[SceneManager] Остановка симуляции и восстановление сцены..." << std::endl;
    
    // Восстанавливаем сцену редактора в чистую память из сохраненного снапшота
    auto restoredScene = std::make_unique<WorldScene>();
    
    // Передаем JSON-снапшот в сериализатор
    if (SceneSerializer::LoadSceneFromMemory(restoredScene.get(), s_levelSnapshot))
    {
        m_activeScene = std::move(restoredScene);
        m_activeScene->m_pAttachedViewport = g_UIManager->getMainViewport();
        m_activeScene->m_isSimulationMode = false; // Возвращаем режим редактора
    }
    
    s_levelSnapshot.clear();
    g_UIManager->setEntity(nullptr); // Сбрасываем выделение в инспекторе
}
