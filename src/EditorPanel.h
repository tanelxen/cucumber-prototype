//
//  EditorPanel.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 09.08.26.
//

#pragma once

#include <string>

class EditorPanel
{
public:
    EditorPanel(const std::string& name) : m_name(name) { }
    
    virtual void update(float dt) { };
    virtual void draw() = 0;
    
    std::string m_name;
    bool isVisible = true;
};
