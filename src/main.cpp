#include <framework/window.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <nativefiledialog/nfd.h>
#include <deque>

#include "splat.hpp"
#include "canvas.hpp"
#include "style.hpp"

const glm::ivec2 workspace_offset { 300, 0 };
const float stamp_spacing = 5.0f; // Stroke length before a new stamp is placed
const int brush_fidelity = 72; // Number of points in brush

int main()
{
    glm::ivec2 win_size { 1300, 1000 };
    glm::ivec2 workspace_size { win_size.x - 300, win_size.y }; // The area where the canvas sits (leaving the GUI out)
    Window window { "Watercolour Painting", win_size, OpenGLVersion::GL2, true };
    
    const glm::ivec2 canvas_size { 900, 600 };
    const glm::ivec2 canvas_pos { (workspace_size - canvas_size) / 2 + workspace_offset };
    Canvas canvas { canvas_pos, canvas_size };

    glm::vec3 brush_color = { 1.0f, 0.0f, 0.0f };
    int brush_size = 10;
    bool ctrl = false;
    bool debug = false;

    glm::vec2 cursor_pos;
    glm::vec2 last_stamp;
    bool stroke = false;
    int stroke_id = 0;

    std::deque<Splat> live_splats = {};
    std::deque<Splat> undone_splats = {};

    float time_step = 0.1f;
    float time_accum = 0.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.27f, 0.27f, 0.27f, 1.0f);

    // Callbacks
    window.registerKeyCallback([&](const int key, const int scancode, const int action, const int mods) {

        // Hold Ctrl
        if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) {
            if (action == GLFW_PRESS)
                ctrl = true;
            else if (action == GLFW_RELEASE)
                ctrl = false;
        }

        // Ctrl+Z undoes the last stroke
        if (key == GLFW_KEY_Z && ctrl && action == GLFW_PRESS) {
            if (live_splats.size() > 0) {
                const int last_stroke_id = live_splats.back().stroke_id;
                while (live_splats.size() > 0 && live_splats.back().stroke_id == last_stroke_id) {
                    undone_splats.push_back(live_splats.back());
                    live_splats.pop_back();
                }
            }
        }

        // Ctrl+Y redoes the last undone stroke
        if (key == GLFW_KEY_Y && ctrl && action == GLFW_PRESS) {
            if (undone_splats.size() > 0) {
                const int last_stroke_id = undone_splats.back().stroke_id;
                while (undone_splats.size() > 0 && undone_splats.back().stroke_id == last_stroke_id) {
                    live_splats.push_back(undone_splats.back());
                    undone_splats.pop_back();
                }
            }
        }

        // Toggle debug info
        if (key == GLFW_KEY_D && action == GLFW_PRESS)
            debug = !debug;
    });

    window.registerMouseButtonCallback([&](const int button, const int action, const int mods) {
        // Left mouse button is for drawing strokes
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            stroke = true;
            last_stamp = canvas.canvas_coords(cursor_pos);
            if (canvas.contains_point(cursor_pos))
                live_splats.push_back(Splat(last_stamp, brush_color, brush_size, stroke_id)); // TODO: use stamps instead of splats
            undone_splats.clear();
        } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
            stroke = false;
            stroke_id++;
        }
    });

    window.registerMouseMoveCallback([&](const glm::vec2& new_pos) {
        if (stroke) {
            const glm::vec2 cur_pos = canvas.canvas_coords(new_pos);
            const float dist = glm::distance(last_stamp, cur_pos);
            // Place stamps where appropriate
            if (dist >= stamp_spacing) {
                const glm::vec2 dir = glm::normalize(glm::vec2(cur_pos - last_stamp));
                for (float i = stamp_spacing; i <= dist; i += stamp_spacing) {
                    last_stamp = last_stamp + stamp_spacing * dir;
                    if (canvas.contains_canvas_point(last_stamp))
                        live_splats.push_back(Splat(last_stamp, brush_color, brush_size, stroke_id)); // TODO: use stamps instead of splats
                }
            }
        }
        cursor_pos = new_pos;
    });

    window.registerWindowResizeCallback([&](const glm::ivec2& size) {
        // Resizing window and its contents
        win_size = size;
        workspace_size = win_size - workspace_offset;

        // Recenter the canvas
        canvas.pos = (workspace_size - canvas.size) / 2 + workspace_offset;
    });

    // ImGui setup
    darkStyle();

    auto t = std::chrono::system_clock::now();
    float fps = 0.0f;

    // Main loop
    while (!window.shouldClose()) {

        // Calculate time delta
        const auto new_t = std::chrono::system_clock::now();
        const float dt = std::chrono::duration<float>(new_t - t).count();
        time_accum += dt;
        t = new_t;

        // Time step
        while (time_accum >= time_step) {
            time_accum -= time_step;
            fps = 1.0f / dt;
        }

        window.updateInput();

        // GUI
        {
            // Main menu
            ImGui::BeginMainMenuBar();
            if (ImGui::MenuItem("New")) {
                // TODO
            }
            if (ImGui::MenuItem("Load")) {
                // TODO
            }
            if (ImGui::MenuItem("Save")) {
                // TODO
            }

            // Print canvas dimensions on the right-hand side of the menu bar
            const std::string canvas_size_str = std::to_string(canvas.size.x) + "x" + std::to_string(canvas.size.y);
            const char* canvas_size_cstr = canvas_size_str.c_str();
            const int canvas_size_str_width = ImGui::CalcTextSize(canvas_size_cstr).x;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - canvas_size_str_width - 8);
            ImGui::Text(canvas_size_cstr);

            const int main_menu_height = ImGui::GetWindowHeight();
            ImGui::EndMainMenuBar();

            // Options
            ImGui::SetNextWindowPos(ImVec2(-1, main_menu_height - 1), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(300, window.getWindowSize().y - main_menu_height + 2), ImGuiCond_Always);
            ImGui::Begin("", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            {
                // Brush settings
                ImGui::Text("Brush");
                ImGui::SliderInt("Size", &brush_size, 1, 50);
                ImGui::Separator();
                ImGui::ColorPicker3("Colour", &brush_color.r);

                // Debug info
                if (debug) {
                    ImGui::Separator();
                    ImGui::Text("Debug (%d fps)", (int)fps);
                    ImGui::Text("Live splats: %d", live_splats.size());
                    ImGui::Text("Last stamp: (%f, %f)", last_stamp.x, last_stamp.y);
                }
            }
        }

        // Drawing logic
        const glm::mat4 proj = glm::ortho(0.0f, (float)win_size.x, 0.0f, (float)win_size.y, -1.0f, 1.0f);

        glViewport(0, 0, win_size.x, win_size.y);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw canvas
        canvas.draw(proj, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));

        // Draw splats
        for (const Splat& splat : live_splats) {
            const glm::vec2 splat_pos = canvas.window_coords(splat.pos);
            const glm::vec4 splat_proj = proj * glm::vec4(splat_pos, 0.0f, 1.0f);
            glColor4f(splat.color.r, splat.color.g, splat.color.b, 0.1f);
            glBegin(GL_TRIANGLE_FAN);
            for (int i = 0; i < brush_fidelity; i++) {
                const float angle = glm::radians(i * 360.0f / brush_fidelity);
                const glm::vec2 point = canvas.clamp_point(glm::vec2(cos(angle) * splat.size, sin(angle) * splat.size) + splat_pos);
                const glm::vec4 point_proj = proj * glm::vec4(point, 0.0f, 1.0f);
                glVertex2f(point_proj.x, point_proj.y);
            }
            glEnd();
        }

        // Draw brush
        if (canvas.contains_point(cursor_pos)) {

            // Hide cursor
            window.setMouseCapture(true);

            // Draw a circle
            glColor4f(0.0f, 0.0f, 0.0f, stroke ? 0.8f : 0.6f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < brush_fidelity; i++) {
                const float angle = glm::radians(i * 360.0f / brush_fidelity);
                const glm::vec2 point = canvas.clamp_point(glm::vec2(cos(angle) * brush_size, sin(angle) * brush_size) + cursor_pos);
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