#include <fstream>
#include <iostream>

/*Output-Path for the generated moves*/
#define PATH "moves.txt"

/*
 * Structure of the board. It has a dimension of 9 x 9, to also allow to put a border
 * around the actual field.
 */
short int field[9][9];

/*
* Tracks the smallest number of pegs left on the board
*/
short int minimum = 99;

/*
* Keeps track of the current move sequence during the search
*/
short int move[31][3];

/*
* Initializes the board with proper values.
*/
void initialize(void);

/*
* The most important function of this program...
*/
void computation(short instance);

/*Counts the number of pegs on the board*/
short int CountStones(void);

/*Writes a found solution to a file*/
void WriteToFile(void);


int main(int argc, char* argv[])
{
    initialize();   // Prepare everyting
    std::cout << "The solution will be saved in moves.txt.";
    computation(0); // Start exhaustive search
    WriteToFile();  // Save solution
    std::cout << "I found a solution....";
    return 0;
}

void initialize(void)
{
    short int i,j;
    for(i=0;i<9;i++)
    {
        field[i][0]=2; // Top boundary of the board
        field[i][8]=2; // Bottom boundary of the board
        field[0][i]=2; // Left boundary of the board
        field[8][i]=2; // Right boundary of the board
    }
    for(i=1;i<8;i++)
        for(j=1;j<8;j++)
            field[i][j]=1;

    /* Fill the corners of the board with twos, since they don't have holes.*/
    /* Top left*/
    field[1][1]=2;
    field[2][1]=2;
    field[1][2]=2;
    field[2][2]=2;

    /*To right*/
    field[6][1]=2;
    field[7][1]=2;
    field[6][2]=2;
    field[7][2]=2;

    /*Bottom left*/
    field[1][6]=2;
    field[2][6]=2;
    field[1][7]=2;
    field[2][7]=2;

    /*bottom right*/
    field[6][6]=2;
    field[7][6]=2;
    field[6][7]=2;
    field[7][7]=2;

    /*The hole in the middle has to be empty*/
    field[4][4]=0;

}

/*Recursive function for backtracking

1. Find a peg
2. Find neighboring peg and check if there is a hole one further
3. Peg jumps over the other one into the hole. The peg has to be moved by 2 places and the other peg has to
be removed. */

void computation(short instance)
{
    short int i,j;              // Loop variables
    bool NoPossibilities=true;  // This variable tracks, if a move could be performed or not from the current position
    for(i=1;i<8;i++)      // Start with 1, since 0 is the boundary
        for(j=1;j<8;j++)  // The same here...
        {
            if(field[j][i]==1) // If there is a peg in this hole...
            {
                if(field[j+1][i]==1) // If there is another peg on the right...
                {
                    if(field[j+2][i]==0) // and if one further there is an empty hole
                    {
                        NoPossibilities=false;  // Set to false, since we have an option for a move
                        field[j][i]=0;         // Remove peg from its original position
                        field[j+1][i]=0;       // Remove the peg over which we jump
                        field[j+2][i]=1;       // Put first peg into new hole
                        move[instance][0]=j;   // Save old x-position of peg
                        move[instance][1]=i;   // Save old y-position of peg
                        move[instance][2]=20;  // Save direction of the move (20, for right)
                        computation(instance+1); // recursion step
                        if(minimum==1)           // In case a solution was found, stop the search
                            break;               // stop here...
                        field[j][i]=1;           // Undo move...
                        field[j+1][i]=1;
                        field[j+2][i]=0;
                    }
                }
                if(field[j-1][i]==1) // If there is another peg on the left...
                {
                    /* almost same as above */
                    if(field[j-2][i]==0)
                    {
                        NoPossibilities=false;
                        field[j][i]=0;
                        field[j-1][i]=0;
                        field[j-2][i]=1;
                        move[instance][0]=j;
                        move[instance][1]=i;
                        move[instance][2]=-20;
                        computation(instance+1);
                        if(minimum==1)
                            break;
                        field[j][i]=1;
                        field[j-1][i]=1;
                        field[j-2][i]=0;
                    }
                }
                if(field[j][i+1]==1) // If there is another peg below ...
                {
                    /* almost same as above */
                    if(field[j][i+2]==0)
                    {
                        NoPossibilities=false;
                        field[j][i]=0;
                        field[j][i+1]=0;
                        field[j][i+2]=1;
                        move[instance][0]=j;
                        move[instance][1]=i;
                        move[instance][2]=2;
                        computation(instance+1);
                        if(minimum==1)
                            break;
                        field[j][i]=1;
                        field[j][i+1]=1;
                        field[j][i+2]=0;
                    }
                }
                if(field[j][i-1]==1) // If there is another peg above ...
                {
                    /* almost same as above */
                    if(field[j][i-2]==0)
                    {
                        NoPossibilities=false;
                        field[j][i]=0;
                        field[j][i-1]=0;
                        field[j][i-2]=1;
                        move[instance][0]=j;
                        move[instance][1]=i;
                        move[instance][2]=-2;
                        computation(instance+1);
                        if(minimum==1)
                            break;
                        field[j][i]=1;
                        field[j][i-1]=1;
                        field[j][i-2]=0;
                    }
                }
            }
        }
    if(NoPossibilities) // If no move could be performed...
    {
        short int count;
        count=CountStones();  // Count remaining pegs
        if(count==1 && field[4][4] != 0) // In case only one peg is left in the center
            minimum=count;    // set minimum = 1, which stops the search
    }
}

/*
* Counts the number of pegs on the board
*/
short int CountStones(void)
{
    short int i, j, count=0;
    for(i=1;i<8;i++)      // Starts with 1, since 0 is the boarndary
        for(j=1;j<8;j++)  // same here....
        {
            if(field[i][j]==1) // In case there is a peg at this position
                count++;       // increase counter
        }
    return count;
}

/*
* Write solution to file....
*/
void WriteToFile(void)
{
    std::ofstream file;
    short int i, j;
    file.open(PATH);  // Create file specified above
    file << "Explanation: Every move consists of three values.\n";
    file << "1. X-Position of the peg\n";
    file << "2. Y-Position of the peg\n";
    file << "3. Direction, in which the selected peg jumps\n";
    file << "(20->right, -20 left, 2 down, -2 up)\n\n";
    for(i=0;i<31;i++) {
        for(j=0;j<3;j++)
            file << move[i][j] << '\n';
        file << '\n';
    }
    file.close();
}
