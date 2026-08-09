//
//  AssetsPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

#include "EditorPanel.h"

class AssetsPanel: public EditorPanel
{
public:
    AssetsPanel();
    ~AssetsPanel() = default;
    
    void draw() override;
    
private:
};
