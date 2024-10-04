const glm::vec4 backdrop_color { 0.21f, 0.21f, 0.21f, 1.0f };
const glm::vec2 backdrop_offset { 4.0f, -4.0f };

struct Canvas {

    glm::vec2 pos, size;
    float zoom = 1.0f;
    Canvas(glm::vec2 pos, glm::vec2 size)
        : pos(pos)
        , size(size)
    {
    }

    // Return true iff a point is inside the canvas
    bool contains_point(glm::vec2 point) const
    {
        return point.x >= pos.x && point.x <= pos.x + zoom * size.x && point.y >= pos.y && point.y <= pos.y + zoom * size.y;
    }

    // Return true iff a point, given in canvas coordinates, is inside the canvas
    bool contains_canvas_point(glm::vec2 point) const
    {
        return point.x >= 0 && point.x <= size.x && point.y >= 0 && point.y <= size.y;
    }

    // Convert window coordinates to canvas coordinates
    glm::vec2 canvas_coords(glm::vec2 point) const
    {
        return (point - pos) / zoom;
    }

    // Convert canvas coordinates to window coordinates
    glm::vec2 window_coords(glm::vec2 point) const
    {
        return pos + zoom * point;
    }

    // Clamp a point to the canvas
    glm::vec2 clamp_point(glm::vec2 point) const
    {
        return glm::clamp(point, pos, pos + zoom * size);
    }

    // Clamp a point, given in canvas coordinates, to the canvas
    glm::vec2 clamp_canvas_point(glm::vec2 point) const
    {
        return glm::clamp(point, glm::vec2(0.0f, 0.0f), glm::vec2(size.x - 0.0001f, size.y - 0.0001f));
    }

    // Draw backdrop effect
    void draw_backdrop(glm::mat4 proj) const
    {
        const glm::vec4 backdrop_lower_left = proj * glm::vec4(pos + backdrop_offset, 0.0f, 1.0f);
        const glm::vec4 backdrop_upper_right = proj * glm::vec4(pos + zoom * size + backdrop_offset, 0.0f, 1.0f);

        glColor4f(backdrop_color.r, backdrop_color.g, backdrop_color.b, backdrop_color.a);
        glBegin(GL_QUADS);
        glVertex2f(backdrop_lower_left.x, backdrop_lower_left.y);
        glVertex2f(backdrop_upper_right.x, backdrop_lower_left.y);
        glVertex2f(backdrop_upper_right.x, backdrop_upper_right.y);
        glVertex2f(backdrop_lower_left.x, backdrop_upper_right.y);
        glEnd();
    }

    // Draw a texture to the canvas
    void draw_texture(glm::mat4 proj, GLuint texture, float alpha = 1.0f) const
    {
        const glm::vec4 canvas_lower_left = proj * glm::vec4(pos, 0.0f, 1.0f);
        const glm::vec4 canvas_upper_right = proj * glm::vec4(pos + zoom * size, 0.0f, 1.0f);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(canvas_lower_left.x, canvas_lower_left.y);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(canvas_upper_right.x, canvas_lower_left.y);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(canvas_upper_right.x, canvas_upper_right.y);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(canvas_lower_left.x, canvas_upper_right.y);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }
};