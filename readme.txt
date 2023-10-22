CIS 3090 Assignment 2 - Game of Life MPI
Author: Kyle Lukaszek
ID: 1113798
Due: October 20th, 2023



COMPILATION INSTRUCTIONS:

To compile the program, run the following command:

"make"

To compile the program with RULE debugging enabled, run the following command:

"make DEBUG=1"

To clean the directory, run the following command:

"make clean"



RUNNING INSTRUCTIONS:

To run the program, run the following command:

mpirun -n <number of processes: default = 1 (max:4)> ./a2 -s <size of board: default = 24 (24x24)>

The program will run and output the results to the terminal.



NOTES: 

The program will output 10 generations of the game of life to the terminal.

The default board size is 24x24, and the default number of processes is 1.

The states of the board are represented by the following characters:

    0 - Dead
    1-4 - Alive

A cell labeled as 1 means that the cell was spawned by process at rank 0, 2 means that the cell was spawned by process at rank 1, etc.
I chose to implement it like this because it was the easiest way to keep track of which process spawned which cell.

Just to clarify, the program will output the board after each generation, and then output the next generation after that.

Do not forget that 0 is not the rank that spawned it, 0 means that the cell is DEAD.

MISSING FUNCTIONALITY:

I am missing the functionality to handle when the board size is not divisible by the number of processes.
This is because I spent too much time trying to figure out how I could get the program to output the correct grids to console,
and whenever I tried to implement the functionality to handle the remainder of the board, it would break the program.