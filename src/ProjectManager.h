//
//  ProjectManager.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 15.10.26.
//

#pragma once

#include <filesystem>
#include <array>
#include <string>
#include <vector>

#include "../vendor/json.hpp"

class ProjectManager
{
public:
    static ProjectManager& Get();

    bool openProject(const std::filesystem::path& path, std::string& error);
    bool createProject(const std::filesystem::path& parentPath, const std::string& name, std::string& error);
    void saveEditorCamera(float x, float y, float z);

    bool hasOpenProject() const { return !m_projectPath.empty(); }
    const std::filesystem::path& projectPath() const { return m_projectPath; }
    const std::string& projectName() const { return m_projectName; }
    const std::string& startScene() const { return m_startScene; }
    
    std::filesystem::path assetsPath() const { return m_projectPath / "assets"; }
    std::filesystem::path scriptsPath() const { return assetsPath() / "scripts"; }
    std::filesystem::path scenesPath() const { return assetsPath() / "scenes"; }
    
    std::filesystem::path resolveProjectPath(const std::filesystem::path& path) const;
    std::string makeProjectRelative(const std::filesystem::path& path) const;
    std::vector<std::filesystem::path> recentProjects() const { return m_recentProjects; }
    std::array<float, 3> editorCameraPosition() const;

private:
    ProjectManager();

    bool loadProjectFile(const std::filesystem::path& projectPath, std::string& error);
    void addRecentProject(const std::filesystem::path& path);
    void saveRecentProjects() const;
    std::filesystem::path recentProjectsFile() const;

    std::filesystem::path m_projectPath;
    std::string m_projectName;
    std::string m_startScene;
    nlohmann::json m_projectData;
    std::vector<std::filesystem::path> m_recentProjects;
};
