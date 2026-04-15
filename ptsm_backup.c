/*
 * ptsm.c - Parallel Traveling Salesman Problem (modified, no return to start)
 *
 * Algorithm: Branch and Bound with OpenMP parallelism
 *
 * Usage: ./ptsm <num_cities> <num_threads> <filename.txt>
 *
 * The path starts at city 0, visits every city exactly once, and does NOT
 * return to city 0. We want the minimum total distance.
 *
 * Parallelization strategy:
 *   The top-level loop over the first city after 0 (n-1 choices) is distributed
 *   across threads with dynamic scheduling. Each thread runs its own recursive
 *   branch-and-bound. A shared best_cost (protected by a critical section) is
 *   read before expanding a node so that all threads benefit from pruning
 *   discoveries made by other threads.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <omp.h>

/* -------------------------------------------------------------------------
 * Globals (read-only after initialization, except best_cost / best_path)
 * ---------------------------------------------------------------------- */

int  n;                  /* number of cities */
int *dist;               /* n x n distance matrix, row-major: dist[i*n + j] */

/* Shared best solution -- updated by multiple threads */
int  best_cost;          /* protected by best_lock */
int *best_path;          /* length n, protected by best_lock */
omp_lock_t best_lock;

/* -------------------------------------------------------------------------
 * Branch-and-bound recursive function (called per thread)
 *
 * path        : current path being built (length n, first `depth` entries filled)
 * visited     : boolean array of size n
 * depth       : how many cities are already in `path`
 * current_cost: total distance accumulated so far
 * ---------------------------------------------------------------------- */
void bnb(int *path, int *visited, int depth, int current_cost)
{
    /* Base case: all cities visited */
    if (depth == n) {
        /* Try to update the global best */
        omp_set_lock(&best_lock);
        if (current_cost < best_cost) {
            best_cost = current_cost;
            memcpy(best_path, path, n * sizeof(int));
        }
        omp_unset_lock(&best_lock);
        return;
    }

    int last = path[depth - 1];

    /* Try every unvisited city as the next step */
    for (int c = 0; c < n; c++) {
        if (visited[c]) continue;

        int new_cost = current_cost + dist[last * n + c];

        /* Pruning: read best_cost once (no lock needed for a read; worst case
         * we just fail to prune, which is correct but slightly suboptimal) */
        if (new_cost >= best_cost) continue;

        /* Extend the path */
        path[depth]   = c;
        visited[c]    = 1;

        bnb(path, visited, depth + 1, new_cost);

        /* Backtrack */
        visited[c]    = 0;
    }
}

/* -------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_cities> <num_threads> <filename.txt>\n",
                argv[0]);
        return 1;
    }

    n             = atoi(argv[1]);
    int num_threads = atoi(argv[2]);
    const char *filename = argv[3];

    if (n <= 0 || num_threads <= 0) {
        fprintf(stderr, "Error: num_cities and num_threads must be positive.\n");
        return 1;
    }

    /* ---- Read the distance matrix ---- */
    dist = (int *)malloc(n * n * sizeof(int));
    if (!dist) { perror("malloc"); return 1; }

    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("fopen"); return 1; }

    for (int i = 0; i < n * n; i++) {
        if (fscanf(fp, "%d", &dist[i]) != 1) {
            fprintf(stderr, "Error reading distance matrix.\n");
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);

    /* ---- Initialize shared best solution ---- */
    best_cost = INT_MAX;
    best_path = (int *)malloc(n * sizeof(int));
    if (!best_path) { perror("malloc"); return 1; }

    omp_init_lock(&best_lock);
    omp_set_num_threads(num_threads);

    /* ---- Parallel branch and bound ----
     *
     * We fix the first city to 0, then distribute the n-1 choices for the
     * second city across threads. Each thread gets its own private path and
     * visited arrays so there is no false sharing or data races in the
     * recursive call itself.
     */
    #pragma omp parallel for schedule(dynamic, 1) default(none) \
        shared(n, dist, best_cost, best_path, best_lock)
    for (int second = 1; second < n; second++) {
        /* Per-thread private work arrays */
        int *path    = (int *)malloc(n * sizeof(int));
        int *visited = (int *)calloc(n, sizeof(int));
        if (!path || !visited) {
            free(path); free(visited);
            continue; /* skip this branch on allocation failure */
        }

        path[0] = 0;
        path[1] = second;
        visited[0] = 1;
        visited[second] = 1;

        int edge_cost = dist[0 * n + second];

        /* Only enter if this first edge is already below the global best */
        if (edge_cost < best_cost) {
            bnb(path, visited, 2, edge_cost);
        }

        free(path);
        free(visited);
    }

    /* ---- Print result ---- */
    printf("Best path: ");
    for (int i = 0; i < n; i++) {
        printf("%d", best_path[i]);
        if (i < n - 1) printf(" ");
    }
    printf("\nDistance: %d\n", best_cost);

    /* ---- Cleanup ---- */
    omp_destroy_lock(&best_lock);
    free(dist);
    free(best_path);

    return 0;
}
