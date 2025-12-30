#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <set>

#define SPARSENESS 5

// A struct to hold a single operation (type and key)
typedef struct {
    char type;
    int key;
} operation;

/**
 * @brief Shuffles an array of integers using the Fisher-Yates algorithm.
 */
void shuffle_int(int *array, size_t n) {
    if (n > 1) {
        for (size_t i = 0; i < n - 1; i++) {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            int temp = array[j];
            array[j] = array[i];
            array[i] = temp;
        }
    }
}

void shuffle_op(operation *array, size_t n) {
    if (n > 1) {
        for (size_t i = 0; i < n - 1; i++) {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            operation temp = array[j];
            array[j] = array[i];
            array[i] = temp;
        }
    }
}

int main(int argc, char** argv) {
    srand(time(NULL));
    int opt;
    int nqueries = 1000000;
    int ins_proportion = 40;
    int del_proportion = 20;
    extern char* optarg;

    const char* usage = "Usage: %s -n {queries (>10)} -i {insert %%} -d {delete %%}\n"
                        "Search proportion is calculated as 100 - insert%% - delete%%\n";

    // --- Argument Parsing ---
    while ((opt = getopt(argc, argv, "n:i:d:")) != -1) {
        switch (opt) {
            case 'n': nqueries = atoi(optarg); break;
            case 'i': ins_proportion = atoi(optarg); break;
            case 'd': del_proportion = atoi(optarg); break;
            default:
                fprintf(stderr, usage, argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // --- Input Validation ---
    if (nqueries < 10) {
        fprintf(stderr, "Error: Number of queries (-n) must be at least 10.\n");
        fprintf(stderr, usage, argv[0]);
        exit(EXIT_FAILURE);
    }

    if (ins_proportion + del_proportion > 100) {
        fprintf(stderr, "Error: insert%% + delete%% must be <= 100\n");
        exit(EXIT_FAILURE);
    }

    FILE *outfile = fopen("workload.txt", "w");
    if (outfile == NULL) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }

    // --- Calculate Operation Counts ---
    int search_proportion = 100 - ins_proportion - del_proportion;
    int num_inserts = (int)((nqueries * ins_proportion) / 100.0);
    int num_deletes = (int)((nqueries * del_proportion) / 100.0);
    int num_searches = nqueries - num_inserts - num_deletes;

    int preload_size = num_deletes + num_searches;
    if (preload_size == 0) {
         preload_size = nqueries / 2;
    }

    // --- 중복 방지를 위한 고유 키 생성 ---
    int* preloaded_keys = (int*)malloc(preload_size * sizeof(int));
    if (preloaded_keys == NULL) {
        perror("Failed to allocate memory for preloaded_keys");
        fclose(outfile);
        exit(EXIT_FAILURE);
    }
    
    // C++ set을 사용하여 중복 방지 (또는 다른 방법 사용)
    std::set<int> used_keys;
    int key_range = nqueries * nqueries;
    
    // 고유한 짝수 키들 생성
    for (int i = 0; i < preload_size; i++) {
        int new_key;
        do {
            new_key = (rand() % key_range) * 2;  // 짝수만 생성
        } while (used_keys.find(new_key) != used_keys.end());  // 중복 체크
        
        used_keys.insert(new_key);
        preloaded_keys[i] = new_key;
    }

    // Write the initial load operations to the file
    for (int i = 0; i < preload_size; i++) {
        fprintf(outfile, "i %d\n", preloaded_keys[i]);
    }

    // Shuffle keys to randomize which ones are for deletion vs. searching
    shuffle_int(preloaded_keys, preload_size);

    // --- Workload Definition ---
    operation* workload = (operation*)malloc(nqueries * sizeof(operation));
    if (workload == NULL) {
        perror("Failed to allocate memory for workload");
        free(preloaded_keys);
        fclose(outfile);
        exit(EXIT_FAILURE);
    }

    int op_idx = 0;

    // Create delete operations from the first part of the shuffled array
    for (int i = 0; i < num_deletes; i++) {
        workload[op_idx++] = (operation){'d', preloaded_keys[i]};
    }

    // Create search operations from the second, non-overlapping part
    for (int i = 0; i < num_searches; i++) {
        workload[op_idx++] = (operation){'q', preloaded_keys[num_deletes + i]};
    }

    // Create insert operations using new, non-conflicting odd-numbered keys
    std::set<int> odd_keys;
    for (int i = 0; i < num_inserts; i++) {
        int new_key;
        do {
            new_key = (rand() % (nqueries * SPARSENESS)) * 2 + 1;  // 홀수
        } while (odd_keys.find(new_key) != odd_keys.end());
        
        odd_keys.insert(new_key);
        workload[op_idx++] = (operation){'i', new_key};
    }

    // Shuffle the entire list of operations to create a random workload
    shuffle_op(workload, nqueries);

    // --- Workload Execution: Write the final workload to the file ---
    for (int i = 0; i < nqueries; i++) {
        fprintf(outfile, "%c %d\n", workload[i].type, workload[i].key);
    }

    // --- Cleanup ---
    free(preloaded_keys);
    free(workload);
    fclose(outfile);

    printf("Workload successfully saved to workload.txt\n");
    return 0;
}

