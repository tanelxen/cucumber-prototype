//
//  CameraManager.h
//  Cucumber
//
//  Владеет ответом на вопрос "какие игровые камеры сейчас активны".
//  Раньше это делали WorldScene::getActiveCameraStack() (с побочными эффектами) и
//  m_pActiveCameraComponent (который писал рендер). Теперь:
//   - rebuild() — единственное место, где разрешаются primary и порядок оверлеев;
//   - stack()/active() — чистые геттеры без аллокаций;
//   - рендер сюда ничего не пишет.
//

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

class CameraComponent;

class CameraManager
{
public:
    struct Stack
    {
        CameraComponent* primary = nullptr;       // базовая камера
        std::vector<CameraComponent*> overlays;   // отсортированы по depth (меньше = раньше)
    };

    void add(CameraComponent* camera);
    void remove(CameraComponent* camera);

    // Вызывать раз в кадр (в конце WorldScene::update) и после структурных изменений.
    void rebuild();

    const Stack& stack(uint32_t stackId) const;
    CameraComponent* active() const { return stack(0).primary; }
    bool hasActive() const { return active() != nullptr; }

    // Явно сделать камеру основной в её стеке. false — если камера не подходит.
    bool setPrimary(CameraComponent* camera);

    const std::vector<CameraComponent*>& all() const { return m_cameras; }

private:
    std::vector<CameraComponent*> m_cameras;
    std::unordered_map<uint32_t, Stack> m_stacks;
};
