// life.c
// Build: gcc -O2 life.c -o life
// Run:   ./life input.txt 100 50
//        => runs 100 generations on a 50x50 board

#include <stdio.h>
#include <stdlib.h>
#include <cuda.h>

#define GHOST 1

__host__ __device__ __forceinline__ int idx(int x, int y, int N) {
    return y * N + x;
}

__device__ __forceinline__
unsigned char load_or_zero_dev(const unsigned char *__restrict__ cur,
                               int gx, int gy, int N) {
    if ((unsigned)gx >= (unsigned)N || (unsigned)gy >= (unsigned)N) return 0;
    return cur[idx(gx, gy, N)];
}

__global__ void step_gpu_ghost(const unsigned char *__restrict__ cur, unsigned char *__restrict__ next, int N) {
    extern __shared__ unsigned char temp[];
    
    int gx = blockIdx.x * blockDim.x + threadIdx.x;
    int gy = blockIdx.y * blockDim.y + threadIdx.y; 

    int tx = threadIdx.x;
    int ty = threadIdx.y;

    const int sW = blockDim.x + 2 * (GHOST);
    //const int sH = blockDim.y + 2 * (GHOST);

    int sx = tx + (GHOST);
    int sy = ty + (GHOST);

    temp[sy * sW + sx] = load_or_zero_dev(cur, gx, gy, N);

    // 1) 중앙(내 셀) 로드
    temp[sy * sW + sx] = load_or_zero_dev(cur, gx, gy, N);

    // 2) 좌/우 ghost column
    if (tx < GHOST) { // 왼쪽 테두리 로딩 담당
        int ngx = gx - GHOST;
        temp[sy * sW + (sx - GHOST)] = load_or_zero_dev(cur, ngx, gy, N);
    }
    if (tx >= blockDim.x - GHOST) { // 오른쪽 테두리 로딩 담당
        int ngx = gx + GHOST;
        temp[sy * sW + (sx + GHOST)] = load_or_zero_dev(cur, ngx, gy, N);
    }

    // 3) 상/하 ghost row
    if (ty < GHOST) { // 위쪽 테두리
        int ngy = gy - GHOST;
        temp[(sy - GHOST) * sW + sx] = load_or_zero_dev(cur, gx, ngy, N);
    }
    if (ty >= blockDim.y - GHOST) { // 아래쪽 테두리
        int ngy = gy + GHOST;
        temp[(sy + GHOST) * sW + sx] = load_or_zero_dev(cur, gx, ngy, N);
    }

    // 4) 코너 4개 (좌상/우상/좌하/우하)
    if (tx < GHOST && ty < GHOST) { // 좌상단
        temp[(sy - GHOST) * sW + (sx - GHOST)] = load_or_zero_dev(cur, gx - GHOST, gy - GHOST, N);
    }
    if (tx >= blockDim.x - GHOST && ty < GHOST) { // 우상단
        temp[(sy - GHOST) * sW + (sx + GHOST)] = load_or_zero_dev(cur, gx + GHOST, gy - GHOST, N);
    }
    if (tx < GHOST && ty >= blockDim.y - GHOST) { // 좌하단
        temp[(sy + GHOST) * sW + (sx - GHOST)] = load_or_zero_dev(cur, gx - GHOST, gy + GHOST, N);
    }
    if (tx >= blockDim.x - GHOST && ty >= blockDim.y - GHOST) { // 우하단
        temp[(sy + GHOST) * sW + (sx + GHOST)] = load_or_zero_dev(cur, gx + GHOST, gy + GHOST, N);
    }

    // 5) 모든 스레드가 temp 채울 때까지 대기
    __syncthreads();

    // 보드 밖이면 계산 안 함
    if (gx >= N || gy >= N) return;

    // 6) 고스트 셀 포함 shared temp에서 이웃 8칸 카운트
    int alive = 0;
    alive += temp[(sy - 1) * sW + (sx - 1)];
    alive += temp[(sy - 1) * sW + (sx    )];
    alive += temp[(sy - 1) * sW + (sx + 1)];
    alive += temp[(sy    ) * sW + (sx - 1)];
    alive += temp[(sy    ) * sW + (sx + 1)];
    alive += temp[(sy + 1) * sW + (sx - 1)];
    alive += temp[(sy + 1) * sW + (sx    )];
    alive += temp[(sy + 1) * sW + (sx + 1)];

    int self = temp[sy * sW + sx];

    unsigned char out;
    if (self)
        out = (alive == 2 || alive == 3);
    else
        out = (alive == 3);

    next[idx(gx, gy, N)] = out;
}




int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input_file> <#generations> <board_size>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    int generations = atoi(argv[2]);
    int N = atoi(argv[3]);

    size_t total = (size_t)N * N;

    /*
    variables to store current board and next board
    */
    unsigned char *cur, *next;
    unsigned char *d_cur, *d_next;

    //unsigned char *cur = calloc(total, 1);
    //unsigned char *next = calloc(total, 1);
    cur = (unsigned char*)calloc(total, 1);
    next = (unsigned char*)calloc(total, 1);
    
    if (!cur || !next) {
        perror("malloc");
        return 1;
    }

    // Load initial pattern
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen");
        return 1;
    }

    int x, y;
    while (fscanf(f, "%d %d", &x, &y) == 2) {
        if (x >= 0 && x < N && y >= 0 && y < N) {
            cur[idx(x, y, N)] = 1;
        }
    }
    fclose(f);
    
    // Alloc space for device copies
    cudaMalloc((void**)&d_cur, total);
    cudaMalloc((void**)&d_next, total);

    // Copy to device
    cudaMemcpy(d_cur, cur, total, cudaMemcpyHostToDevice);
    //cudaMemcpy(d_next, next, total, cudaMemcpyHostToDevice);

/*
f<<<n1, n2>>>
n1 = the number of blocks
n2 = the number of threads per a block

dim3 g(n1, n2, n3)
dim3 b(m1, m2, m3)
f<<<g, b>>>
n1, n2, n3개의 블록, 각 블록 안에 m1, m2, m3개의 쓰레드
*/
    dim3 b(16, 16);
    //dim3 g(ceil(N/blockDim.x), ceil(N/blockDim.y));
    dim3 g((N + b.x - 1) / b.x, (N + b.y - 1) / b.y);
    size_t shared_memory = (b.x + 2 * (GHOST)) * (b.y + 2 * (GHOST)) * sizeof(unsigned char);

    // Run simulation
    for (int t = 0; t < generations; t++) {
        step_gpu_ghost<<<g, b, shared_memory>>>(d_cur, d_next, N);
        //cudaDeviceSynchronize();
        unsigned char *tmp = d_cur; 
        d_cur = d_next;
        d_next = tmp;
    }

    // Copy result back to host
    cudaMemcpy(cur, d_cur, total, cudaMemcpyDeviceToHost);
    //cudaMemcpy(next, d_next, total, cudaMemcpyDeviceToHost);

    // Print final board
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
//            putchar(cur[idx(x, y, N)] ? '#' : '.');
              if(cur[idx(x, y, N)]) printf("%d %d\n", x, y);
        }
//        putchar('\n');
    }

    
    free(cur);
    free(next);
    cudaFree(d_cur);
    cudaFree(d_next);
    return 0;
}

