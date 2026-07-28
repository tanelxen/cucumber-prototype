//
//  ModelLoader.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.07.26.
//

#pragma once

#include <string>

class Mesh;

namespace ModelLoader {
    bool LoadOBJ(const std::string& filepath, Mesh& mesh);
};
