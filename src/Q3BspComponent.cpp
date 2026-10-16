//
//  BSPMeshComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.10.26.
//

#include "Q3BspComponent.h"

#include "q3/Q3BSPAsset.h"
#include "q3/Q3BspMesh.h"
#include "q3/Q3BspCollision.h"

bool Q3BspComponent::loadQ3BSP(const std::filesystem::path& filepath)
{
    Q3BSPAsset bsp;
    
    if (!bsp.initFromFile(filepath.c_str())) {
        return false;
    }
    
    mesh = std::make_shared<Q3BspMesh>();
    mesh->initFromBsp(&bsp);
    
    collision = std::make_shared<Q3BspCollision>();
    collision->initFromBsp(&bsp);
    
//    auto collision = owner->getComponent<BSPCollisionComponent>();
//    
//    if (collision == nullptr)
//    {
//        collision = owner->addComponent<BSPCollisionComponent>();
//    }
//    
//    collision->initFromBsp(&bsp);
    
    return true;
}
