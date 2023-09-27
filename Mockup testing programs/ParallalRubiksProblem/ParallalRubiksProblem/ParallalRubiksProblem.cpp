#include <iostream>
#include <vector>
#include <chrono>
#include <queue>
#include <thread>
#include <mutex>

// Define the number of faces and moves
const int NUM_FACES = 10;
const int NUM_MOVES = 12;

// Define the Rubik's Cube as a 3x3x3 matrix
const int CUBE_SIZE = 10;
const int NUM_CUBES = CUBE_SIZE * CUBE_SIZE * CUBE_SIZE;

// Define cube rotations (clockwise)
const int ROTATIONS[NUM_MOVES][4] = {
    {0, 1, 3, 2}, {0, 1, 5, 4}, {1, 3, 7, 5}, {3, 2, 6, 7},
    {2, 0, 4, 6}, {4, 5, 7, 6}, {0, 2, 3, 1}, {0, 4, 5, 1},
    {2, 3, 7, 6}, {0, 1, 5, 4}, {1, 3, 7, 5}, {2, 0, 4, 6}
};

// Define cube colors
enum Color { WHITE, YELLOW, RED, ORANGE, GREEN, BLUE };

// Define a cube
struct Cube {
    std::vector<Color> faces[NUM_FACES];
};

// Function to initialize a solved cube
Cube initCube() {
    Cube cube;
    for (int i = 0; i < NUM_FACES; ++i) {
        cube.faces[i].resize(NUM_CUBES, static_cast<Color>(i));
    }
    return cube;
}

// Function to print the cube
void printCube(const Cube& cube) {
    for (int face = 0; face < NUM_FACES; ++face) {
        std::cout << "Face " << face << ":\n";
        for (int i = 0; i < NUM_CUBES; ++i) {
            std::cout << cube.faces[face][i] << " ";
            if ((i + 1) % CUBE_SIZE == 0) {
                std::cout << "\n";
            }
        }
        std::cout << "\n";
    }
}

// Function to perform a clockwise rotation of a cube face
void rotateFace(Cube& cube, int face) {
    std::vector<Color> temp = cube.faces[face];
    for (int i = 0; i < NUM_CUBES; ++i) {
        cube.faces[face][i] = temp[ROTATIONS[face][i]];
    }
}

// Function to check if the cube is solved
bool isSolved(const Cube& cube) {
    // Implement your cube solved condition here
    // For a solved cube, each face should have all the same color
    for (int face = 0; face < NUM_FACES; ++face) {
        Color firstColor = cube.faces[face][0];
        for (int i = 1; i < NUM_CUBES; ++i) {
            if (cube.faces[face][i] != firstColor) {
                return false;
            }
        }
    }
    return true;
}

// Function to solve the Rubik's Cube using BFS in parallel
bool solveCube(Cube startCube, std::vector<int>& solution, int numThreads) {
    std::queue<Cube> q;
    std::queue<std::vector<int>> moveQueue;
    std::mutex mtx;

    q.push(startCube);
    moveQueue.push({});

    bool solved = false;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            while (!solved) {
                mtx.lock();
                if (q.empty()) {
                    mtx.unlock();
                    break;
                }
                Cube cube = q.front();
                q.pop();
                std::vector<int> moves = moveQueue.front();
                moveQueue.pop();
                mtx.unlock();

                if (isSolved(cube)) {
                    mtx.lock();
                    solution = moves;
                    solved = true;
                    mtx.unlock();
                    break;
                }

                for (int move = 0; move < NUM_MOVES; ++move) {
                    Cube nextCube = cube;
                    rotateFace(nextCube, move);

                    std::vector<int> nextMoves = moves;
                    nextMoves.push_back(move);

                    mtx.lock();
                    q.push(nextCube);
                    moveQueue.push(nextMoves);
                    mtx.unlock();
                }
            }
            });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    return solved;
}

int main() {

    Cube cube = initCube();
    std::vector<int> solution;

    int numThreads = 4; // Adjust the number of threads as needed

    auto startTime = std::chrono::high_resolution_clock::now();

    bool solved = solveCube(cube, solution, numThreads);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    if (solved) {
        std::cout << "Rubik's Cube Solved in " << solution.size() << " moves:\n";
        for (int move : solution) {
            std::cout << move << " ";
        }
        std::cout << "\n";

        // Print the solved cube for verification
        //printCube(cube);
        std::cout << "Execution Time: " << duration.count() << " microseconds\n";
    }
    else {
        std::cout << "Rubik's Cube cannot be solved.\n";
    }

    return 0;
}
