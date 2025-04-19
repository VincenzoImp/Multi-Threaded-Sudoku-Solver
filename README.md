# Abstract

This document presents a multithreaded approach to solving 9x9 Sudoku puzzles using Pthread and OpenMP libraries. Three solutions are discussed: two based on Pthread and one based on OpenMP. The first Pthread-based solution implements a fully recursive depth-first search (DFS) approach in parallel. The second, also with Pthread, is a hybrid variation that retains the DFS structure but introduces partial breadth-first search (BFS) behavior by allowing multiple task delegations. The third solution, implemented with OpenMP, follows a BFS strategy and parallelizes where possible.

This document describes the architecture of all three solutions, the encountered issues and limitations, and reports on performance tests conducted using a shared dataset of 9x9 Sudoku puzzles. All puzzles in the dataset have a unique solution. The multithreaded solutions are compared with each other and with a sequential baseline.

---

## 1. Sudoku Solver with Pthread - Classic DFS

This solution is based on a recursive depth-first search (DFS) traversal. DFS explores the entire solution tree for the given Sudoku puzzle and is advantageous when the solution lies deep in the tree (as opposed to BFS, which performs better for shallow solutions).

The algorithm performs a DFS on the possible Sudoku configurations, using a user-defined number of threads. The goal is to find the unique valid solution by distributing the computational workload among threads in parallel.

At initialization, the algorithm receives the number of threads and the Sudoku puzzle to solve. During input parsing, the empty cells are identified and stored in a list to avoid redundant grid reads.

> *During optimization, we experimented with shuffling the order of empty cells, but keeping a linear order proved more effective.*

### Key Variables

- **problem_solved**: Flag indicating whether a valid solution has been found.
- **sudokus_to_solve**: Array of Sudoku puzzles pending validation.
- **semaphores**: Array of semaphores, one per thread.
- **mutex_shared_data**: Mutex controlling access to the FIFO.
- **shared_fifo_idle_threads**: Shared FIFO queue of idle threads.
- **mutex_solved_sudoku**: Mutex protecting access to the solved Sudoku.
- **solved_sudoku**: Structure that holds the valid solution.

### Thread States

- **Idle Validator**: Thread waiting for work.
- **Active Validator**: Thread actively validating a Sudoku configuration.
- **Delegator**: An Active Validator that may delegate part of its work to an Idle Validator.

![Possible thread states](stati%20thread.png)

Execution begins with all threads in the Idle Validator state. One thread receives the initial puzzle and becomes an Active Validator. As threads generate new configurations (by inserting valid digits in empty cells), they check if any Idle Validators are available and, if so, delegate a new Sudoku puzzle for validation.

The process continues until one thread successfully completes a valid Sudoku. The result is saved, and the `problem_solved` flag is set to `true`, signaling all threads to stop.

---

## 2. Sudoku Solver with Pthread - Hybrid DFS/BFS Variant

This solution builds upon the classic DFS model, introducing the possibility for Delegator threads to delegate multiple Sudoku puzzles at once. This behavior emulates a breadth-first search (BFS) while retaining the structure and mechanisms of DFS.

The maximum number of concurrent delegations is configurable. Increasing this value enhances the BFS-like behavior, improving performance when solutions are found in the upper levels of the solution tree.

Thread architecture remains unchanged. The key difference lies in the Delegator behavior: if multiple puzzles are ready to be validated, the Delegator can distribute them to several Idle Validators in a single batch.

> *This strategy showed strong results in testing, enabling better scalability for simpler cases while maintaining solid performance for more complex ones.*

---

## 3. Sudoku Solver with OpenMP - BFS

This solution follows a pure BFS approach, leveraging OpenMP to parallelize computation. BFS is particularly effective when the puzzle's solution lies near the top of the search tree.

The algorithm maintains two lists:

- `list`: containing the puzzles to be expanded.
- `new_list`: which stores the puzzles generated in the next level of the tree.

Parallel operations occur primarily during the double loop that generates `new_list`, where each puzzle in `list` may generate multiple children.

### Key Variables

- **solved_sudoku**: Flag indicating whether a valid solution has been found.
- **list**: List of puzzles to explore.
- **index_list**: Index into `list`.
- **new_list**: Puzzles generated at the next level.
- **new_index_list**: Index into `new_list`.

The main advantage of this method is its ease of parallelization. However, if the solution lies deep in the tree, BFS suffers from high memory consumption due to the large number of partial configurations it must manage.

---

## 4. Comparison of Solutions

The three solutions behave differently depending on the complexity of the puzzle and the depth at which the solution is located:

- The **pure DFS with Pthreads** is effective for deep solutions due to its recursive nature.
- The **hybrid DFS/BFS variant** provides a good balance between memory usage and parallelization, adapting well to different cases.
- The **OpenMP BFS approach** performs best on simpler puzzles or those with shallow solutions, but becomes inefficient in terms of memory for more complex cases.

In tests performed on a mixed dataset of Sudoku puzzles with increasing difficulty, the hybrid solution demonstrated the most balanced performance overall, making the best use of hardware resources in heterogeneous scenarios.

---

## 5. Conclusion

This project demonstrated how the choice of parallelization strategy significantly affects the performance of a Sudoku-solving algorithm. The pure DFS version offers a solid foundation, while the hybrid extension delivers tangible benefits in versatility. The BFS approach, though easy to parallelize with OpenMP, suffers in memory usage when solving complex puzzles. Together, the three solutions provide a comprehensive overview of multithreading potential in search-based problem-solving.
