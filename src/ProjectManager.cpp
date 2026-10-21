//
//  ProjectManager.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 15.10.26.
//

#include "ProjectManager.h"
#include "TextureManager.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>

ProjectManager& ProjectManager::Get()
{
    static ProjectManager instance;
    return instance;
}

ProjectManager::ProjectManager()
{
    std::ifstream file(recentProjectsFile());
    if (!file.is_open()) return;

    try
    {
        nlohmann::json recent;
        file >> recent;
        
        if (!recent.is_array()) return;

        for (const auto& path : recent)
        {
            if (!path.is_string()) continue;
            std::filesystem::path projectPath = path.get<std::string>();
            
            if (std::filesystem::exists(projectPath / "project.json")) {
                m_recentProjects.push_back(std::move(projectPath));
            }
        }
    }
    catch (const nlohmann::json::exception& exception)
    {
        std::cerr << "[ProjectManager] Failed to read recent projects: " << exception.what() << std::endl;
    }
}

std::filesystem::path ProjectManager::recentProjectsFile() const
{
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) return std::filesystem::path(appData) / "Cucumber" / "recent_projects.json";
#endif
    const char* home = std::getenv("HOME");
    if (home) return std::filesystem::path(home) / ".config" / "Cucumber" / "recent_projects.json";
    return std::filesystem::current_path() / "recent_projects.json";
}

void ProjectManager::saveRecentProjects() const
{
    const auto path = recentProjectsFile();
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    
    if (error)
    {
        std::cerr << "[ProjectManager] Failed to create recent-projects directory: " << error.message() << std::endl;
        return;
    }

    nlohmann::json recent = nlohmann::json::array();
    
    for (const auto& projectPath : m_recentProjects) {
        recent.push_back(projectPath.string());
    }

    std::ofstream file(path);
    if (!file || !(file << recent.dump(2))) {
        std::cerr << "[ProjectManager] Failed to save recent projects to " << path << std::endl;
    }
}

void ProjectManager::addRecentProject(const std::filesystem::path& path)
{
    m_recentProjects.erase(std::remove(m_recentProjects.begin(), m_recentProjects.end(), path), m_recentProjects.end());
    m_recentProjects.insert(m_recentProjects.begin(), path);
    
    if (m_recentProjects.size() > 20) m_recentProjects.resize(20);
    
    saveRecentProjects();
}

bool ProjectManager::loadProjectFile(const std::filesystem::path& projectPath, std::string& error)
{
    const auto filePath = projectPath / "project.json";
    std::ifstream file(filePath);
    
    if (!file)
    {
        error = "Не удалось открыть " + filePath.string();
        return false;
    }

    nlohmann::json data;
    
    try
    {
        file >> data;
    }
    catch (const nlohmann::json::exception& exception)
    {
        error = "Некорректный project.json: " + std::string(exception.what());
        return false;
    }

    if (!data.is_object())
    {
        error = "project.json должен содержать JSON-объект.";
        return false;
    }

    if (data.contains("start_scene") && !data["start_scene"].is_string())
    {
        error = "Поле start_scene должно быть строкой.";
        return false;
    }
    if (data.contains("editor") && !data["editor"].is_object())
    {
        error = "Поле editor должно быть JSON-объектом.";
        return false;
    }
    if (data.contains("name") && !data["name"].is_string())
    {
        error = "Поле name должно быть строкой.";
        return false;
    }

    std::string startScene = data.value("start_scene", "");
    if (!startScene.empty() && !std::filesystem::exists(projectPath / startScene))
    {
        error = "Стартовая сцена не найдена: " + (projectPath / startScene).string();
        return false;
    }

    m_projectPath = projectPath;
    m_projectName = data.value("name", projectPath.filename().string());
    m_startScene = std::move(startScene);
    m_projectData = std::move(data);
    
    return true;
}

bool ProjectManager::openProject(const std::filesystem::path& path, std::string& error)
{
    std::error_code filesystemError;
    const auto projectPath = std::filesystem::weakly_canonical(path, filesystemError);
    if (filesystemError || !std::filesystem::is_directory(projectPath))
    {
        error = "Выбранная папка проекта недоступна.";
        return false;
    }

    if (!loadProjectFile(projectPath, error)) return false;
    addRecentProject(projectPath);
    
    TextureManager::Get().init(assetsPath());
    
    return true;
}

bool ProjectManager::createProject(const std::filesystem::path& parentPath, const std::string& name, std::string& error)
{
    if (name.empty() || name == "." || name == ".." || name.find_first_of("/\\") != std::string::npos)
    {
        error = "Укажите корректное имя проекта.";
        return false;
    }

    const auto projectPath = parentPath / name;
    
    if (std::filesystem::exists(projectPath))
    {
        error = "Папка уже существует: " + projectPath.string();
        return false;
    }

    std::error_code filesystemError;
    std::filesystem::create_directories(projectPath / "assets" / "scenes", filesystemError);
    
    if (!filesystemError) std::filesystem::create_directories(projectPath / "assets" / "prefabs", filesystemError);
    if (!filesystemError) std::filesystem::create_directories(projectPath / "assets" / "scripts", filesystemError);
    if (!filesystemError) std::filesystem::create_directories(projectPath / "assets" / "textures", filesystemError);
    if (!filesystemError) std::filesystem::create_directories(projectPath / "assets" / "meshes", filesystemError);
    if (!filesystemError) std::filesystem::create_directories(projectPath / "assets" / "sounds", filesystemError);
    
    if (filesystemError)
    {
        error = "Не удалось создать папки проекта: " + filesystemError.message();
        return false;
    }

    const std::string startScene = "assets/scenes/main.json";
    {
        std::ofstream scene(projectPath / startScene);
        if (!scene || !(scene << R"({"type":"scene","entities":[]})"))
        {
            error = "Не удалось создать стартовую сцену.";
            return false;
        }
    }

    nlohmann::json project = {
        {"name", name},
        {"start_scene", startScene},
        {"editor", {{"camera_position", {0.0, 128.0, 128.0}}}}
    };
    {
        std::ofstream file(projectPath / "project.json");
        if (!file || !(file << project.dump(4)))
        {
            error = "Не удалось создать project.json.";
            return false;
        }
    }

    if (!openProject(projectPath, error)) return false;
    return true;
}

std::filesystem::path ProjectManager::resolveProjectPath(const std::filesystem::path& path) const
{
    if (path.is_absolute() || !hasOpenProject()) return path;
    return m_projectPath / path;
}

std::string ProjectManager::makeProjectRelative(const std::filesystem::path& path) const
{
    if (!hasOpenProject() || path.empty()) return path.generic_string();
    
    std::error_code error;
    const auto relativePath = std::filesystem::relative(path, m_projectPath, error);
    
    return error ? path.generic_string() : relativePath.generic_string();
}

std::array<float, 3> ProjectManager::editorCameraPosition() const
{
    if (m_projectData.contains("editor") &&
        m_projectData["editor"].is_object() &&
        m_projectData["editor"].contains("camera_position") &&
        m_projectData["editor"]["camera_position"].is_array() &&
        m_projectData["editor"]["camera_position"].size() == 3)
    {
        const auto& position = m_projectData["editor"]["camera_position"];
        if (position[0].is_number() && position[1].is_number() && position[2].is_number())
        {
            return {
                position[0].get<float>(),
                position[1].get<float>(),
                position[2].get<float>()
            };
        }
    }
    return {0.0f, 128.0f, 128.0f};
}

void ProjectManager::saveEditorCamera(float x, float y, float z)
{
    if (!hasOpenProject()) return;
    
    m_projectData["editor"]["camera_position"] = {x, y, z};
    std::ofstream file(m_projectPath / "project.json");
    
    if (!file || !(file << m_projectData.dump(4))) {
        std::cerr << "[ProjectManager] Failed to save project settings." << std::endl;
    }
}
