#include <deque>
#include <framework/window.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <nativefiledialog/nfd.h>

#include "canvas.hpp"
#include "splat.hpp"
#include "style.hpp"

const glm::ivec2 workspace_offset { 300, 0 };
const float stamp_spacing = 5.0f; // Stroke length before a new stamp is placed
const int drying_time = 60;

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
    float roughness = 1;
    float flow = 1.0f;
    int lifetime = 60;
    int vertices = 25;

    bool ctrl = false;
    bool debug = false;
    bool display_wet_map = false;

    glm::vec2 cursor_pos;
    glm::vec2 last_stamp;
    bool stroke = false;
    bool pan = false;
    int stroke_id = 0;

    std::deque<Splat> flowing_splats = {};
    std::deque<Splat> fixed_splats = {};
    std::deque<Splat> undone_splats = {};

    float time_step = 0.0167f; // Roughly corresponds to 60 fps
    float time_accum = 0.0f;

    glEnable(GL_BLEND);
    glStencilFunc(GL_EQUAL, 1, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Canvas texture
    GLuint bg_fbo;
    glGenFramebuffers(1, &bg_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, bg_fbo);

    GLuint bg;
    glGenTextures(1, &bg);
    glBindTexture(GL_TEXTURE_2D, bg);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, canvas_size.x, canvas_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLuint bg_stencil;
    glGenRenderbuffers(1, &bg_stencil);
    glBindRenderbuffer(GL_RENDERBUFFER, bg_stencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, canvas_size.x, canvas_size.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, bg_stencil);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bg, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.27f, 0.27f, 0.27f, 1.0f);

    // Wet map
    GLuint wet_map_fbo;
    glGenFramebuffers(1, &wet_map_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, wet_map_fbo);

    GLuint wet_map;
    glGenTextures(1, &wet_map);
    glBindTexture(GL_TEXTURE_2D, wet_map);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, canvas_size.x, canvas_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, wet_map, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
            if (flowing_splats.size() > 0 || fixed_splats.size() > 0) {
                const int last_stroke_id = flowing_splats.size() > 0 ? flowing_splats.back().stroke_id : fixed_splats.back().stroke_id;
                while (flowing_splats.size() > 0 && flowing_splats.back().stroke_id == last_stroke_id) {
                    undone_splats.push_back(flowing_splats.back());
                    flowing_splats.pop_back();
                }
                while (fixed_splats.size() > 0 && fixed_splats.back().stroke_id == last_stroke_id) {
                    undone_splats.push_back(fixed_splats.back());
                    fixed_splats.pop_back();
                }
            }
        }

        // Ctrl+Y redoes the last undone stroke
        if (key == GLFW_KEY_Y && ctrl && action == GLFW_PRESS) {
            if (undone_splats.size() > 0) {
                const int last_stroke_id = undone_splats.back().stroke_id;
                while (undone_splats.size() > 0 && undone_splats.back().stroke_id == last_stroke_id) {
                    if (undone_splats.back().life > 0)
                        flowing_splats.push_back(undone_splats.back());
                    else
                        fixed_splats.push_back(undone_splats.back());
                    undone_splats.pop_back();
                }
            }
        }

        // Toggle debug info
        if (key == GLFW_KEY_D && action == GLFW_PRESS)
            debug = !debug;
    });

    window.registerMouseButtonCallback([&](const int button, const int action, const int mods) {
        // Left mouse button draws strokes
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            stroke = true;

            // Place the first stamp
            last_stamp = canvas.canvas_coords(cursor_pos);
            if (canvas.contains_canvas_point(last_stamp))
                flowing_splats.push_back(Splat(canvas, last_stamp, brush_color, brush_size, roughness, flow, stroke_id, lifetime, vertices)); // TODO: use stamps instead of splats
            undone_splats.clear();

            // Bind wet map
            glBindFramebuffer(GL_FRAMEBUFFER, wet_map_fbo);
            glViewport(0, 0, canvas.size.x, canvas.size.y);
            glColor3f(1.0f, 1.0f, 1.0f);
            const glm::mat4 proj = glm::ortho(0.0f, (float)canvas.size.x, 0.0f, (float)canvas.size.y, -1.0f, 1.0f);

            // Update wet map
            glBegin(GL_TRIANGLE_FAN);
            for (int i = 0; i < vertices; i++) {
                const float angle = i * 2.0f * glm::pi<float>() / vertices;
                const glm::vec2 point = last_stamp + (float)brush_size * glm::vec2(std::cos(angle), std::sin(angle));
                const glm::vec2 point_proj = proj * glm::vec4(point, 0.0f, 1.0f);
                glVertex2f(point_proj.x, point_proj.y);
            }
            glEnd();

            // Unbind wet map
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

        } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
            stroke = false;
            stroke_id++;
        }

        // Middle mouse button pans the canvas
        if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
            pan = true;
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
            pan = false;
    });

    window.registerMouseMoveCallback([&](const glm::vec2& new_pos) {
        if (stroke) {

            const glm::vec2 cur_pos = canvas.canvas_coords(new_pos);
            const float dist = glm::distance(last_stamp, cur_pos);

            // Iterate along the stroke, updating the wet map and placing stamps
            if (dist >= stamp_spacing) {

                // Bind wet map
                glBindFramebuffer(GL_FRAMEBUFFER, wet_map_fbo);
                glViewport(0, 0, canvas.size.x, canvas.size.y);
                glColor3f(1.0f, 1.0f, 1.0f);
                const glm::mat4 proj = glm::ortho(0.0f, (float)canvas.size.x, 0.0f, (float)canvas.size.y, -1.0f, 1.0f);

                const glm::vec2 dir = glm::normalize(glm::vec2(cur_pos - last_stamp));
                glm::vec2 pos = last_stamp;

                for (float i = 1.0f; i <= dist; i += 1.0f) {

                    pos += dir;

                    // Update wet map
                    glBegin(GL_TRIANGLE_FAN);
                    for (int j = 0; j < vertices; j++) {
                        const float angle = j * 2.0f * glm::pi<float>() / vertices;
                        const glm::vec2 point = pos + (float)brush_size * glm::vec2(std::cos(angle), std::sin(angle));
                        const glm::vec2 point_proj = proj * glm::vec4(point, 0.0f, 1.0f);
                        glVertex2f(point_proj.x, point_proj.y);
                    }
                    glEnd();

                    // Place stamp
                    if (std::fmod(i, stamp_spacing) == 0.0f) {
                        last_stamp = pos;
                        if (canvas.contains_canvas_point(last_stamp))
                            flowing_splats.push_back(Splat(canvas, last_stamp, brush_color, brush_size, roughness, flow, stroke_id, lifetime, vertices)); // TODO: use stamps instead of splats
                    }
                }

                // Unbind wet map
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }

        if (pan)
            // Pan the canvas
            canvas.pos -= cursor_pos - new_pos;

        cursor_pos = new_pos;
    });

    window.registerWindowResizeCallback([&](const glm::ivec2& size) {
        // Resizing window and its contents
        win_size = size;
        workspace_size = win_size - workspace_offset;

        // Recenter the canvas
        canvas.pos = (glm::vec2(workspace_size) - canvas.zoom * canvas.size) / 2.0f + glm::vec2(workspace_offset);
    });

    window.registerScrollCallback([&](const glm::vec2& offset) {
        // Increment zoom
        const float change = offset.y > 0.0f ? 0.25f : -0.25f;
        const float new_zoom = canvas.zoom + change;
        if (new_zoom >= 0.25f && new_zoom <= 8.0f) {
            const float scale = new_zoom / canvas.zoom;
            canvas.zoom = new_zoom;
            canvas.pos = (canvas.pos - cursor_pos) * scale + cursor_pos;
        }
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

            glBindFramebuffer(GL_FRAMEBUFFER, wet_map_fbo);
            glViewport(0, 0, canvas.size.x, canvas.size.y);

            // Advect flowing splats
            auto wet_map_data = new unsigned char[3 * canvas.size.x * canvas.size.y];
            glReadPixels(0, 0, canvas.size.x, canvas.size.y, GL_RGB, GL_UNSIGNED_BYTE, wet_map_data);
            for (auto it = flowing_splats.begin(); it != flowing_splats.end(); it++)
                it->advect(canvas, wet_map_data);

            // Fix splats which have stopped flowing
            while (flowing_splats.size() > 0 && flowing_splats.front().life <= 0) {
                fixed_splats.push_back(flowing_splats.front());
                flowing_splats.pop_front();
            }

            // Age fixed splats
            for (auto it = fixed_splats.begin(); it != fixed_splats.end(); it++)
                it->life--;

            // Reduce wetness
            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
            glBlendFunc(GL_ONE, GL_ONE);

            glColor3b(1, 1, 1);
            glBegin(GL_QUADS);
            glVertex2f(-1.0f, -1.0f);
            glVertex2f(1.0f, -1.0f);
            glVertex2f(1.0f, 1.0f);
            glVertex2f(-1.0f, 1.0f);
            glEnd();

            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
            const std::string canvas_size_str = std::to_string((int)(100 * canvas.zoom)) + "%%   " + std::to_string((int)canvas.size.x) + "x" + std::to_string((int)canvas.size.y);
            const char* canvas_size_cstr = canvas_size_str.c_str();
            const int canvas_size_str_width = ImGui::CalcTextSize(canvas_size_cstr).x;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - canvas_size_str_width - 8);
            ImGui::Text(canvas_size_cstr);

            const int main_menu_height = ImGui::GetWindowHeight();
            ImGui::EndMainMenuBar();

            // Settings
            ImGui::SetNextWindowPos(ImVec2(-1, main_menu_height - 1), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(300, window.getWindowSize().y - main_menu_height + 2), ImGuiCond_Always);
            ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            {
                // Brush settings
                ImGui::Text("Brush");
                ImGui::SliderInt("Size", &brush_size, 1, 50);
                ImGui::SliderFloat("Roughness", &roughness, 0.0f, 2.0f);
                SliderPercent("Flow", &flow, 0.0f, 2.0f);
                ImGui::SliderInt("Lifetime", &lifetime, 0, 300);
                ImGui::SliderInt("Vertices", &vertices, 3, 50);
                ImGui::Separator();
                ImGui::ColorPicker3("Colour", &brush_color.r);

                // Debug info
                if (debug) {
                    ImGui::Separator();
                    ImGui::Text("Debug (%d fps)", (int)fps);
                    ImGui::Text("Strokes: %d", stroke_id);
                    ImGui::Text("Live splats: %d", flowing_splats.size() + fixed_splats.size());
                    ImGui::Text("Last stamp: (%f, %f)", last_stamp.x, last_stamp.y);
                    ImGui::Checkbox("Wet map", &display_wet_map);
                }
            }
        }

        // Drawing logic
        glm::mat4 proj;

        const auto draw_splat = [&](const Splat& splat, bool draw_to_window = true) {
            if (draw_to_window && debug) {
                // Debug vertices
                glColor4f(splat.color.r, splat.color.g, splat.color.b, 0.1f);
                glBegin(GL_TRIANGLE_FAN);

                const glm::vec2 center = canvas.window_coords(splat.pos);
                const glm::vec4 center_proj = proj * glm::vec4(center, 0.0f, 1.0f);
                glVertex2f(center_proj.x, center_proj.y);

                for (int i = 0; i <= splat.vertices.size(); i++) {
                    const int j = i % splat.vertices.size();
                    const glm::vec2 point = canvas.window_coords(splat.vertices[j].pos);
                    const glm::vec4 point_proj = proj * glm::vec4(point, 0.0f, 1.0f);
                    glVertex2f(point_proj.x, point_proj.y);
                }

                glEnd();
            } else {
                // Two pass stencil buffer approach
                glEnable(GL_STENCIL_TEST);
                float x_min = draw_to_window ? win_size.x : canvas.size.x, x_max = 0, y_min = draw_to_window ? win_size.y : canvas.size.y, y_max = 0; // Bounding box

                // First pass: mask out color buffer, draw splat to stencil buffer
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glStencilOp(GL_INVERT, GL_KEEP, GL_KEEP);

                glBegin(GL_TRIANGLE_FAN);

                const glm::vec2 center = draw_to_window ? canvas.window_coords(splat.pos) : splat.pos;
                const glm::vec4 center_proj = proj * glm::vec4(center, 0.0f, 1.0f);
                glVertex2f(center_proj.x, center_proj.y);

                for (int i = 0; i <= splat.vertices.size(); i++) {
                    const int j = i % splat.vertices.size();
                    const glm::vec2 point = draw_to_window ? canvas.window_coords(splat.vertices[j].pos) : splat.vertices[j].pos;
                    const glm::vec4 point_proj = proj * glm::vec4(point, 0.0f, 1.0f);
                    glVertex2f(point_proj.x, point_proj.y);

                    // Update bounding box
                    x_min = std::min(point_proj.x, x_min);
                    x_max = std::max(point_proj.x, x_max);
                    y_min = std::min(point_proj.y, y_min);
                    y_max = std::max(point_proj.y, y_max);
                }

                glEnd();

                // Second pass: draw quad to color buffer, clear stencil buffer
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glColor4f(splat.color.r, splat.color.g, splat.color.b, 0.1f);
                glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);

                // Draw quad around bounding box
                glBegin(GL_QUADS);
                glVertex2f(x_min, y_min);
                glVertex2f(x_max, y_min);
                glVertex2f(x_max, y_max);
                glVertex2f(x_min, y_max);
                glEnd();

                glDisable(GL_STENCIL_TEST);
            }
        };

        // Draw dried splats to the canvas texture
        if (fixed_splats.size() > 0 && fixed_splats.front().life < -drying_time) {
            glBindFramebuffer(GL_FRAMEBUFFER, bg_fbo);
            glViewport(0, 0, canvas.size.x, canvas.size.y);
            proj = glm::ortho(0.0f, (float)canvas.size.x, 0.0f, (float)canvas.size.y, -1.0f, 1.0f);
            while (fixed_splats.size() > 0 && fixed_splats.front().life < -drying_time) {
                draw_splat(fixed_splats.front(), false);
                fixed_splats.pop_front();
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // Draw canvas
        glViewport(0, 0, win_size.x, win_size.y);
        glClear(GL_COLOR_BUFFER_BIT);
        proj = glm::ortho(0.0f, (float)win_size.x, 0.0f, (float)win_size.y, -1.0f, 1.0f);

        canvas.draw_backdrop(proj);
        if (!display_wet_map) {

            // Draw the canvas texture
            canvas.draw_texture(proj, bg);

            // Draw "live" splats to the canvas
            std::for_each(fixed_splats.begin(), fixed_splats.end(), draw_splat);
            std::for_each(flowing_splats.begin(), flowing_splats.end(), draw_splat);

        } else
            canvas.draw_texture(proj, wet_map);

        // Draw brush
        if (cursor_pos.x > workspace_offset.x && canvas.contains_point(cursor_pos)) {

            // Hide cursor
            window.setMouseCapture(true);

            // Draw a circle
            glColor4f(0.0f, 0.0f, 0.0f, stroke ? 0.8f : 0.6f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < vertices; i++) {
                const float angle = i * 2.0f * glm::pi<float>() / vertices;
                const glm::vec2 point = canvas.clamp_point(glm::vec2(cos(angle) * canvas.zoom * brush_size, sin(angle) * canvas.zoom * brush_size) + cursor_pos);
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