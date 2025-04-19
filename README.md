# Parallel Sudoku Solver

## Introduction

This report investigates and compares three parallel approaches for solving 9x9 Sudoku puzzles with a unique solution, utilizing multithreading to speed up the solution process. The methods include two Pthread-based approaches (one classic DFS and one hybrid DFS/BFS) and an OpenMP-based BFS approach. Each solution leverages parallelism in different ways to address the challenge of Sudoku puzzle-solving.

The first Pthread-based solution implements a fully recursive depth-first search (DFS) approach in parallel. The second, a hybrid Pthread solution, introduces partial breadth-first search (BFS) behavior by allowing multiple task delegations while retaining the DFS structure. The third solution, implemented with OpenMP, follows a BFS strategy and parallelizes where possible.

This document compares the three methods in terms of performance, scalability, and memory usage, describing their key features, advantages, and limitations. The solutions are tested using a shared dataset of 9x9 Sudoku puzzles with unique solutions. The goal is to understand the trade-offs between the methods based on puzzle complexity and the number of available threads.

## 1. Sudoku Solver with Pthread - Classic DFS

This solution uses a **Depth-First Search (DFS)** traversal approach. The algorithm performs a DFS on the possible Sudoku configurations, using a user-defined number of threads. The goal is to find the valid solution by distributing the computational workload among threads in parallel.

### Key Features
- **DFS Approach**: DFS is effective when the solution lies deeper in the tree.
- **Threads**: Each thread explores different paths recursively.
- **Shared Data Structures**: Uses semaphores, mutexes, and a shared FIFO queue to manage idle and active threads.

### Key Variables
- **problem_solved**: Flag indicating whether a valid solution has been found.
- **sudokus_to_solve**: Array of Sudoku puzzles pending validation.
- **semaphores**: Array of semaphores, one per thread.
- **mutex_shared_data**: Mutex controlling access to the FIFO.
- **shared_fifo_idle_threads**: Shared FIFO queue of idle threads.
- **mutex_solved_sudoku**: Mutex protecting access to the solved Sudoku.
- **solved_sudoku**: Structure that holds the valid solution.

### Thread Roles
- **Idle Validator**: Thread waiting for work. It is pushed into a shared FIFO and blocked on a semaphore until activated.
- **Active Validator**: A working thread performing DFS to validate and solve a Sudoku instance. It can request to offload subtasks.
- **Delegator**: A special Active Validator that identifies Idle Validators and delegates one or more Sudoku branches for concurrent exploration.

This method is best suited for puzzles where the solution lies deeper in the search tree, and the number of threads is relatively small.

## 2. Sudoku Solver with Pthread - Hybrid DFS/BFS

The hybrid approach builds upon the classic DFS model by allowing **multi-delegation**. This enhances the DFS strategy and introduces **breadth-first search (BFS)** behavior by enabling delegator threads to assign multiple Sudoku puzzles at once. 

### Key Features
- **Hybrid DFS/BFS**: Allows deeper levels to be expanded in breadth when idle threads are available.
- **Tunable Parallelism**: The number of delegations can be configured, making the approach adaptable to puzzle complexity.
- **Improved Scalability**: This variant provides better throughput by parallelizing more tasks, making it more suitable for puzzles of varying depths.

This approach introduces a slight increase in synchronization complexity due to multi-delegation, but it allows for more efficient use of resources when handling puzzles with mixed difficulty levels.

## 3. Sudoku Solver with OpenMP - BFS

This solution implements a **Breadth-First Search (BFS)** approach using OpenMP for parallelism. The algorithm expands each level of the Sudoku solution tree in parallel, with each thread working on different configurations at each level.

### Key Features
- **BFS Approach**: BFS is effective when the solution is near the top of the search tree, making it ideal for simpler puzzles.
- **Parallel Expansion**: The BFS method generates configurations level by level, with each level expanded in parallel using OpenMP's `#pragma omp parallel for`.

### Key Variables
- **solved_sudoku**: Flag indicating whether a valid solution has been found.
- **list**: List of puzzles to explore.
- **index_list**: Index into `list`.
- **new_list**: Puzzles generated at the next level.
- **new_index_list**: Index into `new_list`.

While BFS is easy to parallelize, it suffers from **high memory usage** as the depth of the search tree increases. This approach is best suited for shallow puzzles where solutions can be found quickly.

## Comparative Analysis

The three approaches exhibit varying strengths and weaknesses depending on puzzle complexity, solution depth, and available parallelism. Below is a comparison of the methods:

| Feature / Method        | Pthread DFS      | Hybrid DFS/BFS      | OpenMP BFS         |
|-------------------------|------------------|---------------------|--------------------|
| **Traversal Type**       | DFS              | DFS with BFS option | BFS                |
| **Threading Model**      | Manual (Pthreads)| Manual (Pthreads)   | Shared (OpenMP)    |
| **Delegation**           | 1-at-a-time      | Configurable        | Full level parallel|
| **Load Balancing**       | Manual via FIFO  | Adaptive            | Implicit via loop  |
| **Memory Usage**         | Low              | Moderate            | High (deep levels) |
| **Best For**             | Hard puzzles     | Mixed difficulty    | Easy puzzles       |
| **Complexity**           | Medium           | High (more sync)    | Low (easy setup)   |

### Key Takeaways:
- **Pthread DFS** is ideal for deeply nested solutions where memory usage is a concern, and the recursive nature of DFS is beneficial.
- **Hybrid DFS/BFS** provides the best **customizability** and **parallel efficiency**, especially when dealing with puzzles of varying difficulty.
- **OpenMP BFS** is efficient for simpler puzzles where the solution is near the root, but becomes less efficient in terms of memory usage for more complex puzzles.

## Conclusion

This project demonstrates how the choice of parallelization strategy can significantly impact the performance of a Sudoku-solving algorithm. The **pure DFS** approach provides a solid foundation for deeply nested solutions, while the **hybrid DFS/BFS** model strikes a good balance between memory usage and parallel efficiency, making it suitable for a wide range of puzzles. The **OpenMP BFS** approach excels in simplicity and speed for shallow puzzles but suffers from memory overhead as the problem complexity increases.

The **hybrid Pthread** solution stands out as the most versatile and scalable approach, offering the best trade-off between memory usage, parallel efficiency, and adaptability to different puzzle complexities.
