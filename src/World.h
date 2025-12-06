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

class Camera;

class WorldScene
{
public:
    WorldScene(Camera* camera);
    ~WorldScene();
    
    void loadMap(const std::string &filename);
    void update(float dt);
    void draw();
    
private:
    Camera* m_pCamera;
    
    std::unique_ptr<class CameraController> m_pPlayer;
    
    
};
