/*
    Ryan Bean
    Multithreaded implementation of Conways game of life using mpi.h
    Created for CptS 411 at WSU

    Build:  mpicc -o main main.c
    Run:    mpirun -np <number of processes> <path to main>
*/

#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

//  Number of rows and columns, dont have to be equal but they do have to be divisiable by the number of processes
#define NUM_ROWS    8
#define NUM_COLS    8

typedef struct cell     {
    int alive,                  // 1 means alive 0 means dead
        gen,                    // generation the cell was born into, not sure if needed yet
        row,                    // row position of the cell
        col;                    // column position of the cell
} Cell;

Cell *grid;                     // 2D array representing the matrix of all the cells
Cell *curr_local_grid;          // 2D array representing the matrix of just one process, of the current generation
Cell *new_local_grid;           // 2D array representing the matrix of just one process, of the next generation.
int num_gens;                   // number of generations to simulate
int curr_gen;                   // current generation of the program;
int num_cols_per_p;             // number of columns per process
int num_cells_per_p;            // number of cells per process
MPI_Datatype cell_type_mpi;     // data type of the cell mpi

int world_size;                 // number of processes
int rank;                       // rank of the process

double total_runtime;           // represents the runtime of the whole program
double total_commtime;          // represents the time taken by all the communication steps.


clock_t start_rt,               // the time at the start of the program
        end_rt,                 // the time at the end of the program
        start_ct,               // used to calculate communication time steps
        end_ct;                 // used to calculate communication time steps

MPI_Group world_group;
MPI_Group neighborhood_group;
MPI_Comm neighborhood_comm;

// functions
void get_seeds(int* seeds);
void simulate();
void print_grid();
void GenerateInitialGOL();
int get_cell_state_at_coordinate(int col, int row);
void get_neighbor_ranks(int *ranks);
int determine_state(int col, int row);
int get_neighbor_life_count(int col, int row);

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

    // start the runtime clock here
    start_rt = clock();

    // communication step so we start the comm time
    start_ct = clock();

    // Send the number of generations to each process
    MPI_Bcast( &num_gens , 1 , MPI_INT , 0 , MPI_COMM_WORLD);

    // communcation ends
    end_ct = clock();

    // update the total communcation time
    total_commtime = ((double)end_ct - (double)start_ct) / (double)CLOCKS_PER_SEC;

    // allocate space for the grid
    if (rank == 0) grid = (Cell *)malloc(NUM_COLS * NUM_ROWS * sizeof(Cell));

    // initiallizing some commonly used variables
    num_cells_per_p = NUM_COLS * NUM_ROWS / world_size;
    num_cols_per_p = NUM_COLS / world_size;

    // Get the seeds for generating random values 
    GenerateInitialGOL();

    // print beginning message
    if (rank == 0) {
        system("clear");
        printf("-----------Starting Simulation-----------\n");
    }
    
    // run simulation
    simulate();

    // simulation has finished so get the time.
    end_rt = clock();

    total_runtime = ((double)end_rt - (double)start_rt) / (double)CLOCKS_PER_SEC;
    
    double average_commtime, average_runtime;
    // do time calculations
    MPI_Allreduce( &total_commtime , &average_commtime , 1 , MPI_DOUBLE , MPI_SUM , MPI_COMM_WORLD);
    average_commtime = average_commtime / world_size;

    MPI_Allreduce( &total_runtime , &average_runtime , 1 , MPI_DOUBLE , MPI_SUM , MPI_COMM_WORLD);
    average_runtime = average_runtime / world_size;

    printf("Rank=%d, num processes=%d, grid size=%d, num generations=%d, total_commtime=%lf, total_runtime=%lf\n", rank, world_size, NUM_COLS, num_gens, average_commtime, average_runtime - average_commtime);

    // wsl all the allocated memory
    if (rank == 0) free(grid);

    free(curr_local_grid);
    free(new_local_grid);

    // Finalize the mpi
    MPI_Finalize();
}

void simulate()  {

    // communication step
    start_ct = clock();

    // organize processors before start of simulation
    MPI_Barrier(MPI_COMM_WORLD);

    end_ct = clock();
    total_commtime += ((double)end_ct - (double)start_ct) / (double)CLOCKS_PER_SEC;

    // initialize currrent generation to 0
    curr_gen = 0;

    // int used to determine the frequency of printing the grid, freq=1 -> everytime, freq=2 -> every other loop etc.
    int print_freq = 1;

    // this grid represents the local grid for each process for the next generation
    new_local_grid = (Cell *) malloc(num_cells_per_p * sizeof(Cell));

    // main loop for running program
    while (curr_gen < num_gens) {

        // print the grid
        if (curr_gen % print_freq == 0) {
            
            // get all the grids into rank 0
            MPI_Gather( curr_local_grid , num_cells_per_p , cell_type_mpi , grid , num_cells_per_p , cell_type_mpi , 0 , MPI_COMM_WORLD);

            // only rank 0 prints the grid
            if (rank == 0)  {
                printf("Generation %d\n", curr_gen); 
                print_grid();
            }
        }

        // communication step
        start_ct = clock();

        // line the processors back up to before the next generation
        MPI_Barrier(MPI_COMM_WORLD);

        end_ct = clock();
        total_commtime += ((double)end_ct - (double)start_ct) / (double)CLOCKS_PER_SEC;

        // nested which will initalize the new grid.
        // bassic logic for this is:
        // Each process is responsible for NUM_COLS / world_size
        // so ex. NUM_COLS = 16 and world_size = 8: each process is responsible for 2 columns
        // we update the columns then update each row in that column.
        for (int c = 0; c < num_cols_per_p; c++) {
            for (int r = 0; r < NUM_ROWS; r++)  {
                (new_local_grid + NUM_ROWS * c + r)->alive = determine_state(rank * num_cols_per_p + c, r);
            }
        }

        curr_local_grid = new_local_grid;

        // iterate the generation 
        curr_gen++;
    }
}

// Function Used for determining the state of a cell.
// Conway's game of life rules:
// i.   A living cell that has less than 3 living neighbors die
// ii.  A living cell with more than 5 living neighbors also dies
// iii. A living cell with between 3 and 5 living neighbors lives on to the next generation;
// iv.  A dead cell will come (back) to life, if it has between 3 and 5 living neighbors. 
int determine_state(int col, int row)   {

    // number of neighbors alive.
    int neighbor_life_count = get_neighbor_life_count(col, row);
    
    return (neighbor_life_count >= 3 && neighbor_life_count <= 5) ? 1 : 0;
}


int get_neighbor_life_count(int col, int row)   {
    int r, c;
    int count = 0;
    for (int i = row - 1; i <= row + 1; i++) {
        if (i >= NUM_ROWS)  {
            r = 0;
        }
        else if (i < 0) {
            r = NUM_ROWS - 1;
        }
        else {  
            r = i;
        }
        for (int j = col - 1; j <= col + 1; j++)    {
            if (j < 0)    {
                c = NUM_COLS - 1;
            }
            else if (j >= NUM_COLS) {
                c = 0;
            }
            else {
                c = j;
            }
            count += get_cell_state_at_coordinate(c, r);
        }
    }

    count -= get_cell_state_at_coordinate(col, row);
    return count;
}

void get_neighbor_ranks(int *ranks)    {

    // if it is the root rank, the left neighbor is the last column and the right neighbor is rank 1
    if (rank == 0)  {
        *ranks = world_size - 1;
        *(ranks + 1) = rank + 1;
    }
    // else if the rank is the the last root, the right neighbor in the root rank 0 and left neighbor second the last rank
    else if (rank == world_size - 1)    {
        *ranks = rank - 1;
        *(ranks + 1) = 0;
    }
    // otherwise it's a rank in the middle so the left neighbor is just rank - 1 and right is rank + 1
    else    {
        *ranks = rank - 1;
        *(ranks + 2) = rank + 1;
    }
}

void GenerateInitialGOL()   {
    int *seeds = (int *) malloc(world_size * sizeof(int));
    int *seed = (int *) malloc(sizeof(int));
    if (rank == 0)
        get_seeds(seeds); //seed = seeds[0];

    // communication step
    start_ct = clock();
    
    MPI_Barrier(MPI_COMM_WORLD);

    end_ct = clock();

    total_commtime += ((double)end_ct - (double)start_ct) / (double)CLOCKS_PER_SEC;

    // communication step
    start_ct = clock();

    // send the seeds to each rank
    MPI_Scatter( seeds , 1 , MPI_INT , seed , 1 , MPI_INT , 0 , MPI_COMM_WORLD);

    end_ct = clock();
    total_commtime += ((double)end_ct - (double)start_ct) / (double)CLOCKS_PER_SEC;

    // Array used by each process to initialize thier elements
    // this will later be combined into the global grid element 
    curr_local_grid = (Cell *) malloc(num_cells_per_p * sizeof(Cell));

    // seed the random number generator with each unique seed
    srand(*seed);

    int column;

    // loop to initalize the grid    
    for (int c = 0; c < NUM_COLS / world_size; c++) {
        for (int r = 0; r < NUM_ROWS; r++) {
            column = c*NUM_COLS;
            (curr_local_grid + column + r)->alive = (rand() % 2 == 0) ? 1 : 0;
            (curr_local_grid + column + r)->col = rank * num_cols_per_p + c;
            (curr_local_grid + column + r)->row = r;
            (curr_local_grid + column + r)->gen = 0;
        }
    }

    // MPI_Allgather( local_grid , s  , cell_type_mpi , grid , s , cell_type_mpi , MPI_COMM_WORLD);

    // free(local_grid);
    free(seeds);
    free(seed);
}

// int i = (rank * NUM_COLS) / world_size; i < 2 * rank + (NUM_COLS / world_size); i++
int get_cell_state_at_coordinate(int col, int row)   {

    // check if the col and row values are valid
    if (col >= NUM_COLS || row >= NUM_ROWS)   {
        printf("ERROR - get_cell_state_at_coordinate() row or col > NUM_COLS || NUM_ROWS\n");
        exit(1);
    }

    // eg. rank_of_owner = col 3 / (16 / 8) = rank 1
    int rank_of_owner = col / num_cols_per_p;
    int state = (curr_local_grid + NUM_ROWS * (col % num_cols_per_p) + row)->alive;
    int states[world_size];
    
    // communication step
    start_ct = clock();

    // send the state from the owner rank the the rank executing the function
    // not really likeing this method of doing is but couldn't get MPI_Sendrecv, or MPI_Bcast, or creating my own MPI_Comm to work
    MPI_Allgather( &state , 1 , MPI_INT , states , 1 , MPI_INT , MPI_COMM_WORLD);

    end_ct = clock();
    total_commtime += ((double)end_ct - (double)start_ct) / (double)CLOCKS_PER_SEC;

    return states[rank_of_owner];
}

void get_seeds(int* seeds)  {
    srand(time(NULL));
    for (int i = 0; i < world_size; i++) *(seeds + i) = rand();
}

void print_grid()   {
    // Gather all the cells info into rank 0
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