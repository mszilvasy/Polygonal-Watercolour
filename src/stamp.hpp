#include <array>

void add_water(const Canvas& canvas, const glm::vec2& pos, int vertices, float r)
{
    glBegin(GL_TRIANGLE_FAN);
    for (int i = 0; i < vertices; i++) {
        const float angle = i * 2.0f * glm::pi<float>() / vertices;
        const glm::vec2 dir = glm::vec2(std::cos(angle), std::sin(angle));
        const glm::vec2 point = pos + r * dir;
        const glm::vec2 point_proj = canvas.proj * glm::vec4(point, 0.0f, 1.0f);
        glColor4f((dir.x + 1.0f) / 2.0f, (dir.y + 1.0f) / 2.0f, 0.0f, 1.0f);
        glVertex2f(point_proj.x, point_proj.y);
    }
    glEnd();
}

struct Stamp {

    virtual void place(std::list<Splat>* splats, const Canvas& canvas, const glm::vec2& pos, const glm::vec3& color, float size, float roughness, float flow, int stroke_id, int lifetime, int n_vertices) = 0;

    virtual void wet_canvas(const Canvas& canvas, const glm::vec2& pos, int vertices, float brush_size) { add_water(canvas, pos, vertices, brush_size); }

    virtual void menu() {}
};

struct Simple : Stamp {
    void place(std::list<Splat>* splats, const Canvas& canvas, const glm::vec2& pos, const glm::vec3& color, float size, float roughness, float flow, int stroke_id, int lifetime, int n_vertices) override
    {
        const glm::vec4 color_a = glm::vec4(color, 0.1f);
        splats->emplace_back(canvas, pos, color_a, size, roughness, flow, stroke_id, lifetime, n_vertices);
    }
};

struct Crunchy : Stamp {
    // Using this as a "Simple+", as the crunchy brush described in the paper can already be achieved by adjusting roughness and flow on the simple brush, with the only missing component being the scale factor.
    float scale = 1.0f;

    void place(std::list<Splat>* splats, const Canvas& canvas, const glm::vec2& pos, const glm::vec3& color, float size, float roughness, float flow, int stroke_id, int lifetime, int n_vertices) override
    {
        const glm::vec4 color_a = glm::vec4(color, 0.1f);
        splats->emplace_back(canvas, pos, color_a, scale * size, roughness, flow, stroke_id, lifetime, n_vertices);
    }

    void menu() override
    {
        ImGui::SliderFloat("Scale", &scale, 0.25f, 1.0f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The size of the splat relative to the wetting region.\nLower this below 1.0, reduce flow and increase roughness to achieve\nthe effect of the \"crunchy\" brush described in the paper.");
    }
};

struct WetOnDry : Stamp {

    int lobes = 6;
    float b = 0.05f;

    void place(std::list<Splat>* splats, const Canvas& canvas, const glm::vec2& pos, const glm::vec3& color, float size, float roughness, float flow, int stroke_id, int lifetime, int n_vertices) override
    {
        const glm::vec4 color_a = glm::vec4(color, 0.02f);
        const float r = 0.5f * size;
        splats->emplace_back(canvas, pos, color_a, r, roughness, flow, stroke_id, lifetime, n_vertices);
        for (int i = 0; i < lobes; i++) {
            const float angle = i * 2.0f * glm::pi<float>() / lobes;
            const glm::vec2 offset = r * glm::vec2(std::cos(angle), std::sin(angle));
            const glm::vec2 bias = b * offset;
            splats->emplace_back(canvas, pos + offset, color_a, r, roughness, flow, stroke_id, lifetime, n_vertices, bias);
        }
    }

    void menu() override
    {
        ImGui::SliderInt("Lobes", &lobes, 2, 12);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The number of extra splats to be placed around the centre.\nThis is fixed at 6 in the brush described by the paper.");
        ImGui::SliderFloat("Bias", &b, 0.0f, 0.2f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The outer splats have an outward motion bias.\nAdjust this factor to the brush size and lifetime.");
    }
};

struct WetOnWet : Stamp {

    float scale = 1.5f;

    void place(std::list<Splat>* splats, const Canvas& canvas, const glm::vec2& pos, const glm::vec3& color, float size, float roughness, float flow, int stroke_id, int lifetime, int n_vertices) override
    {
        const glm::vec4 color_a = glm::vec4(color, 0.05f);
        splats->emplace_back(canvas, pos, color_a, scale * size, roughness, flow, stroke_id, lifetime, n_vertices);
        splats->emplace_back(canvas, pos, color_a, 0.5 * size, roughness, flow, stroke_id, lifetime, n_vertices);
    }

    void wet_canvas(const Canvas& canvas, const glm::vec2& pos, int vertices, float brush_size) override
    {
        for (int i = 0; i < 4; i++) {
            const float angle = (i * 0.5f + 0.25f) * glm::pi<float>();
            const glm::vec2 dir = glm::vec2(std::cos(angle), std::sin(angle));
            const glm::vec2 point = pos + scale * brush_size * dir;
            add_water(canvas, point, vertices, 2.0f * brush_size);
        }
    }

    void menu() override
    {
        ImGui::SliderFloat("Scale", &scale, 0.5f, 2.0f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The relative size of the outer of the two splats.\nEffectively fixed at 1.5 in the brush described by the paper.");
    }
};

struct Blobby : Stamp {

    float offset = 1.0f;
    std::array<float, 4> sizes { 0.5f, 0.5f, 0.5f, 0.5f };

    void place(std::list<Splat>* splats, const Canvas& canvas, const glm::vec2& pos, const glm::vec3& color, float size, float roughness, float flow, int stroke_id, int lifetime, int n_vertices) override
    {
        const glm::vec4 color_a = glm::vec4(color, 0.025f);
        for (int i = 0; i < 4; i++) {
            sizes[i] = U(0.33f, 1.0f);
            const float angle = i * 0.5f * glm::pi<float>();
            const glm::vec2 point = pos + offset * size * glm::vec2(std::cos(angle), std::sin(angle));
            splats->emplace_back(canvas, point, color_a, sizes[i] * size, roughness, flow, stroke_id, lifetime, n_vertices);
        }
    }

    void wet_canvas(const Canvas& canvas, const glm::vec2& pos, int vertices, float brush_size) override
    {
        for (int i = 0; i < 4; i++) {
            const float angle = i * 0.5f * glm::pi<float>();
            const glm::vec2 dir = glm::vec2(std::cos(angle), std::sin(angle));
            const glm::vec2 point = pos + offset * brush_size * dir;
            add_water(canvas, point, vertices, sizes[i] * brush_size);
        }
    }

    void menu() override
    {
        ImGui::SliderFloat("Offset", &offset, 0.0f, 1.5f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The offset of the splats from the centre of the stroke.");
    }
};
