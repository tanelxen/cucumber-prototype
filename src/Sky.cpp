//
//  Sky.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 01.09.26.
//

#include "Sky.h"
#include "ShaderVariant.h"
#include "RenderDevice.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../vendor/stb_image.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>

// 9 коэффициентов SH для RGB
float totalWeight = 0.0f;

static ShaderVariant g_skyShader;

// Сохранение 9 векторов в бинарный файл
bool saveSHToFile(const std::string& filePath) {
    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "[SH] Не удалось создать файл кэша: " << filePath << std::endl;
        return false;
    }
    
    // Записываем весь массив (9 * sizeof(glm::vec3) = 108 байт)
    out.write(reinterpret_cast<const char*>(RenderDevice::shCoefficients.data()), RenderDevice::shCoefficients.size() * sizeof(glm::vec3));
    out.close();
    
    std::cout << "[SH] Коэффициенты успешно закэшированы в: " << filePath << std::endl;
    return true;
}

// Загрузка 9 векторов из бинарного файла
bool loadSHFromFile(const std::string& filePath) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) {
        return false; // Файла нет или он недоступен
    }
    
    // Читаем данные напрямую в вектор
    in.read(reinterpret_cast<char*>(RenderDevice::shCoefficients.data()), RenderDevice::shCoefficients.size() * sizeof(glm::vec3));
    in.close();
    
    std::cout << "[SH] Коэффициенты успешно загружены из кэша: " << filePath << std::endl;
    return true;
}

// Базисные функции SH (Y_lm)
void computeSHBasis(const glm::vec3& dir, float* Y) {
    float x = dir.x; float y = dir.y; float z = dir.z;
    
    // l = 0
    Y[0] = 0.282095f;
    
    // l = 1
    Y[1] = 0.488603f * y;
    Y[2] = 0.488603f * z;
    Y[3] = 0.488603f * x;
    
    // l = 2
    Y[4] = 1.092548f * x * y;
    Y[5] = 1.092548f * y * z;
    Y[6] = 0.315392f * (3.0f * z * z - 1.0f);
    Y[7] = 1.092548f * x * z;
    Y[8] = 0.546274f * (x * x - y * y);
}

// Вызывать для каждой из 6 граней при загрузке через stb_image
void processCubeFace(int faceIndex, int width, int height, float* floatData) {
    float Y[9];
    
    for (int v = 0; v < height; ++v) {
        // Перевод в NDC [-1, 1] с выравниванием по центру пикселя
        float t = (v + 0.5f) / (float)height;
        float y_ndc = 1.0f - 2.0f * t;
        
        for (int u = 0; u < width; ++u) {
            float s = (u + 0.5f) / (float)width;
            float x_ndc = 2.0f * s - 1.0f;
            
            // Вычисляем направление луча из центра куба для текущей грани
            glm::vec3 dir;
            switch (faceIndex) {
                case 0: dir = glm::vec3( 1.0f,  y_ndc, -x_ndc); break; // +X
                case 1: dir = glm::vec3(-1.0f,  y_ndc,  x_ndc); break; // -X
                case 2: dir = glm::vec3( x_ndc,  1.0f, -y_ndc); break; // +Y
                case 3: dir = glm::vec3( x_ndc, -1.0f,  y_ndc); break; // -Y
                case 4: dir = glm::vec3( x_ndc,  y_ndc,  1.0f); break; // +Z
                case 5: dir = glm::vec3(-x_ndc,  y_ndc, -1.0f); break; // -Z
            }
            dir = glm::normalize(dir);
            
            // Учет искажения площади пикселя на сфере (Solid Angle)
            float da = 1.0f + x_ndc * x_ndc + y_ndc * y_ndc;
            float solidAngle = 4.0f / (std::sqrt(da) * da);
            
            // Чтение RGB (данные должны быть в Linear Space! Используйте stbi_loadf)
            int pixelIdx = (v * width + u) * 3;
            glm::vec3 color(floatData[pixelIdx], floatData[pixelIdx + 1], floatData[pixelIdx + 2]);
            
            computeSHBasis(dir, Y);
            
            // Накапливаем коэффициенты
            for (int i = 0; i < 9; ++i) {
                RenderDevice::shCoefficients[i] += color * Y[i] * solidAngle;
            }
            totalWeight += solidAngle;
        }
    }
}

void processCubeFace(int faceIndex, int width, int height, unsigned char* ucData) {
    float Y[9];
    
    for (int v = 0; v < height; ++v) {
        // Перевод в NDC [-1, 1] с выравниванием по центру пикселя
        float t = (v + 0.5f) / (float)height;
        float y_ndc = 1.0f - 2.0f * t;
        
        for (int u = 0; u < width; ++u) {
            float s = (u + 0.5f) / (float)width;
            float x_ndc = 2.0f * s - 1.0f;
            
            // Вычисляем направление луча из центра куба для текущей грани
            glm::vec3 dir;
            switch (faceIndex) {
                case 0: dir = glm::vec3( 1.0f,  y_ndc, -x_ndc); break; // +X
                case 1: dir = glm::vec3(-1.0f,  y_ndc,  x_ndc); break; // -X
                case 2: dir = glm::vec3( x_ndc,  1.0f, -y_ndc); break; // +Y
                case 3: dir = glm::vec3( x_ndc, -1.0f,  y_ndc); break; // -Y
                case 4: dir = glm::vec3( x_ndc,  y_ndc,  1.0f); break; // +Z
                case 5: dir = glm::vec3(-x_ndc,  y_ndc, -1.0f); break; // -Z
            }
            dir = glm::normalize(dir);
            
            // Учет искажения площади пикселя на сфере (Solid Angle)
            float da = 1.0f + x_ndc * x_ndc + y_ndc * y_ndc;
            float solidAngle = 4.0f / (std::sqrt(da) * da);
            
            // Чтение RGB (данные должны быть в Linear Space! Используйте stbi_loadf)
            int pixelIdx = (v * width + u) * 3;
            glm::vec3 color((float)ucData[pixelIdx]/255.0f, (float)ucData[pixelIdx + 1]/255.0f, (float)ucData[pixelIdx + 2]/255.0f);
            
            computeSHBasis(dir, Y);
            
            // Накапливаем коэффициенты
            for (int i = 0; i < 9; ++i) {
                RenderDevice::shCoefficients[i] += color * Y[i] * solidAngle;
            }
            totalWeight += solidAngle;
        }
    }
}

// Финальное масштабирование после обработки всех 6 граней
void finalizeSH() {
    float scale = (4.0f * 3.14159265f) / totalWeight;
    for (int i = 0; i < 9; ++i) {
        RenderDevice::shCoefficients[i] *= scale;
    }
}

bool loadAndComputeCubemapSH(const std::string& directoryPath, const std::string& extension) {
    // Стандартный порядок граней для кубической карты в OpenGL (от 0 до 5)
    std::string faceSuffixes[6] = {
        "posx", "negx", // +X, -X
        "posy", "negy", // +Y, -Y
        "posz", "negz"  // +Z, -Z
    };
    
    std::cout << "[SH] Начинаем расчет сферических гармоник для кубмапы..." << std::endl;
    
    // Сбрасываем глобальные переменные перед новым расчетом
    std::fill(RenderDevice::shCoefficients.begin(), RenderDevice::shCoefficients.end(), glm::vec3(0.0f));
    extern float totalWeight; // если она объявлена глобально
    totalWeight = 0.0f;
    
    // Отключаем вертикальный переворот при загрузке. Для SH проекции
    // направление лучей в коде `processCubeFace` уже учитывает стандартную развертку OpenGL.
    stbi_set_flip_vertically_on_load(false);
    
    for (int i = 0; i < 6; ++i) {
        std::string fullPath = directoryPath + "/" + faceSuffixes[i] + extension;
        
        int width, height, nrChannels;
        
        // ВАЖНО: Используем stbi_loadf для загрузки HDR (float) данных в Linear Space.
        // Запрашиваем строго 3 канала (RGB), чтобы шаг в массиве был предсказуем.
        float* data = stbi_loadf(fullPath.c_str(), &width, &height, &nrChannels, 3);
        
        if (data) {
            std::cout << "[SH] Успешно загружена грань: " << faceSuffixes[i]
            << " (" << width << "x" << height << ")" << std::endl;
            
            // Передаем указатель на float-данные в наш обработчик
            processCubeFace(i, width, height, data);
            
            // Сразу освобождаем память CPU, так как текстура нам больше не нужна
            stbi_image_free(data);
        }
        else {
            std::cerr << "[SH] Ошибка загрузки файла: " << fullPath << std::endl;
            return false;
        }
    }
    
    // Финализируем расчет (применяем масштабные коэффициенты)
    finalizeSH();
    
    std::cout << "[SH] Расчет окончен. Получено 9 коэффициентов." << std::endl;
    return true;
}

// Модифицированная функция загрузки кубмапы с поддержкой кэша
GLuint loadCubemapWithSHCache(const std::string& directoryPath, const std::string& extension)
{
    std::string cacheFilePath = directoryPath + "/sh_cache.sh";
    std::vector<std::string> faceSuffixes = { "posx", "negx", "posy", "negy", "posz", "negz" };
    
    // Попытка загрузить SH из готового файла кэша
    bool isSHLoadedFromCache = loadSHFromFile(cacheFilePath);
    
    // 1. Создаем текстуру кубмапы в OpenGL для неба
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    
    // Если кэша не было, сбрасываем переменные для нового расчета
    if (!isSHLoadedFromCache)
    {
        std::fill(RenderDevice::shCoefficients.begin(), RenderDevice::shCoefficients.end(), glm::vec3(0.0f));
        extern float totalWeight;
        totalWeight = 0.0f;
    }
    
    stbi_set_flip_vertically_on_load(false);
    
    for (unsigned int i = 0; i < 6; i++)
    {
        std::string fullPath = directoryPath + "/" + faceSuffixes[i] + extension;
        int width, height, nrChannels;
        
        // Загружаем картинку
        float* data = stbi_loadf(fullPath.c_str(), &width, &height, &nrChannels, 3);
        
        if (data)
        {
            // А. Считаем SH на CPU только ЕСЛИ они не были загружены из файла
            if (!isSHLoadedFromCache) {
                processCubeFace(i, width, height, data);
            }
            
            // Б. В любом случае загружаем текстуру на GPU для скайбокса
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_FLOAT, data);
            
            stbi_image_free(data);
        }
        else
        {
            std::cerr << "[Skybox] Ошибка загрузки: " << fullPath << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }
    }
    
    // 2. Если производился расчет — финализируем его и сохраняем кэш на диск
    if (!isSHLoadedFromCache)
    {
        finalizeSH();
        saveSHToFile(cacheFilePath);
    }
    
    // 3. Настройка параметров фильтрации (как в прошлом шаге)
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    return textureID;
}

static GLuint skyboxTextureID = 0;
static GLuint skyboxVBO = 0;
static GLuint skyboxVAO = 0;

void Sky::init()
{
    g_skyShader.init("assets/shaders/skybox.glsl");
    
    // Путь к папке, где лежат файлы: posx.hdr, negx.hdr и т.д.
    std::string cubemapDir = "assets/textures/cubemaps/tantolunden2";
    
    skyboxTextureID = loadCubemapWithSHCache(cubemapDir, ".jpg");
    
    float vertices[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
    };
    
    glGenVertexArrays(1, &skyboxVAO);
    glBindVertexArray(skyboxVAO);
    
    glGenBuffers(1, &skyboxVBO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 36 * 3, vertices, GL_STATIC_DRAW);
    

    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Sky::draw(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const
{
//    glDisable(GL_CULL_FACE);
    
    // 2. Рисуем небо
    glDepthFunc(GL_LEQUAL); // Меняем функцию глубины, чтобы z = 1.0 проходил тест
    
    g_skyShader.bind();
    
    // Передаем матрицы камеры
    glUniformMatrix4fv(glGetUniformLocation(g_skyShader.getProgramId(), "projection"), 1, GL_FALSE, &projectionMatrix[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_skyShader.getProgramId(), "view"), 1, GL_FALSE, &viewMatrix[0][0]);
    
    // Активируем кубмапу неба
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTextureID);
    glUniform1i(glGetUniformLocation(g_skyShader.getProgramId(), "skybox"), 0);
    
    // Рисуем куб неба
    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    
    g_skyShader.unbind();
    glDepthFunc(GL_LESS); // Возвращаем дефолтную функцию глубины
}
