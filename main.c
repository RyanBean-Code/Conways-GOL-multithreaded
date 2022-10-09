#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>

typedef struct cell     {
    int alive,                  // 1 means alive 0 means dead
        generation,             // generation the cell was born into, not sure if needed yet
        row,                    // row position of the cell
        col;                    // column position of the cell
    struct cell neighbors[8];   // array of each other the cells neighbors
} Cell;


int main(int argc, char* argv[])    {
    
}