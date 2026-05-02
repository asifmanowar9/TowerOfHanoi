#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;
constexpr int kDefaultDiskCount = 3;
constexpr int kMinDiskCount = 3;
constexpr int kMaxDiskCount = 7;

constexpr float kBaseY = -0.7f;
constexpr float kBaseHeight = 0.05f;
constexpr float kPoleWidth = 0.04f;
constexpr float kPoleHeight = 0.9f;
constexpr float kDiskHeight = 0.09f;
constexpr float kDiskMinWidth = 0.22f;
constexpr float kDiskMaxWidth = 0.5f;

constexpr float kTowerX[3] = {-0.6f, 0.0f, 0.6f};

struct Disk {
    int size;
};

struct GameState {
    std::array<std::vector<Disk>, 3> towers;
    int selectedTower = -1;
    int moveCount = 0;
    int minMoves = 0;
    bool solved = false;
    bool solvedAnnounced = false;
    int diskCount = kDefaultDiskCount;
};

struct MoveAnimation {
    bool active = false;
    Disk disk{0};
    int from = -1;
    int to = -1;
    float elapsed = 0.0f;
    float duration = 0.45f;
    float startX = 0.0f;
    float endX = 0.0f;
    float startY = 0.0f;
    float endY = 0.0f;
    float peakY = 0.0f;
};

struct AppContext {
    GameState state;
    MoveAnimation animation;
};

GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::fprintf(stderr, "Shader compile error: %s\n", infoLog);
    }
    return shader;
}

GLuint CreateProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        std::fprintf(stderr, "Program link error: %s\n", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

void ResetGame(GameState& state) {
    for (auto& tower : state.towers) {
        tower.clear();
    }

    for (int i = state.diskCount; i >= 1; --i) {
        state.towers[0].push_back(Disk{i});
    }

    state.selectedTower = -1;
    state.moveCount = 0;
    state.minMoves = (1 << state.diskCount) - 1;
    state.solved = false;
    state.solvedAnnounced = false;
}

bool CanMove(const GameState& state, int from, int to) {
    if (from < 0 || from > 2 || to < 0 || to > 2 || from == to) {
        return false;
    }
    if (state.towers[from].empty()) {
        return false;
    }
    if (state.towers[to].empty()) {
        return true;
    }
    return state.towers[from].back().size < state.towers[to].back().size;
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float DiskWidth(const GameState& state, int size) {
    int denom = std::max(1, state.diskCount - 1);
    float t = static_cast<float>(size - 1) / static_cast<float>(denom);
    return Lerp(kDiskMinWidth, kDiskMaxWidth, t);
}

float DiskYForIndex(int index) {
    return kBaseY + kBaseHeight * 0.5f + kDiskHeight * 0.5f + index * kDiskHeight;
}

void BeginMove(GameState& state, MoveAnimation& animation, int from, int to) {
    if (!CanMove(state, from, to)) {
        return;
    }

    int sourceIndex = static_cast<int>(state.towers[from].size()) - 1;
    int destIndex = static_cast<int>(state.towers[to].size());

    animation.active = true;
    animation.disk = state.towers[from].back();
    animation.from = from;
    animation.to = to;
    animation.elapsed = 0.0f;
    animation.startX = kTowerX[from];
    animation.endX = kTowerX[to];
    animation.startY = DiskYForIndex(sourceIndex);
    animation.endY = DiskYForIndex(destIndex);
    animation.peakY = kBaseY + kPoleHeight + kDiskHeight * 1.2f;

    state.towers[from].pop_back();
    state.moveCount += 1;
}

void UpdateWindowTitle(GLFWwindow* window, const GameState& state) {
    int extraMoves = std::max(0, state.moveCount - state.minMoves);
    int score = std::max(0, 1000 - extraMoves * 25);
    if (state.solved) {
        score = 1000 - extraMoves * 25;
        if (score < 0) {
            score = 0;
        }
    }

    std::string title = "Tower of Hanoi | Disks: " + std::to_string(state.diskCount) +
                        " ([ ] or +/- to change) | Moves: " + std::to_string(state.moveCount) +
                        " / Min: " + std::to_string(state.minMoves) +
                        " | Score: " + std::to_string(score);
    if (state.solved) {
        title += " | Solved! Press R to restart";
    }
    glfwSetWindowTitle(window, title.c_str());
}

struct RenderContext {
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint colorLocation = -1;
};

RenderContext CreateRenderContext() {
    const char* vertexSource = R"(
        #version 330 core
        layout(location = 0) in vec2 aPosition;
        void main() {
            gl_Position = vec4(aPosition, 0.0, 1.0);
        }
    )";

    const char* fragmentSource = R"(
        #version 330 core
        uniform vec3 uColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(uColor, 1.0);
        }
    )";

    RenderContext context;
    context.program = CreateProgram(vertexSource, fragmentSource);
    context.colorLocation = glGetUniformLocation(context.program, "uColor");

    float quadVertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };

    GLuint indices[] = {0, 1, 2, 2, 3, 0};

    GLuint ebo = 0;
    glGenVertexArrays(1, &context.vao);
    glGenBuffers(1, &context.vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(context.vao);
    glBindBuffer(GL_ARRAY_BUFFER, context.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    return context;
}

void DrawRect(const RenderContext& context, float x, float y, float width, float height,
              float r, float g, float b) {
    glUseProgram(context.program);
    glUniform3f(context.colorLocation, r, g, b);

    glBindVertexArray(context.vao);

    GLint posLocation = 0;
    glEnableVertexAttribArray(posLocation);

    std::array<float, 8> vertices = {
        x - width * 0.5f, y - height * 0.5f,
        x + width * 0.5f, y - height * 0.5f,
        x + width * 0.5f, y + height * 0.5f,
        x - width * 0.5f, y + height * 0.5f
    };

    glBindBuffer(GL_ARRAY_BUFFER, context.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void DrawSegmentDigit(const RenderContext& context, int digit, float x, float y, float scale,
                      float r, float g, float b) {
    static const int kSegments[10] = {
        0b1111110, 0b0110000, 0b1101101, 0b1111001, 0b0110011,
        0b1011011, 0b1011111, 0b1110000, 0b1111111, 0b1111011
    };

    float w = 0.05f * scale;
    float h = 0.12f * scale;
    float t = 0.015f * scale;

    auto draw = [&](float cx, float cy, float ww, float hh) {
        DrawRect(context, cx, cy, ww, hh, r, g, b);
    };

    int mask = kSegments[digit];
    if (mask & 0b1000000) draw(x, y + h, w, t);        // top
    if (mask & 0b0100000) draw(x + w * 0.5f, y + h * 0.5f, t, h); // upper right
    if (mask & 0b0010000) draw(x + w * 0.5f, y - h * 0.5f, t, h); // lower right
    if (mask & 0b0001000) draw(x, y - h, w, t);        // bottom
    if (mask & 0b0000100) draw(x - w * 0.5f, y - h * 0.5f, t, h); // lower left
    if (mask & 0b0000010) draw(x - w * 0.5f, y + h * 0.5f, t, h); // upper left
    if (mask & 0b0000001) draw(x, y, w, t);            // middle
}

void DrawNumber(const RenderContext& context, int value, float x, float y, float scale,
                float r, float g, float b) {
    std::string text = std::to_string(value);
    float spacing = 0.08f * scale;
    float startX = x - spacing * 0.5f * static_cast<float>(text.size() - 1);
    for (size_t i = 0; i < text.size(); ++i) {
        int digit = text[i] - '0';
        DrawSegmentDigit(context, digit, startX + spacing * static_cast<float>(i), y, scale, r, g, b);
    }
}

void RenderHud(const RenderContext& context, const GameState& state) {
    float left = -0.85f;
    float y = 0.85f;
    DrawNumber(context, state.moveCount, left + 0.15f, y, 1.0f, 0.9f, 0.9f, 0.9f);
    DrawNumber(context, state.minMoves, left + 0.45f, y, 0.8f, 0.6f, 0.8f, 0.9f);

    int extraMoves = std::max(0, state.moveCount - state.minMoves);
    int score = std::max(0, 1000 - extraMoves * 25);
    DrawNumber(context, score, left + 0.75f, y, 0.8f, 0.8f, 0.9f, 0.6f);

    DrawNumber(context, state.diskCount, 0.85f, 0.85f, 0.7f, 0.7f, 0.9f, 0.7f);
}

void RenderGame(const RenderContext& context, const GameState& state, const MoveAnimation& animation) {
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    DrawRect(context, 0.0f, kBaseY, 1.8f, kBaseHeight, 0.7f, 0.6f, 0.4f);

    for (int i = 0; i < 3; ++i) {
        float poleColor = (state.selectedTower == i) ? 0.9f : 0.75f;
        DrawRect(context, kTowerX[i], kBaseY + kPoleHeight * 0.5f, kPoleWidth, kPoleHeight,
                 poleColor, poleColor, poleColor);
    }

    for (int towerIndex = 0; towerIndex < 3; ++towerIndex) {
        const auto& tower = state.towers[towerIndex];
        for (int diskIndex = 0; diskIndex < static_cast<int>(tower.size()); ++diskIndex) {
            const Disk& disk = tower[diskIndex];
            float t = static_cast<float>(disk.size - 1) / static_cast<float>(std::max(1, state.diskCount - 1));
            float width = DiskWidth(state, disk.size);
            float y = DiskYForIndex(diskIndex);
            float hue = 0.2f + 0.6f * (1.0f - t);
            float r = 0.2f + 0.6f * hue;
            float g = 0.3f + 0.5f * (1.0f - t);
            float b = 0.7f - 0.5f * hue;

            DrawRect(context, kTowerX[towerIndex], y, width, kDiskHeight * 0.9f, r, g, b);
        }
    }

    if (animation.active) {
        float t = static_cast<float>(animation.disk.size - 1) /
                  static_cast<float>(std::max(1, state.diskCount - 1));
        float width = DiskWidth(state, animation.disk.size);
        float hue = 0.2f + 0.6f * (1.0f - t);
        float r = 0.2f + 0.6f * hue;
        float g = 0.3f + 0.5f * (1.0f - t);
        float b = 0.7f - 0.5f * hue;

        float phaseUp = 0.3f;
        float phaseAcross = 0.4f;
        float tNorm = std::min(1.0f, animation.elapsed / animation.duration);
        float x = animation.startX;
        float y = animation.startY;
        if (tNorm < phaseUp) {
            float local = tNorm / phaseUp;
            y = Lerp(animation.startY, animation.peakY, local);
        } else if (tNorm < phaseUp + phaseAcross) {
            float local = (tNorm - phaseUp) / phaseAcross;
            x = Lerp(animation.startX, animation.endX, local);
            y = animation.peakY;
        } else {
            float local = (tNorm - phaseUp - phaseAcross) / (1.0f - phaseUp - phaseAcross);
            x = animation.endX;
            y = Lerp(animation.peakY, animation.endY, local);
        }

        DrawRect(context, x, y, width, kDiskHeight * 0.9f, r, g, b);
    }

    RenderHud(context, state);
}

int TowerFromKey(int key) {
    if (key == GLFW_KEY_1) return 0;
    if (key == GLFW_KEY_2) return 1;
    if (key == GLFW_KEY_3) return 2;
    return -1;
}

void HandleInput(GameState& state, MoveAnimation& animation, int key) {
    if (state.solved) {
        if (key == GLFW_KEY_ESCAPE) {
            state.selectedTower = -1;
        }
        return;
    }

    if (animation.active) {
        return;
    }

    int tower = TowerFromKey(key);
    if (tower >= 0) {
        if (state.selectedTower < 0) {
            state.selectedTower = tower;
        } else {
            BeginMove(state, animation, state.selectedTower, tower);
            state.selectedTower = -1;
        }
    }

    if (key == GLFW_KEY_ESCAPE) {
        state.selectedTower = -1;
    }
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) {
        return;
    }

    auto* app = static_cast<AppContext*>(glfwGetWindowUserPointer(window));
    if (!app) {
        return;
    }

    if (key == GLFW_KEY_R) {
        ResetGame(app->state);
        app->animation.active = false;
        return;
    }

    if ((key == GLFW_KEY_LEFT_BRACKET || key == GLFW_KEY_MINUS) && !app->animation.active) {
        app->state.diskCount = std::max(kMinDiskCount, app->state.diskCount - 1);
        ResetGame(app->state);
        app->animation.active = false;
        return;
    }

    if ((key == GLFW_KEY_RIGHT_BRACKET || key == GLFW_KEY_EQUAL) && !app->animation.active) {
        app->state.diskCount = std::min(kMaxDiskCount, app->state.diskCount + 1);
        ResetGame(app->state);
        app->animation.active = false;
        return;
    }

    HandleInput(app->state, app->animation, key);
}
}  // namespace

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Tower of Hanoi", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, KeyCallback);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::fprintf(stderr, "Failed to initialize GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    AppContext app;
    ResetGame(app.state);
    glfwSetWindowUserPointer(window, &app);

    RenderContext context = CreateRenderContext();
    glViewport(0, 0, kWindowWidth, kWindowHeight);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        static double lastTime = glfwGetTime();
        double now = glfwGetTime();
        float delta = static_cast<float>(now - lastTime);
        lastTime = now;

        if (app.animation.active) {
            app.animation.elapsed += delta;
            if (app.animation.elapsed >= app.animation.duration) {
                app.state.towers[app.animation.to].push_back(app.animation.disk);
                app.animation.active = false;

                if (static_cast<int>(app.state.towers[2].size()) == app.state.diskCount) {
                    app.state.solved = true;
                    if (!app.state.solvedAnnounced) {
                        std::puts("Solved! Press R to restart.");
                        app.state.solvedAnnounced = true;
                    }
                }
            }
        }

        UpdateWindowTitle(window, app.state);
        RenderGame(context, app.state, app.animation);
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
