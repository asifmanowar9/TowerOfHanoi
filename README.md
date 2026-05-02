# Tower of Hanoi (OpenGL + GLFW)

An interactive Tower of Hanoi puzzle implemented in C++ with OpenGL and GLFW. The game renders a stylized 2D scene, animates disk moves, enforces legal moves, and scores the player based on efficiency compared to the theoretical minimum.

## Features
- Keyboard-only interaction with clear rules (move top disk only).
- Adjustable disk count (3 to 7) at runtime.
- Smooth move animation (lift, slide, drop).
- Move counter, minimum-move target, and live score display.
- Win state detection with restart prompt.

## Controls
- `1`, `2`, `3`: Select source tower, then select destination tower.
- `[` or `-`: Decrease disk count (min 3).
- `]` or `=`: Increase disk count (max 7).
- `Esc`: Cancel current selection.
- `R`: Restart the game.

## Scoring
For $n$ disks, the minimum number of moves is:

$$M = 2^n - 1$$

Score is computed as:

$$Score = MaxPoints - ((S_{act} - M) \times P)$$

Where:
- $S_{act}$ is the actual number of moves taken.
- $MaxPoints = 1000$.
- $P = 25$ points per extra move.

Scores are clamped at 0.

## Project Structure
```
TowerOfHanoi/
├─ CMakeLists.txt
├─ README.md
├─ external/
│  ├─ glad/
│  └─ std/
└─ src/
   └─ main.cpp
```

## Build Requirements (Linux)
- CMake 3.16+
- C++17 compiler (GCC or Clang)
- GLFW development package

Install GLFW on Debian/Ubuntu:
```bash
sudo apt install libglfw3-dev
```

## Build and Run
```bash
cmake -S . -B build
cmake --build build
./build/tower_of_hanoi
```

## Notes
- The included GLFW folder is a Windows binary distribution and is not used on Linux.
- The renderer uses OpenGL 3.3 core profile.

## Roadmap Ideas
- On-screen text labels for HUD values.
- Mouse drag support for disk moves.
- True 3D rendering (camera, depth, cylinders).
- Custom themes and disk textures.

## License
This project is provided as-is for educational use.
