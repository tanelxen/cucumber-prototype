# Cucumber

Crossplatform level editor. It was made with GLFW and support modern rendering via OpenGL 4.1.

On startup, the editor shows the project manager. Choose a recent project, browse to a
project folder containing `project.json`, or create a new project. New projects contain
an `assets/` directory and a starter scene.

`project.json` stores the project name, startup scene path, and editor settings:

```json
{
  "name": "MyProject",
  "start_scene": "assets/scenes/main.json",
  "editor": {
    "camera_position": [0, 128, 128]
  }
}
```

Editor-owned fonts, icons, shaders, and other built-in files live in `resources/`.
