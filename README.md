# Binary Decision Diagram — C Implementation

A C implementation of a **Reduced Ordered Binary Decision Diagram (ROBDD)** for representing and evaluating Boolean functions expressed in Disjunctive Normal Form (DNF). Includes a variable ordering optimizer and a correctness test suite.

## How It Works

A BDD is a directed acyclic graph where each internal node represents a Boolean variable with two outgoing edges — a low (false) branch and a high (true) branch — terminating at two shared terminal nodes (0 and 1).

The implementation builds BDDs top-down using recursive Shannon expansion over the variable order. Reduction rules are applied during construction via a unique table (chained hash table): if a node's low and high branches are identical it is eliminated and the branch is returned directly; if an equivalent node already exists in the table it is reused rather than duplicated. This keeps the diagram minimal.

Variable ordering is optimized by systematically trying all pairwise variable swaps, building a BDD for each permutation, and keeping the one with the fewest nodes. In testing across 100 random Boolean functions this consistently reduced node count by ~95–98% compared to the theoretical maximum.

## Project Structure

```
BDD.c / BDD.h      # BDD implementation
Tester.c           # Correctness test suite (100 randomized tests)
bdd_test_results.txt  # Sample test output
```

## API

```c
BDD* create_BDD(char* Bfunction, char* vars);
// Build a BDD for a DNF expression with a given variable order

BDD* BDD_create_with_best_order(char* Bfunction);
// Try all pairwise variable order permutations and return the most compact BDD

int BDD_use(BDD* bdd, const char* assignment);
// Evaluate the BDD for a binary assignment string (e.g. "1010")

void free_bdd(BDD* bdd);
```

## Testing

`Tester.c` generates 100 random DNF expressions over 13–15 variables and validates both BDDs against direct term evaluation across all 2ⁿ input combinations. All 100 tests pass.

```
Number of tests passed: 100
Number of tests failed: 0
```

## Building & Running

```bash
gcc -O2 -o tester Tester.c BDD.c -lm
./tester
```

Results are written to `bdd_test_results.txt`.
