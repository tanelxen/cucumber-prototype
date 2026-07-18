//
//  ViewportPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

#include <string>

class Viewport;

class ViewportPanel
{
public:
    ViewportPanel(const std::string &name);
//    ~Viewport();
    
    void setViewport(Viewport* viewport);
    void draw();
    
private:
    std::string m_name;
    Viewport* m_viewport;
};
