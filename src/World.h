//
//  SourceMapScene.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#pragma once

#include <string>
#include <vector>
#include <memory>


class Viewport;

class Entity;

class WorldScene
{
public:
    WorldScene();
    ~WorldScene();
    
    void loadMap(const std::string &filename);
    void update(float dt);
    void draw();
    
    void selectEntityAtIndex(int index);
    
    void save();
    void load();
    
    Viewport* m_pMainView;
    
//    std::vector<std::unique_ptr<Entity>> m_entities;
    std::vector<std::unique_ptr<Entity>> m_entities;
    
    Entity* findEntityByName(const std::string& name);
    
    bool m_drawGrid = true;
    bool m_drawSkybox = true;
    
private:
};

inline WorldScene* g_currentScene;
