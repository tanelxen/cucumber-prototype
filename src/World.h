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
    
    Viewport* m_pMainView;
    
//    std::vector<std::unique_ptr<Entity>> m_entities;
    std::vector<std::unique_ptr<Entity>> m_entities;
    
private:
};
