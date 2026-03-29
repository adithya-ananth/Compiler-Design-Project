# Chaitin's Register Allocation: Phases 1–5

This document explains the first five phases of the register allocation process implemented in `src/reg_alloc.c`. The allocator follows a **Chaitin-Briggs** style graph-coloring approach.

## Overview

The register allocator operates on a per-function basis. It repeats the cycle of Phases 1 to 5 until all variables are successfully assigned to physical registers or spilled to memory.

---

### Phase 1: Liveness Analysis
**Functionality**: Computes the set of variables that are "live" at every instruction.
- **Backward Pass**: The allocator walks through each basic block in reverse order.
- **Live Variables**: A variable is "live" if it has been defined but not yet used by all subsequent instructions that require it.
- **Internal Mechanism**: It starts with the `live_out` set of a block and updates it as it moves upward, adding "used" variables and removing "defined" variables.

### Phase 2: Interference Graph Construction
**Functionality**: Builds a graph where each node represents an allocatable variable or temporary.
- **Interference Edges**: An edge is added between two nodes (variables) if they are live at the same time. This signifies that they cannot reside in the same physical register.
- **Constraint Modeling**: This graph represents the constraints that the coloring algorithm must satisfy.

### Phase 3: Simplify (Stack-Based Reduction)
**Functionality**: Prunes the interference graph to determine the order of coloring.
- **Degree < K**: The algorithm repeatedly finds a node with fewer than `K` neighbors (where `K` is the number of available physical registers) and pushes it onto a stack.
- **Spill Candidates**: If all remaining nodes have a degree $\ge K$, the algorithm must pick a "spill candidate" based on heuristics (like the highest degree) to push onto the stack, potentially leading to a memory spill.

### Phase 4: Select (Color Assignment)
**Functionality**: Assigns physical registers to variables by popping them from the stack.
- **Greedy Coloring**: For each node popped, the allocator looks at its neighbors in the interference graph and assigns it the first available color (register) not used by any of its already-colored neighbors.
- **Actual Spills**: If no colors are available (all `K` registers are taken by neighbors), the node is marked as "spilled" and assigned a stack offset in the function's frame.

### Phase 5: Spill Rewrite
**Functionality**: Modifies the Intermediate Representation (IR) to handle spilled variables.
- **Memory Access**: For every variable marked as spilled:
    - **Before Uses**: A new temporary is created, and a `load` from the stack offset is inserted.
    - **After Definitions**: The result is stored back to the stack offset.
- **Iteration**: Since new temporaries are introduced, the allocator must return to **Phase 1** and re-run the entire process. These new temporaries have very short liveness periods, making them much easier to color in the next round.

---

*Note: Phase 6 (Graphviz Export) is used for debugging and visualization of the interference graph, but is not part of the core allocation logic.*
