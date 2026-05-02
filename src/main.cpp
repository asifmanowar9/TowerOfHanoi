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
constexpr int kDiskCount = 3;

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

    for (int i = kDiskCount; i >= 1; --i) {
        state.towers[0].push_back(Disk{i});
    }

    state.selectedTower = -1;
    state.moveCount = 0;
    state.minMoves = (1 << kDiskCount) - 1;
    state.solved = false;
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

void MoveDisk(GameState& state, int from, int to) {
    if (!CanMove(state, from, to)) {
        return;
    }
    state.towers[to].push_back(state.towers[from].back());
    state.towers[from].pop_back();
    state.moveCount += 1;

    if (static_cast<int>(state.towers[2].size()) == kDiskCount) {
        state.solved = true;
    }
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
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

    std::string title = "Tower of Hanoi | Moves: " + std::to_string(state.moveCount) +
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
    float transform[4] = {
        width, 0.0f,
        0.0f, height
    };

    float tx = x;
    float ty = y;

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

void RenderGame(const RenderContext& context, const GameState& state) {
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
            float t = static_cast<float>(disk.size - 1) / (kDiskCount - 1);
            float width = Lerp(kDiskMinWidth, kDiskMaxWidth, t);
            float y = kBaseY + kBaseHeight * 0.5f + kDiskHeight * 0.5f + diskIndex * kDiskHeight;
            float hue = 0.2f + 0.6f * (1.0f - t);
            float r = 0.2f + 0.6f * hue;
            float g = 0.3f + 0.5f * (1.0f - t);
            float b = 0.7f - 0.5f * hue;

            DrawRect(context, kTowerX[towerIndex], y, width, kDiskHeight * 0.9f, r, g, b);
        }
    }
}

int TowerFromKey(int key) {
    if (key == GLFW_KEY_1) return 0;
    if (key == GLFW_KEY_2) return 1;
    if (key == GLFW_KEY_3) return 2;
    return -1;
}

void HandleInput(GameState& state, int key) {
    int tower = TowerFromKey(key);
    if (tower >= 0) {
        if (state.selectedTower < 0) {
            state.selectedTower = tower;
        } else {
            MoveDisk(state, state.selectedTower, tower);
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

    GameState* state = static_cast<GameState*>(glfwGetWindowUserPointer(window));
    if (!state) {
        return;
    }

    if (key == GLFW_KEY_R) {
        ResetGame(*state);
        return;
    }

    HandleInput(*state, key);
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

    GameState state;
    ResetGame(state);
    glfwSetWindowUserPointer(window, &state);

    RenderContext context = CreateRenderContext();
    glViewport(0, 0, kWindowWidth, kWindowHeight);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        UpdateWindowTitle(window, state);
        RenderGame(context, state);
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
