//
//  SceneManager.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 05.10.26.
//

#pragma once

#include <memory>
#include <string>

class WorldScene;

class SceneManager
{
public:
    static SceneManager& Get();
    
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    
    WorldScene* getActiveScene() { return m_activeScene.get(); }
    
    void createNewScene(const std::string& name = "Untitled");
    bool openFile(const std::string& filePath);
    void saveCurrentAs(const std::string& filename);
    void saveCurrent();
    
    // Переключение режимов (вызывается из UI редактора)
    void enterPrefabEditMode(const std::string& prefabPath);
    void exitPrefabEditMode();
    
    // Режим Симуляции (Play Mode)
    void startSimulation();
    void stopSimulation();
    
private:
    std::unique_ptr<WorldScene> m_activeScene = nullptr;
    std::unique_ptr<WorldScene> m_savedLevelScene = nullptr; // Бэкап уровня при уходе в префаб
    
    SceneManager() = default;
};
