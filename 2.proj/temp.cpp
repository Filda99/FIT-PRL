
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define FILENAME argv[1]

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.txt>\n", argv[0]);
        exit(1);
    }

    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int rows, cols;

    if (rank == 0) {
        printf("Rank: %d\n", rank);
        FILE *file = fopen(FILENAME, "r");
        if (file == NULL) {
            fprintf(stderr, "Error opening file\n");
            exit(1);
        }

        // Determine the number of rows and columns in the file
        fscanf(file, "%d %d", &rows, &cols);
        printf("Rows: %d, Cols: %d\n", rows, cols);

        // Dynamically allocate memory for the matrix
        int **matrix = (int **)malloc(rows * sizeof(int *));
        for (int i = 0; i < rows; i++) {
            matrix[i] = (int *)malloc(cols * sizeof(int));
            for (int j = 0; j < cols; j++) {
                fscanf(file, "%d", &matrix[i][j]);
            }
        }
        fclose(file);

        // Distribute rows to processes using MPI_Send
        int rows_per_process = rows / size;
        for (int i = 1; i < size; i++) {
            int start_row = i * rows_per_process;
            int end_row = (i == size - 1) ? rows : (i + 1) * rows_per_process;
            int num_rows = end_row - start_row;
            printf("Sending %d rows to process %d\n", num_rows, i);
            MPI_Send(&num_rows, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            for (int r = start_row; r < end_row; r++) {
                MPI_Send(matrix[r], cols, MPI_INT, i, 0, MPI_COMM_WORLD);
            }
        }

        // Process 0's portion
        rows = rows_per_process;
        for (int i = 0; i < rows; i++) {
            printf("%d: ", rank);
            for (int j = 0; j < cols; j++) {
                printf("%d ", matrix[i][j]);
            }
            printf("\n");
        }

        // Free dynamically allocated memory
        for (int i = 0; i < rows; i++) {
            free(matrix[i]);
        }
        free(matrix);
    } else {
        // Receive number of rows
        MPI_Recv(&rows, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Dynamically allocate memory for local portion
        int **local_rows = (int **)malloc(rows * sizeof(int *));
        for (int i = 0; i < rows; i++) {
            local_rows[i] = (int *)malloc(cols * sizeof(int));
        }

        // Receive assigned rows using MPI_Recv
        for (int i = 0; i < rows; i++) {
            MPI_Recv(local_rows[i], cols, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // Print current state
        for (int i = 0; i < rows; i++) {
            printf("%d: ", rank);
            for (int j = 0; j < cols; j++) {
                printf("%d ", local_rows[i][j]);
            }
            printf("\n");
        }

        // Free dynamically allocated memory
        for (int i = 0; i < rows; i++) {
            free(local_rows[i]);
        }
        free(local_rows);
    }

    MPI_Finalize();
    return 0;
}