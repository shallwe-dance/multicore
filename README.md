# Multicore Computing

Course projects for **Multicore Computing, Fall 2025**, at **Sungkyunkwan University (SKKU)**.

This repository explores shared-memory concurrency, CPU parallelism, distributed transaction coordination, and GPU computing through four assignments written in C, C++, and CUDA.

## Projects at a Glance

| Directory | Assignment | Technologies | Main Topics |
| --- | --- | --- | --- |
| [proj1](proj1/) | Concurrent skip list | C++, POSIX threads | Work distribution, mutual exclusion, synchronization overhead |
| [proj2](proj2/) | Parallel closest-pair search and DFT | C++, OpenMP | Loop parallelism, reductions, independent output computation |
| [proj3](proj3/) | Two-phase commit with crash recovery | C, ONC RPC, XDR | Distributed coordination, persistent logging, failure recovery |
| [proj4](proj4/) | Conway's Game of Life on the GPU | CUDA C++, Python | Grid decomposition, shared memory, halo cells, double buffering |

## Project 1 — Concurrent Skip List

This assignment implements concurrent access to a shared skip list, an ordered data structure that uses multiple levels of linked nodes to support insertion, lookup, and deletion.

The implementation combines a multithreaded workload driver with a skip list protected by a single mutex:

- **Work distribution:** Four POSIX worker threads process commands assigned by `key % NT`. Commands for the same key stay with one worker and retain their input order.
- **Synchronization:** A coarse-grained mutex protects each skip-list operation. A scope-based lock wrapper acquires and releases the mutex around access to the shared structure.
- **Randomized structure:** Node heights are chosen probabilistically using a thread-local random number generator.
- **Workload measurement:** A generator creates mixed insertion, lookup, and deletion workloads, while the driver reports elapsed time and throughput.

Because all operations use the same mutex, access to the data structure is serialized even though the driver has multiple workers. This makes the assignment a study of both concurrent program structure and the contention introduced by coarse-grained locking.

**Learning focus:** Thread creation and coordination, per-key work partitioning, shared data structures, and synchronization costs.

**Main files:** [driver.cpp](proj1/driver.cpp), [skiplist.h](proj1/skiplist.h), [inputgen.cpp](proj1/inputgen.cpp).

## Project 2 — Parallel Numerical Computation with OpenMP

This assignment applies OpenMP to two computationally intensive problems, illustrating how the dependency structure of an algorithm determines its parallelization strategy.

### Closest Pair of Points

[problem1.cpp](proj2/problem1.cpp) finds the minimum Euclidean distance among a set of two-dimensional points.

- Examines every distinct pair of points with an **O(N²)** search.
- Parallelizes the outer loop with static scheduling.
- Uses an OpenMP **minimum reduction** to combine the results computed by different threads.
- Compares squared distances during the search and takes a square root for the final result.

### Discrete Fourier Transform

[problem2.cpp](proj2/problem2.cpp) computes the direct **O(N²) discrete Fourier transform (DFT)** of a real-valued input sequence.

- Distributes output frequency indices across threads with static scheduling.
- Computes each frequency component independently using a local complex accumulator.
- Keeps the summation over input samples sequential within each frequency component.
- Writes each result to a separate output element.

Both programs generate input data from a configurable random seed and record elapsed time.

**Learning focus:** Identifying independent work, loop-level parallelism, reductions, thread-local state, and performance measurement.

## Project 3 — Two-Phase Commit and Coordinator Recovery

This assignment models distributed transaction coordination using **two-phase commit (2PC)**. A coordinator communicates with multiple participant processes through ONC RPC.

The protocol has two phases:

1. **Prepare:** The coordinator asks each participant whether it can commit. Participants record their prepared state before returning a successful vote.
2. **Decision:** The coordinator records a commit decision when all participants vote YES, or an abort decision when a participant rejects or fails to respond, then sends that decision to the participants.

The implementation focuses on protocol state, persistent decision logs, and recovery after a coordinator crash.

- **RPC interface:** `commit.x` defines the `PREPARE`, `COMMIT`, and `ABORT` procedures and their XDR data types.
- **Participant identification:** Distinct RPC program numbers allow multiple participants to coexist on the same host.
- **Persistent logging:** Coordinator and participant log entries are synchronized to disk with `fsync`.
- **Coordinator recovery:** On restart, the coordinator inspects its most recent transaction. It resends a recorded decision, or decides to abort if the transaction has no recorded decision.
- **Failure injection:** Flags simulate crashes at selected points in the prepare and commit paths.

The included scripts cover these scenarios:

| Scenario | Behavior Explored |
| --- | --- |
| All participants vote YES | Normal commit |
| A participant crashes during prepare | Abort after a missing response |
| The coordinator crashes after prepare, before logging a decision | Abort during coordinator recovery |
| The coordinator crashes after logging COMMIT, before notifying participants | Resend COMMIT during coordinator recovery |

**Learning focus:** Distributed agreement on transaction outcomes, the ordering of log writes and messages, failure injection, and recovery from persistent state.

**Main files:** [coordinator.c](proj3/coordinator.c), [participant.c](proj3/participant.c), [commit.x](proj3/commit.x), [test scenarios](proj3/test/).

The [project notes](proj3/README.md) contain recorded logs from the original experiments.

## Project 4 — Conway's Game of Life with CUDA

This assignment implements Conway's Game of Life on a finite, two-dimensional grid using a CUDA GPU. Each generation updates a cell according to its eight neighbors: a live cell survives with two or three live neighbors, and a dead cell becomes alive with exactly three.

The implementation uses the following GPU programming techniques:

- **Two-dimensional decomposition:** A grid of 16 × 16 thread blocks assigns one thread to each board cell.
- **Shared-memory tiling:** Each block stages its cells in shared memory so neighboring threads can reuse loaded data.
- **Halo cells:** A one-cell border around each tile supplies neighboring values from adjacent tiles, including corners.
- **Block synchronization:** Threads synchronize after loading the tile and its halo, before evaluating the next generation.
- **Double buffering:** Separate device buffers hold the current and next generations, with their pointers swapped after each update.
- **Boundary handling:** Cells outside the board are treated as dead.

Board cells are stored as single-byte values. Initial patterns and final live cells are represented as coordinate pairs, and the Python input generator creates combinations of gliders, lightweight spaceships, and blinkers.

**Learning focus:** Mapping a neighborhood-based computation to the GPU, data reuse through shared memory, synchronization within thread blocks, and generation-by-generation state updates.

**Main files:** [project4.cu](proj4/project4.cu), [inputgen.py](proj4/inputgen.py), [sample pattern 1](proj4/life.data.1), [sample pattern 2](proj4/life.data.2).
