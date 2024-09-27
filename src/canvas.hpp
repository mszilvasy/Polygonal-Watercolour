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

    // Draw the canvas using a projection matrix proj with the given background color
    void draw(glm::mat4 proj, glm::vec4 color) const
    {
        const glm::vec4 canvas_lower_left = proj * glm::vec4(pos, 0.0f, 1.0f);
        const glm::vec4 canvas_upper_right = proj * glm::vec4(pos + zoom * size, 0.0f, 1.0f);
        const glm::vec4 backdrop_lower_left = proj * glm::vec4(pos + backdrop_offset, 0.0f, 1.0f);
        const glm::vec4 backdrop_upper_right = proj * glm::vec4(pos + zoom * size + backdrop_offset, 0.0f, 1.0f);

        // Backdrop effect (purely aesthetic)
        glColor4f(backdrop_color.r, backdrop_color.g, backdrop_color.b, backdrop_color.a);
        glBegin(GL_QUADS);
        glVertex2f(backdrop_lower_left.x, backdrop_lower_left.y);
        glVertex2f(backdrop_upper_right.x, backdrop_lower_left.y);
        glVertex2f(backdrop_upper_right.x, backdrop_upper_right.y);
        glVertex2f(backdrop_lower_left.x, backdrop_upper_right.y);
        glEnd();

        // Canvas proper
        glColor4f(color.r, color.g, color.b, color.a);
        glBegin(GL_QUADS);
        glVertex2f(canvas_lower_left.x, canvas_lower_left.y);
        glVertex2f(canvas_upper_right.x, canvas_lower_left.y);
        glVertex2f(canvas_upper_right.x, canvas_upper_right.y);
        glVertex2f(canvas_lower_left.x, canvas_upper_right.y);
        glEnd();
    }
};