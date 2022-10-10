#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

//  Number of rows and columns, dont have to be equal but they do have to be divisiable by the number of processes
#define NUM_ROWS    16
#define NUM_COLS    16

typedef struct cell     {
    int alive,                  // 1 means alive 0 means dead
        gen,                    // generation the cell was born into, not sure if needed yet
        row,                    // row position of the cell
        col;                    // column position of the cell
} Cell;

Cell grid[NUM_ROWS][NUM_COLS];  // 2D array representing the matrix of all the cells
int num_gens;                   // number of generations to simulate
int curr_gen;                   // current generation of the program;

int world_size;                 // number of processors

// functions
void get_seeds(int* seeds);

int main(int argc, char* argv[])    {

    //  Initialize the mpi environnment
    MPI_Init( NULL , NULL);

    // int world_size;
    // Get the number of processors
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int rank;
    // Get the rank of each process
    MPI_Comm_rank( MPI_COMM_WORLD , &rank);

    //  Get input from the user for the number of generations to simulate
    if (rank == 0)  {
        printf("-----------------------Conway's Game Of Life-----------------------\n");
        printf("Enter the number of Generations you wish to simulate (integer) : ");
        scanf("%d", &num_gens);
        putchar('\n');
    }
    MPI_Barrier(MPI_COMM_WORLD);

    printf("Rank = %d created of %d processes\n", rank, world_size);

    // Get the seeds for generating random values 
    int seeds[world_size];
    int seed;
    if (rank == 0)
        get_seeds(seeds); seed = seeds[0];
    MPI_Barrier(MPI_COMM_WORLD);
    
    // send those seeds to each rank
    for (int i = 1; i < world_size; i++)    {
        if (rank == 0)  {
            MPI_Send( seeds + i , 1 , MPI_INT , i , 0 , MPI_COMM_WORLD);
        }
        else if (rank == i) {
            MPI_Recv( &seed , 1 , MPI_INT , 0 , 0 , MPI_COMM_WORLD , MPI_STATUS_IGNORE);
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    printf("Rank=%d, seed=%d\n", rank, seed);


    MPI_Finalize();
}

void get_seeds(int* seeds)  {
    srand(time(NULL));
    for (int i = 0; i < world_size; i++) *(seeds + i) = rand();
}