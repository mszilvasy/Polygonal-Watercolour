#include <framework/screen.h>
#include <framework/window.h>
#include <imgui/imgui.h>
#include <nativefiledialog/nfd.h>

int main()
{

    glm::ivec2 win_size { 720, 720 };

    Window window { "Watercolour Painting", win_size, OpenGLVersion::GL2, true };
    Screen screen { win_size, true };

    bool show_gui = true;

    window.registerKeyCallback([&](const int key, const int scancode, const int action, const int mods) {
        if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE)
            show_gui = !show_gui;
    });

    while (!window.shouldClose()) {
        
        window.updateInput();
        
        if (show_gui) {
            ImGui::Begin("Watercolour Painting");
            {
                // UI
            }
        }

        screen.clear(glm::vec3(1.0f));

        // Drawing logic

        screen.draw();

        ImGui::End();
        window.swapBuffers();
    }

    return 0;
}