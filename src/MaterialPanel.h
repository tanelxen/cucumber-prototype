//
//  MaterialPanel.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 12.08.26.
//

#pragma once

#include "EditorPanel.h"

class Material;

class MaterialPanel: public EditorPanel
{
public:
    MaterialPanel();
    ~MaterialPanel() = default;
    
    void setMaterial(Material* material);
    void draw() override;
    
private:
    void drawContent();
    Material* m_material;
};
