#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//#define MATRIX_SIZE 1000  // Size of the square matrix (MATRIX_SIZE x MATRIX_SIZE)

// Generate a random double between 0 and 1
double rand_double() {
    return (double)rand() / RAND_MAX;
}

// Fill a matrix with random values
void generate_random_matrix(double *matrix, int rows, int cols) {
    for (int i = 0; i < rows * cols; i++) {
        matrix[i] = rand_double();
    }
}

// Outer-product matrix multiplication (race-free)
void matmul_block(const double *A_block, const double *B, double *C_block,
                  int rows_per_proc, int dim) {
    // Step 1: Zero-initialize result matrix C_block
    #pragma omp parallel for
    for (int idx = 0; idx < rows_per_proc * dim; idx++) {
        C_block[idx] = 0.0;
    }

    // Step 2: Outer-product accumulation (race-free)
    for (int k = 0; k < dim; k++) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < rows_per_proc; i++) {
            double a_ik = A_block[i * dim + k];
            for (int j = 0; j < dim; j++) {
                C_block[i * dim + j] += a_ik * B[k * dim + j];
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int rank, size;
    double *A = NULL, *B = NULL, *C = NULL;
    double *A_block, *C_block;
    double start_time, end_time, local_elapsed, max_elapsed;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (MATRIX_SIZE % size != 0) {
        if (rank == 0) {
            fprintf(stderr, "Error: MATRIX_SIZE (%d) must be divisible by number of processes (%d)\n",
                    MATRIX_SIZE, size);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    int rows_per_proc = MATRIX_SIZE / size;

    // Allocate and initialize matrices
    if (rank == 0) {
        A = (double *)malloc(MATRIX_SIZE * MATRIX_SIZE * sizeof(double));
        B = (double *)malloc(MATRIX_SIZE * MATRIX_SIZE * sizeof(double));
        C = (double *)malloc(MATRIX_SIZE * MATRIX_SIZE * sizeof(double));

        srand((unsigned)time(NULL));
        generate_random_matrix(A, MATRIX_SIZE, MATRIX_SIZE);
        generate_random_matrix(B, MATRIX_SIZE, MATRIX_SIZE);
    } else {
        B = (double *)malloc(MATRIX_SIZE * MATRIX_SIZE * sizeof(double));
    }

    A_block = (double *)malloc(rows_per_proc * MATRIX_SIZE * sizeof(double));
    C_block = (double *)malloc(rows_per_proc * MATRIX_SIZE * sizeof(double));

    // Scatter rows of A and broadcast B
    MPI_Bcast(B, MATRIX_SIZE * MATRIX_SIZE, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Scatter(A, rows_per_proc * MATRIX_SIZE, MPI_DOUBLE,
                A_block, rows_per_proc * MATRIX_SIZE, MPI_DOUBLE,
                0, MPI_COMM_WORLD);

    // Time the multiplication
    start_time = MPI_Wtime();
    matmul_block(A_block, B, C_block, rows_per_proc, MATRIX_SIZE);
    end_time = MPI_Wtime();

    local_elapsed = end_time - start_time;

    // Gather C_block into full C
    MPI_Gather(C_block, rows_per_proc * MATRIX_SIZE, MPI_DOUBLE,
               C, rows_per_proc * MATRIX_SIZE, MPI_DOUBLE,
               0, MPI_COMM_WORLD);

    // Reduce timing
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE,
               MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Parallel Hybrid %dx%d matrix multiplication took %.6f seconds (max across ranks)\n", MATRIX_SIZE, MATRIX_SIZE, max_elapsed);
    }

    // Cleanup
    free(A_block);
    free(C_block);
    free(B);
    if (rank == 0) {
        free(A);
        free(C);
    }

    MPI_Finalize();
    return 0;
}
