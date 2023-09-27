#include <iostream>
#include <vector>
#include <chrono>
#include <queue>
#include <thread>
#include <mutex>
#include <string>
#include <mpi.h>
#include <condition_variable>

// Define cube colors
const int WHITE = 0;
const int YELLOW = 1;
const int RED = 2;
const int ORANGE = 3;
const int GREEN = 4;
const int BLUE = 5;

std::string getColorName(int color) {
    switch (color) {
    case WHITE: return "WHITE";
    case YELLOW: return "YELLOW";
    case RED: return "RED";
    case ORANGE: return "ORANGE";
    case GREEN: return "GREEN";
    case BLUE: return "BLUE";
    default: return "UNKNOWN";
    }
}

// Define a cube
struct Cube {
    std::vector<std::vector<std::vector<int>>> faces;  // Use a 3D vector for variable dimensions
};

// Function to initialize a cube with user-defined colors
Cube initUserCube(int size) {
    Cube cube;
    cube.faces.resize(6, std::vector<std::vector<int>>(size, std::vector<int>(size)));

    for (int face = 0; face < 6; ++face) {
        std::cout << "Enter colors for Face " << face << ":\n";
        std::cout << "Key: (0 - WHITE, 1 - YELLOW, 2 - RED, 3 - ORANGE, 4 - GREEN, 5 - BLUE)\n";

        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                int color;
                std::cout << "Enter color for row " << i << ", column " << j << ": ";
                std::cin >> color;

                if (color < 0 || color > 5) {
                    std::cout << "Invalid color entered. Please enter a number between 0 and 5.\n";
                    --j;  // Decrement j to re-enter the color for this cell.
                }
                else {
                    cube.faces[face][i][j] = color;
                }
            }
        }
    }
    return cube;
}

// Function to perform a clockwise rotation of a cube face
std::pair<int, std::string> rotateFace(Cube& cube, int face, int size) {
    std::vector<std::vector<int>> temp = cube.faces[face];
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            cube.faces[face][i][j] = temp[size - j - 1][i];
        }
    }
    std::string rotationDirection = (size % 2 == 0) ? "RIGHT" : "LEFT";
    return std::make_pair(face, rotationDirection);
}

// Function to display the Rubik's Cube template
void displayCubeTemplate(const Cube& cube) {
    int size = cube.faces[0].size();

    // Display the cube template
    for (int face = 0; face < 6; ++face) {
        std::cout << "Face " << face << ":\n";
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                std::cout << getColorName(cube.faces[face][i][j]) << " ";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
}


bool isCubeSolved(const Cube& cube) {
    int size = cube.faces[0].size();
    int numFaces = cube.faces.size();

    // Check each face to see if all cells have the same color
    for (int face = 0; face < numFaces; ++face) {
        int firstColor = cube.faces[face][0][0];
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                if (cube.faces[face][i][j] != firstColor) {
                    return false; // Cube is not solved
                }
            }
        }
    }

    return true; // Cube is solved
}


Cube solveCube(Cube& startCube, int numThreads) {
    int size = startCube.faces[0].size();
    int numFaces = startCube.faces.size();
    int totalNumberOfPossibleCubeStates = 1;
    Cube bestSolution;

    // Initialize MPI  and get rank
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Divide the cube's search space among MPI processes
    int numProcesses;
    MPI_Comm_size(MPI_COMM_WORLD, &numProcesses);
    int startIndex = rank * (totalNumberOfPossibleCubeStates / numProcesses);
    int endIndex = (rank + 1) * (totalNumberOfPossibleCubeStates / numProcesses);

    Cube solutionCube; // Store the solved cube
    std::vector<int> solutionSteps; // Store the steps to reach the solution
    std::vector<int> rotationSteps; // Store rotation steps

    // Create a queue for BFS
    std::queue<Cube> q;
    std::queue<std::vector<int>> stepQueue;
    std::queue<std::vector<int>> rotationQueue;

    // Push the initial cube and corresponding steps into the queue for the current process
    if (startIndex == 0) {
        q.push(startCube);
        stepQueue.push({});
        rotationQueue.push({});
    }

    // Create a mutex for thread synchronization
    std::mutex mtx;

    // Create a condition variable for thread synchronization
    std::condition_variable cv;

    // Create a flag to signal when the solution is found
    bool solved = false;

    // Create a thread pool
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            while (true) {
                Cube cube;
                std::vector<int> steps;
                std::vector<int> rotations;

                {
                    std::unique_lock<std::mutex> lock(mtx);

                    // Check if the queue is empty
                    if (q.empty()) {
                        // Release the lock and exit the thread
                        lock.unlock();
                        break;
                    }

                    // Pop a cube state, steps, and rotations from the queue
                    cube = q.front();
                    q.pop();
                    steps = stepQueue.front();
                    stepQueue.pop();
                    rotations = rotationQueue.front();
                    rotationQueue.pop();
                }

                // Check if the cube is solved
                if (isCubeSolved(cube)) {
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        solutionCube = cube; // Store the solved cube
                        solutionSteps = steps; // Store the steps
                        rotationSteps = rotations; // Store rotation steps
                        solved = true;
                    }
                    cv.notify_one(); // Notify waiting threads that the solution is found
                    break;
                }

                // Explore neighboring cube states and add them to the queue
                for (int face = 0; face < numFaces; ++face) {
                    Cube nextCube = cube;
                    auto rotation = rotateFace(nextCube, face, size);
                    int faceNumber = rotation.first;
                    std::string rotationDirection = rotation.second;

                    std::vector<int> nextSteps = steps;
                    nextSteps.push_back(face);

                    std::vector<int> nextRotations = rotations;
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        q.push(nextCube);
                        stepQueue.push(nextSteps);
                        nextRotations.push_back(face); // Add the face rotation as an integer
                    }
                }
            }
            });
    }

    // Wait for all threads to finish
    for (std::thread& thread : threads) {
        thread.join();
    }

    // Gather the solution cubes, steps, and rotations from all processes to rank 0
    if (rank == 0) {
        std::vector<Cube> allSolutions(numProcesses);
        std::vector<std::vector<int>> allSolutionSteps(numProcesses);
        std::vector<std::vector<int>> allRotationSteps(numProcesses);

        allSolutions[0] = solutionCube;
        allSolutionSteps[0] = solutionSteps;
        allRotationSteps[0] = rotationSteps;

        for (int i = 1; i < numProcesses; ++i) {
            // Receive solutions, steps, and rotations from other processes
            MPI_Recv(&allSolutions[i], sizeof(Cube), MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Receive solution steps
            int numSteps;
            MPI_Recv(&numSteps, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::vector<int> steps(numSteps);
            MPI_Recv(steps.data(), numSteps, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            allSolutionSteps[i] = steps;

            // Receive rotation steps
            MPI_Recv(&numSteps, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::vector<int> rotations(numSteps);
            MPI_Recv(rotations.data(), numSteps, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            allRotationSteps[i] = rotations;
        }

        // Determine the best solution from allSolutions based on your criteria

        // Broadcast the best solution to all processes
        for (int i = 1; i < numProcesses; ++i) {
            MPI_Send(&bestSolution, sizeof(Cube), MPI_BYTE, i, 0, MPI_COMM_WORLD);

            // Send solution steps
            int numSteps = allSolutionSteps[i].size();
            MPI_Send(&numSteps, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(allSolutionSteps[i].data(), numSteps, MPI_INT, i, 0, MPI_COMM_WORLD);

            // Send rotation steps
            numSteps = allRotationSteps[i].size();
            MPI_Send(&numSteps, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(allRotationSteps[i].data(), numSteps, MPI_INT, i, 0, MPI_COMM_WORLD);
        }

        return bestSolution;
    }
    else {
        // Send the solution, steps, and rotations to rank 0
        MPI_Send(&solutionCube, sizeof(Cube), MPI_BYTE, 0, 0, MPI_COMM_WORLD);

        // Send solution steps
        int numSteps = solutionSteps.size();
        MPI_Send(&numSteps, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Send(solutionSteps.data(), numSteps, MPI_INT, 0, 0, MPI_COMM_WORLD);

        // Send rotation steps
        numSteps = rotationSteps.size();
        MPI_Send(&numSteps, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Send(rotationSteps.data(), numSteps, MPI_INT, 0, 0, MPI_COMM_WORLD);

        // Receive the best solution from rank 0
        MPI_Recv(&bestSolution, sizeof(Cube), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        return bestSolution;
    }
}


int getUserChoice() {
    int choice;
    std::cout << "Rubik's Cube Solver Menu:\n";
    std::cout << "1. Solve the cube\n";
    std::cout << "2. Exit\n";
    std::cout << "Enter your choice (1/2): ";
    std::cin >> choice;
    return choice;
}


// Function to display the solution steps, including the direction of rotation
void displaySolution(const std::vector<std::pair<int, std::string>>& solution) {
    std::cout << "Solution Steps:\n";
    for (int i = 0; i < solution.size(); ++i) {
        int faceNumber = solution[i].first;
        std::string rotationDirection = solution[i].second;
        std::cout << "Step " << i + 1 << ": Rotate face " << faceNumber << " " << rotationDirection << "\n";
    }
}


int main(int argc, char** argv) {
    int numThreads = 4; // Adjust the number of threads as needed
    int numProcesses = numThreads; // Set the number of MPI processes equal to the number of threads

    MPI_Init(&argc, &argv);
    int rank;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int size; // Size of the Rubik's Cube

    while (true) {
        int choice = getUserChoice(); 

        if (choice == 1) {
            std::cout << "Enter the size of the Rubik's Cube (e.g., 2 for a 2x2x2 cube): ";
            std::cin >> size;

            // Initialize the Rubik's Cube with user-defined colors
            Cube cube = initUserCube(size);

            displayCubeTemplate(cube);

            std::vector<int> solution;
            std::vector<Cube> steps; // Store cube states at each step
            std::vector<int> rotationSteps; // Store rotation steps


            // Call cube-solving function h
            solveCube(cube, numThreads);

            if (isCubeSolved(cube)) {
                std::cout << "Rubik's Cube Solved!\n";
                std::vector<std::pair<int, std::string>> rotationPairs;
                for (int i = 0; i < rotationSteps.size(); ++i) {
                    rotationPairs.emplace_back(rotationSteps[i], getColorName(rotationSteps[i]));
                }
                displaySolution(rotationPairs);
            }
            else {
               std::cout << "Rubik's Cube cannot be solved.\n";
            }
        }
        else if (choice == 2) {
            break; // Exit the program
        }
        else {
            std::cout << "Invalid choice. Please enter 1 or 2.\n";
        }
    }

    MPI_Finalize();

    return 0;
}
