#include <framework/window.h>
#include <imgui/imgui.h>
#include <nativefiledialog/nfd.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>

const glm::ivec2 workspace_offset { 300, 0 };
const glm::ivec2 backdrop_offset { 4, -4 };
const int brush_fidelity = 72; // Number of points in brush

int main()
{
    glm::ivec2 win_size { 1300, 1000 };
    glm::ivec2 workspace_size { win_size.x - 300, win_size.y }; // The area where the canvas sits (leaving the GUI out)
    glm::ivec2 canvas_size { 900, 600 };
    glm::ivec2 canvas_pos { (workspace_size - canvas_size) / 2 + workspace_offset };
    Window window { "Watercolour Painting", win_size, OpenGLVersion::GL2, true };

    int brush_size = 10;
    bool show_gui = true;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.27f, 0.27f, 0.27f, 1.0f);

    // Callbacks
    window.registerKeyCallback([&](const int key, const int scancode, const int action, const int mods) {
        if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE)
            show_gui = !show_gui;
    });

    window.registerWindowResizeCallback([&](const glm::ivec2& size) {
        win_size = size;
        workspace_size = win_size - workspace_offset;

        // Recenter the canvas
        canvas_pos = (workspace_size - canvas_size) / 2 + workspace_offset;
    });

    // Helpers
    auto point_in_canvas = [&](const glm::vec2& point) {
        return point.x >= canvas_pos.x && point.x <= canvas_pos.x + canvas_size.x && point.y >= canvas_pos.y && point.y <= canvas_pos.y + canvas_size.y;
    };

    while (!window.shouldClose()) {

        window.updateInput();
        const glm::vec2 cursor_pos = window.getCursorPixel();
        const bool cursor_in_canvas = point_in_canvas(cursor_pos);

        // GUI
        if (show_gui) {
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(300, window.getWindowSize().y), ImGuiCond_Always);
            ImGui::Begin("", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            {

            }
        }

        // Drawing logic
        const glm::mat4 proj = glm::ortho(0.0f, (float)win_size.x, 0.0f, (float)win_size.y, - 1.0f, 1.0f);

        glViewport(0, 0, win_size.x, win_size.y);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw canvas
        const glm::vec4 canvas_lower_left = proj * glm::vec4(canvas_pos, 0.0f, 1.0f);
        const glm::vec4 canvas_upper_right = proj * glm::vec4(canvas_pos + canvas_size, 0.0f, 1.0f);
        const glm::vec4 backdrop_lower_left = proj * glm::vec4(canvas_pos + backdrop_offset, 0.0f, 1.0f);
        const glm::vec4 backdrop_upper_right = proj * glm::vec4(canvas_pos + canvas_size + backdrop_offset, 0.0f, 1.0f);

        // Backdrop effect (purely aesthetic)
        glColor4f(0.21f, 0.21f, 0.21f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(backdrop_lower_left.x, backdrop_lower_left.y);
        glVertex2f(backdrop_upper_right.x, backdrop_lower_left.y);
        glVertex2f(backdrop_upper_right.x, backdrop_upper_right.y);
        glVertex2f(backdrop_lower_left.x, backdrop_upper_right.y);
        glEnd();

        // Canvas proper
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(canvas_lower_left.x, canvas_lower_left.y);
        glVertex2f(canvas_upper_right.x, canvas_lower_left.y);
        glVertex2f(canvas_upper_right.x, canvas_upper_right.y);
        glVertex2f(canvas_lower_left.x, canvas_upper_right.y);
        glEnd();

        // Draw brush
        if (cursor_in_canvas) {

            // Hide cursor
            window.setMouseCapture(true);
            
            // Draw a circle
            glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < brush_fidelity; i++) {
                const float angle = glm::radians(i * 360.0f / brush_fidelity);
                const glm::vec2 point = glm::vec2(cos(angle) * brush_size, sin(angle) * brush_size) + cursor_pos;
                const glm::vec4 point_proj = proj * glm::vec4(point, 0.0f, 1.0f);
                glVertex2f(point_proj.x, point_proj.y);
            }
            glEnd();
        } else
            window.setMouseCapture(false);

        ImGui::End();
        window.swapBuffers();
    }

    return 0;
}