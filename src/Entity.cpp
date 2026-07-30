//
//  SourceMapScene.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "Entity.h"

Entity::Entity(const std::string &name) : m_name(name)
{
}

//template<typename T, typename... Args>
//void Entity::addComponent(Args&&... args)
//{
//    m_components[typeid(T)] = std::make_unique<T>(std::forward<Args>(args)...);
//}

//template<typename T>
//T* Entity::getComponent()
//{
//    auto it = m_components.find(typeid(T));
//    if (it != m_components.end()) {
//        return static_cast<T*>(it->second.get());
//    }
//    return nullptr;
//}
