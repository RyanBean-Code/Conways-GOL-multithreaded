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

Cell *grid;                     // 2D array representing the matrix of all the cells
Cell *local_grid;               // 2D array representing the matrix of just the process
int num_gens;                   // number of generations to simulate
int curr_gen;                   // current generation of the program;
MPI_Datatype cell_type_mpi;     // data type of the cell mpi

int world_size;                 // number of processors
int rank;

// functions
void get_seeds(int* seeds);
void send_0_to_all_other(const void* send, const void* recv, MPI_Datatype type);
void print_grid();
void GenerateInitialGOL();

int main(int argc, char* argv[])    {

    //  Initialize the mpi environnment
    MPI_Init( NULL , NULL);

    // int world_size;
    // Get the number of processors
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // int rank;
    // Get the rank of each process
    MPI_Comm_rank( MPI_COMM_WORLD , &rank);

    // Creating a datatype for the cell
    MPI_Type_contiguous( 4 , MPI_INT , &cell_type_mpi);
    MPI_Type_commit( &cell_type_mpi);

    //  Get input from the user for the number of generations to simulate
    if (rank == 0)  {
        printf("-----------------------Conway's Game Of Life-----------------------\n");
        printf("Enter the number of Generations you wish to simulate (integer) : ");
        scanf("%d", &num_gens);
        putchar('\n');
    }
    MPI_Barrier(MPI_COMM_WORLD);

    // Send the number of generations to each process
    send_0_to_all_other(&num_gens, &num_gens, MPI_INT);

    // allocate space for the grid
    if (rank == 0)
        grid = (Cell *)malloc(NUM_COLS * NUM_ROWS * sizeof(Cell));

    // Get the seeds for generating random values 
    GenerateInitialGOL();

    // Gather all the cells info into rank 0
    MPI_Gather( local_grid , (NUM_COLS * NUM_ROWS / world_size) , cell_type_mpi , grid , (NUM_COLS * NUM_ROWS / world_size) , cell_type_mpi , 0 , MPI_COMM_WORLD);

    if (rank == 0)  print_grid();

    // Main Loop for simulation
    for (curr_gen = 0; curr_gen < num_gens; curr_gen++) {

    }

    free(grid);
    MPI_Finalize();
}

// int i = (rank * NUM_COLS) / world_size; i < 2 * rank + (NUM_COLS / world_size); i++
void GenerateInitialGOL()   {
    int *seeds = (int *) malloc(world_size * sizeof(int));
    int *seed = (int *) malloc(sizeof(int));
    if (rank == 0)
        get_seeds(seeds); //seed = seeds[0];
    MPI_Barrier(MPI_COMM_WORLD);

    // send the seeds to each rank
    MPI_Scatter( seeds , 1 , MPI_INT , seed , 1 , MPI_INT , 0 , MPI_COMM_WORLD);

    // printf("Rank=%d, seed=%d\n", rank, *seed);

    int s = NUM_COLS * NUM_ROWS / world_size;

    // Array used by each process to initialize thier elements
    // this will later be combined into the global grid element 
    local_grid = (Cell *) malloc(s * sizeof(Cell));

    // seed the random number generator with each unique seed
    srand(*seed);

    // loop to initalize the grid    
    for (int c = 0; c < NUM_COLS / world_size; c++) {
        for (int r = 0; r < NUM_ROWS; r++) {
            (local_grid + c*NUM_COLS + r)->alive = (rand() % 2 == 0) ? 1 : 0;
            (local_grid + c*NUM_COLS + r)->col = rank * (NUM_COLS / world_size) + c;
            (local_grid + c*NUM_COLS + r)->row = r;
            (local_grid + c*NUM_COLS + r)->gen = 0;
        }
    }

    // MPI_Allgather( local_grid , s  , cell_type_mpi , grid , s , cell_type_mpi , MPI_COMM_WORLD);

    // free(local_grid);
    free(seeds);
}

// function sends a value from rank 0 to all other processes
void send_0_to_all_other(const void* send, const void* recv, MPI_Datatype type)    {
    for (int i = 1; i < world_size; i++)    {
        if (rank == 0)  {
            MPI_Send(send, 1 , type , i , 0 , MPI_COMM_WORLD);
        }
        else if (rank == i) {
            MPI_Recv(recv, 1 , type , 0 , 0 , MPI_COMM_WORLD , MPI_STATUS_IGNORE);
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }
}

void get_seeds(int* seeds)  {
    srand(time(NULL));
    for (int i = 0; i < world_size; i++) *(seeds + i) = rand();
}

void print_grid()   {
    for (int r = 0; r < NUM_ROWS; r++)  {
        for (int c = 0; c < NUM_COLS; c++)  {
            if ((grid + c * NUM_COLS + r)->alive == 1)   {
                printf("X ");
            }
            else    {
                printf("0 ");
            }
        }
        putchar('\n');
    }
}