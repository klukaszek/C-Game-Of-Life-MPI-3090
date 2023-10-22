/*
CIS3090 Assignment 2: Conway's Game of Life MPI
Author: Kyle Lukaszek
ID: 1113798
Due: October 20th, 2023
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include <time.h>
#include <string.h>

// Define macros
#define ALIVE(x) (x >= 1)
#define DEAD 0
#define DEFAULT_GRID_SIZE 24

// Function prototypes
void init_subgrid(int **cur_grid, int **next_grid, int rows, int columns, int rank, int size);
void update_ghost_rows(int **cur_grid, int rows, int columns, int prev_rank, int next_rank);
void print_full_grid(int **cur_grid, int rows, int columns, int rank, int size, int timestep);
void print_full_grid_with_ghosts(int **cur_grid, int rows, int columns, int rank, int size, int timestep);
void apply_rules(int **cur_grid, int **next_grid, int default_rows, int rows_per_process, int columns, int rank, int size);

// Main function
int main(int argc, char *argv[]) {
    int rank, size, arg;
    int rows = DEFAULT_GRID_SIZE;
    int columns = DEFAULT_GRID_SIZE;
    int time_steps = 10;

    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Parse arguments using getopt if an argument is passed
    while ((arg = getopt(argc, argv, "s:")) != -1)
    {
        switch (arg)
        {
        case 's':
            rows = atoi(optarg);
            columns = atoi(optarg);
            break;
        }
    }

    // Ensure rows and columns are greater than 0
    if (rows <= 0 || columns <= 0)
    {
        if (rank == 0)
        {
            printf("Number of rows and columns must be greater than 0. Exiting.\n");
        }
        MPI_Finalize();
        return 0;
    }

    // Ensure rows are divisible by the number of processes and that the number of processes is less than 4
    /* 
    Note: I did this because I was struggling so much with 
    implementing remainder rows and rows per process alonmg MPI_Gatherv I just gave up.
    */
    if (rows % size != 0)
    {
        if (rank == 0)
        {
            printf("Number of rows is not divisible by the number of processes. Exiting.\n");
        }
        MPI_Finalize();
        return 0;
    }

    // Ensure the number of processes is less than 4
    if (size > 4)
    {
        if (rank == 0)
        {
            printf("Number of processes must be less than 4. Exiting.\n");
        }
        MPI_Finalize();
        return 0;
    }

    // Initialize the seed for rand based on the current time and rank
    srand(time(NULL) + rank); 

    // Determine the number of rows we have per process
    int rows_per_process = rows / size;
    int remainder_rows = rows % size;
    if(rank == size - 1)
    {
        rows_per_process += rows % size;
    }

    // Don't forget to track the ghost rows and columns for when they are needed
    int rows_with_ghost = rows_per_process + 2;
    int cols_with_ghost = columns + 2;

    // Allocate space for the current grid and next grid (include ghost rows and columns)
    int **cur_grid = (int **)malloc(rows_with_ghost * cols_with_ghost * sizeof(int *));
    int **next_grid = (int **)malloc(cols_with_ghost * cols_with_ghost * sizeof(int *));
    for (int i = 0; i <= rows + 1; i++)
    {
        cur_grid[i] = (int *)malloc((cols_with_ghost) * sizeof(int));
        next_grid[i] = (int *)malloc((cols_with_ghost) * sizeof(int));
    }

    // Initialize a subgrid for each process
    init_subgrid(cur_grid, next_grid, rows_per_process, columns, rank, size);

    //print_full_grid_with_ghosts(cur_grid, rows_per_process, columns, rank, size, 0);

    // Find next-lowest rank process
    int prev_rank;
    if (rank == 0)
        prev_rank = size - 1;
    else
        prev_rank = rank - 1;

    // Find next-highest rank process
    int next_rank;
    if (rank == size - 1)
        next_rank = 0;
    else
        next_rank = rank + 1;

    // Simulate the game of life for the specified number of time steps
    for (int t = 0; t < time_steps; t++)
    {
        // Step 1: Update the ghost rows / edges
        update_ghost_rows(cur_grid, rows_per_process, columns, prev_rank, next_rank);

        // Step 2: Print the current full grid
        print_full_grid(cur_grid, rows_per_process, columns, rank, size, t);

        // Step 3: Apply the rules to the current grid to determine the next grid
        apply_rules(cur_grid, next_grid, rows, rows_per_process, columns, rank, size);

        // Step 4: Copy the next grid into the current grid (excluding ghost rows and columns)
        for(int cur_row = 1; cur_row <= rows_per_process; cur_row++)
        {
            for(int cur_col = 1; cur_col <= columns; cur_col++)
            {
                cur_grid[cur_row][cur_col] = next_grid[cur_row][cur_col];
            }
        }

        // Step 5: Print loop until we have reached the specified number of time steps
    }

    // Free any memory we allocated
    for (int i = 0; i < rows; i++) {
        free(cur_grid[i]);
        free(next_grid[i]);
    }
    free(cur_grid);
    free(next_grid);

    // Finalize MPI even though this leaks like crazy
    MPI_Finalize();

    return 0;
}

// Initialize each subgrid for each MPI process (each cell gets a random state of rank+1 or 0)
// I use rank+1 so that the cells are not 0 (which is DEAD)
void init_subgrid(int **cur_grid, int **next_grid, int rows, int columns, int rank, int size)
{
    int rows_with_ghost = rows + 2;
    int cols_with_ghost = columns + 2;

    // Initialize subgrid cells (not including ghost rows and columns)
    for (int i = 1; i <= rows; i++)
    {
        for (int j = 1; j <= columns; j++)
        {
            // Randomly set the cell to ALIVE or DEAD
            cur_grid[i][j] = (rand() % 2) ? rank + 1 : DEAD;
        }
    }

    // Initialize subgrid ghost rows
    for (int i = 0; i < rows_with_ghost; i++)
    {
        if (i == 0 || i == rows_with_ghost - 1)
        {
            for (int j = 0; j < cols_with_ghost; j++)
            {
                cur_grid[i][j] = 0;
            }
        }
    }

    // Initialize subgrid ghost columns
    for (int i = 0; i < rows_with_ghost; i++)
    {
        cur_grid[i][0] = 0;                   // Initialize left ghost column
        cur_grid[i][cols_with_ghost - 1] = 0; // Initialize right ghost column
    }

    // Initialize next grid
    for (int i = 0; i < rows_with_ghost; i++)
    {
        for (int j = 0; j < cols_with_ghost; j++)
        {
            next_grid[i][j] = 0;
        }
    }
}

// Update the ghost rows / edges of a subgrid so that we can correctly apply the rules
void update_ghost_rows(int **cur_grid, int rows, int columns, int prev_rank, int next_rank)
{
    int rows_per_process = rows;
    int cols_with_ghost = columns + 2;

    // Send the bottom row of the current subgrid to the top of the next subgrid
    MPI_Send(cur_grid[rows_per_process], cols_with_ghost, MPI_INT, next_rank, 0, MPI_COMM_WORLD);

    // Receive the data from the bottom row of the previous subgrid to update the top ghost row
    MPI_Recv(cur_grid[0], cols_with_ghost, MPI_INT, prev_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Send the top row of the current subgrid to the bottom of the previous subgrid
    MPI_Send(cur_grid[1], cols_with_ghost, MPI_INT, prev_rank, 0, MPI_COMM_WORLD);

    // Receive the data from the top row of the next subgrid to update the bottom ghost row
    MPI_Recv(cur_grid[rows_per_process + 1], cols_with_ghost, MPI_INT, next_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

// Print the full grid (excluding ghost rows and columns) for each process
void print_full_grid(int **cur_grid, int rows, int columns, int rank, int size, int timestep)
{
        // Get the string representation of the local grid (subgrid)
        char *local_grid_str = (char *)malloc(sizeof(char) * (rows * columns * 3 + 1)); // Assuming single-digit numbers (0-4)
        local_grid_str[0] = '\0';                                                       // Initialize the string as empty
        for (int i = 1; i <= rows; i++)
        {
            for (int j = 1; j <= columns; j++)
            {
                char buf[4];
                sprintf(buf, "%d ", cur_grid[i][j]);
                strcat(local_grid_str, buf);
            }
            strcat(local_grid_str, "\n");
        }

        // Initialize the receive buffer, receive counts, and displacements for MPI_Gatherv
        char *recv_buffer = NULL;
        int *recv_counts = NULL;
        int *displs = NULL;
        if (rank == 0)
        {
            recv_buffer = (char *)malloc(sizeof(char) * (rows * columns * 3 + 1) * size);
            recv_counts = (int *)malloc(sizeof(int) * size);
            displs = (int *)malloc(sizeof(int) * size);

            for (int p = 0; p < size; p++)
            {
                recv_counts[p] = strlen(local_grid_str) + 1;
                displs[p] = p * (rows * columns * 3 + 1);
            }
        }

        // Gather the data strings from all processes to rank 0
        MPI_Gatherv(local_grid_str, strlen(local_grid_str) + 1, MPI_CHAR, recv_buffer, recv_counts, displs, MPI_CHAR, 0, MPI_COMM_WORLD);

        // Print the complete grid on rank 0
        if (rank == 0)
        {
            printf("Complete Grid at time step %d:\n", timestep);
            for (int p = 0; p < size; p++)
            {
                printf("%s", &recv_buffer[p * (rows * columns * 3 + 1)]);
            }
            printf("\n");
        }

        // Free any memory we allocated
        free(local_grid_str);
}

// Print the full grid (including ghost rows and columns) for each process
void print_full_grid_with_ghosts(int **cur_grid, int rows, int columns, int rank, int size, int timestep)
{
    // Get the string representation of the local grid (subgrid) with ghost rows and columns
    char *local_grid_str = (char *)malloc(sizeof(char) * (rows + 2) * (columns + 2) * 3 + 1); // Assuming single-digit numbers (0-4)
    local_grid_str[0] = '\0';                                                                 // Initialize the string as empty
    for (int i = 0; i < rows + 2; i++)
    {
        for (int j = 0; j < columns + 2; j++)
        {
            char buf[4];
            sprintf(buf, "%d ", cur_grid[i][j]);
            strcat(local_grid_str, buf);
        }
        strcat(local_grid_str, "\n");
    }

    // Initialize the receive buffer, receive counts, and displacements for MPI_Gatherv
    char *recv_buffer = NULL;
    int *recv_counts = NULL;
    int *displs = NULL;
    if (rank == 0)
    {
        recv_buffer = (char *)malloc(sizeof(char) * (rows + 2) * (columns + 2) * 3 * size + 1);
        recv_counts = (int *)malloc(sizeof(int) * size);
        displs = (int *)malloc(sizeof(int) * size);

        for (int p = 0; p < size; p++)
        {
            recv_counts[p] = strlen(local_grid_str) + 1;
            displs[p] = p * (rows + 2) * (columns + 2) * 3 + 1;
        }
    }

    // Gather the data strings from all processes to rank 0
    MPI_Gatherv(local_grid_str, strlen(local_grid_str) + 1, MPI_CHAR, recv_buffer, recv_counts, displs, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Print the complete grid on rank 0
    if (rank == 0)
    {

        printf("Complete Grid at time step %d:\n", timestep);
        for (int p = 0; p < size; p++)
        {
            printf("%s", &recv_buffer[p * (rows + 2) * (columns + 2) * 3 + 1]);
            if(p < size - 1)
                printf("\n");
        }
        printf("----------------------\n");
    }

    // Free any memory we allocated
    free(local_grid_str);
}

// Apply the rules of Conway's Game of Life to the current subgrid to determine the next subgrid
void apply_rules(int **cur_grid, int **next_grid, int default_rows, int rows_per_process, int columns, int rank, int size)
{
    
    // Iterate through each row of the subgrid
    for (int cur_row = 1; cur_row <= rows_per_process; cur_row++)
    {
        #ifdef DEBUG
        printf("Rank %d: Row %d\n", rank, cur_row);
        #endif
        // For each column of the subgrid, do the following:
        for (int cur_col = 1; cur_col <= columns; cur_col++)
        {
            #ifdef DEBUG
            printf("\t\tCell %d (%d)\n", cur_col, cur_grid[cur_row][cur_col]);
            #endif
            int alive_neighbors = 0;

            // For each row of the cell's neighbors, do the following:
            for (int my_neighbor_row = cur_row - 1; my_neighbor_row <= cur_row + 1; my_neighbor_row++)
            {
                // Skip ghost rows if we are on the top or bottom edge of the grid
                if ((cur_row == 1 && my_neighbor_row < 1 && rank == 0) || (cur_row == rows_per_process && my_neighbor_row > rows_per_process && rank == size-1))
                    continue;

                // For each column of the cell's neighbors, do the following:
                for (int my_neighbor_column = cur_col - 1; my_neighbor_column <= cur_col + 1; my_neighbor_column++)
                {
                    // Check if we are looking at the current cell
                    if (my_neighbor_row == cur_row && my_neighbor_column == cur_col)
                        continue;

                    // Skip ghost columns if we are on the left or right edge of the grid
                    if ((cur_col == 1 && my_neighbor_column < 1) || (cur_col == columns && my_neighbor_column > columns))
                        continue;

                    // If the neighbor is alive (value>0) then increment the alive_neighbors counter
                    if (ALIVE(cur_grid[my_neighbor_row][my_neighbor_column]))
                    {
                        alive_neighbors++;
                        #ifdef DEBUG
                        printf("\t\t\t%d %d\n", my_neighbor_row, my_neighbor_column);
                        #endif
                    }
                }
            }

            #ifdef DEBUG
            printf("\t\t\tTotal Neighbors: %d\n", alive_neighbors);
            #endif

            // Apply the rules of Conway's Game of Life
            if (ALIVE(cur_grid[cur_row][cur_col]))
            {
                // If cell is alive and has less than 2 or more than 3 alive neighbors, it dies
                if (alive_neighbors < 2 || alive_neighbors > 3)
                {
                    next_grid[cur_row][cur_col] = DEAD;
                    #ifdef DEBUG
                    printf("\t\t\tCell %d died\n", cur_col);
                    #endif
                }
                // Otherwise, it remains alive and its value is the rank of the process that spawned it + 1 (so that it is not 0)
                else
                {
                    next_grid[cur_row][cur_col] = rank + 1;
                    #ifdef DEBUG
                    printf("\t\t\tCell %d survived\n", cur_col);
                    #endif
                }
            }
            else
            {
                // If cell is dead and has 3 alive neighbors, it comes to life
                // The value of the cell is the rank of the process that spawned it + 1 (so that it is not 0)
                if (alive_neighbors == 3)
                {
                    next_grid[cur_row][cur_col] = rank + 1;
                    #ifdef DEBUG
                    printf("\t\t\tCell %d came to life\n", cur_col);
                    #endif
                }
                // Otherwise, it remains dead
                else
                {
                    next_grid[cur_row][cur_col] = DEAD;
                    #ifdef DEBUG
                    printf("\t\t\tCell %d remained dead\n", cur_col);
                    #endif
                }
            }
        }
    }
}