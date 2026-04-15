#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <omp.h>

int  n; // number of cities
int *dist; // n x n distance matrix

int  best_cost;
int *best_path;
omp_lock_t best_lock;

void helper(int *path, int *visited, int depth, int current_cost)
{
    // Base case
    if (depth == n) {
        omp_set_lock(&best_lock);
        if (current_cost < best_cost) {
            best_cost = current_cost;
            memcpy(best_path, path, n * sizeof(int));
        }
        omp_unset_lock(&best_lock);
        return;
    }

    // current node
    int last = path[depth - 1];

    // Try every unvisited city
    for (int c = 0; c < n; c++) {
        if (visited[c]) continue;

        int new_cost = current_cost + dist[last * n + c];

        // Prune if the new edge is already more expensive than the best cost
        // best_cost is not protected by lock here, but it's fine because we only need a lower bound for pruning
        if (new_cost >= best_cost) continue;

        // pick c
        path[depth]   = c;
        visited[c]    = 1;

        helper(path, visited, depth + 1, new_cost);

        // backtrack
        visited[c]    = 0; // depth is not changed
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_cities> <num_threads> <filename.txt>\n",
                argv[0]);
        return 1;
    }

    n = atoi(argv[1]); // number of cities
    int num_threads = atoi(argv[2]);
    const char *filename = argv[3];

    if (n <= 0 || num_threads <= 0) {
        fprintf(stderr, "Error: num_cities and num_threads must be positive.\n");
        return 1;
    }

    // read distance matrix from file
    dist = (int *)malloc(n * n * sizeof(int));
    if (!dist) {
        perror("malloc");
        return 1;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    // get the distance
    for (int i = 0; i < n * n; i++) {
        if (fscanf(fp, "%d", &dist[i]) != 1) {
            fprintf(stderr, "Error reading distance matrix.\n");
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);

    best_cost = INT_MAX;
    best_path = (int *)malloc(n * sizeof(int));
    if (!best_path) {
        perror("malloc");
        return 1;
    }

    omp_init_lock(&best_lock);
    omp_set_num_threads(num_threads);

    // parallel from the second city
    #pragma omp parallel for schedule(dynamic, 1) default(none) shared(n, dist, best_cost, best_path, best_lock)
    for (int second = 1; second < n; second++) {
        /* Per-thread private work arrays */
        int *path = (int *)malloc(n * sizeof(int));
        int *visited = (int *)calloc(n, sizeof(int));
        if (!path || !visited) {
            free(path); free(visited);
            perror("malloc");
            return 1;
        }

        path[0] = 0; // start from 0
        path[1] = second;
        visited[0] = 1; // mark the city 0 as visited
        visited[second] = 1; // mark the second city as visited

        int edge_cost = dist[0 * n + second]; // newly added edge cost

        // if newly added cost is already larger than the best cost, no need to explore this path
        if (edge_cost < best_cost) {
            helper(path, visited, 2, edge_cost);
        }

        free(path);
        free(visited);
    }

    printf("Best path: ");
    for (int i = 0; i < n; i++) {
        printf("%d", best_path[i]);
        if (i < n - 1) printf(" ");
    }
    printf("\nDistance: %d\n", best_cost);

    omp_destroy_lock(&best_lock);
    free(dist);
    free(best_path);

    return 0;
}
