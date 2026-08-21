//
//  ModelLoader.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.07.26.
//

#pragma once

#include <string>

class Mesh;
class Animator;

namespace ModelLoader {
    bool LoadGLTF(const std::string& filepath, Mesh& mesh, Animator& animator);
};
