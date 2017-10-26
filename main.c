#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>

static const int TERM_CRITERION = 1;

// Specifying some constants
const uint64_t ZERO = 0x1p0 - 1;
const uint64_t B32 = 0x1p32 - 1;
const uint64_t B16 = 0x1p16 - 1;
const uint64_t B08 = 0x1p8 - 1;
const uint64_t B04 = 0x1p4 - 1;
const uint64_t B02 = 0x1p2 - 1;
const uint64_t B01 = 0x1p1 - 1;
uint64_t BORDER = 0x1p0 - 1;
uint64_t BOARD = 0x1p0 - 1;

// Contains bit-masks masking the lowest n bits
const uint64_t B_LVL[] = {B01, B02, B04, B08, B16, B32};

// Size of the board (number of holes
const int NUMBOARDBITS = 41;

// After initialization, this array will contain the bit-numbers of all holes starting
// from the top (left to right in the rows).
int BOARDBITS[NUMBOARDBITS] = {0};

// Number of bits that specify the border. Note that several bits are used as border
// on both sides of the board. This is due to the addressing scheme of the board and
// does not do any harm.
const int NUMBOUNDARYBITS = 15;

// The actual bits that specify the border of the board
const int BOUNDARYBITS[] = {5, 14, 23, 32, 41, 50, 39, 28, 17, 6, 59, 25, 36, 47, 58};

// Specifies the corners (edge) of the board. Required to prevent moves into the corners
// (or actually try them later)
const int NUMCORNERS = 16;
const int CORNERBITS[] = {4, 13, 22, 31, 40, 29, 18, 7, 60, 51, 42, 33, 24, 35, 46, 57};

// actual bit-mask for all the corner holes
uint64_t CORNERS = UINT64_C(0);

// How many rows (columns) does the board have
const int NUMROWS = 9;

// Operations for moving the pegs. An UP-operation will cause all pegs of the board to be
// moved up by one (some might be moved into the boundary).
static const int UP = 1;
static const int DOWN = -1;
static const int LEFT = -10;
static const int RIGHT = +10;
static const int DIRECTIONS[] = {DOWN, LEFT, UP, RIGHT};


// Constants for the transposition table. Using this table can significantly reduce the
// the efforts for the backtracking algorithm, since re-occuring positions do not have to
// be searched twice. Permutations of one move sequence might lead to the same position,
// which only has to be investigated once.
static const int HASHSIZE = (1 << 27);
static const int HASHMASK = HASHSIZE - 1;
static const int HASHMISS = -99;

// Number of symmetries. In total there are actually 8 symmetric positions for each board.
// Currently, only vertically and horizontally mirrored positions are considered. Rotations
// are not yet implemented.
static const int NUMSYMMETRIES = 4;

// Masks for horizontal and vertical lines. Needed for mirroring a board along the vertical or
// horizontal axis
static uint64_t HORLINES[NUMROWS] = {0};
static uint64_t VERTLINES[NUMROWS] = {0};

// Definition of one element of the transposition table. It contains a key (actual board, since several positions
// can be mapped to the same hash-table entry) and a value (number of remaining pegs when solving this specific
// position).
struct HashElement {
    uint64_t key;
    int value;
};
struct HashElement hashTable[HASHSIZE];

/*
 * Modulo operator, since the %-operator is the remainder and cannot deal with negative integers
 */
int mod(int a, int b) {
    int r = a % b;
    return r < 0 ? r + b : r;
}

/*
 * Determines the position of a single bit in a 64bit variable in logarithmic time.
 */
int bitPos(uint64_t x) {
    int bPos = 0;
    for (int i = 5; i >= 0; i--) {
        if ((x & B_LVL[i]) == ZERO) {
            int nBits = (int) B01 << i;
            x >>= nBits;
            bPos += nBits;
        }
    }
    return bPos;
}

/*
 * Fast way to count the one-bits in a 64-bit variable.
 * Only requires as many iterations as bits are set.
 */
int bitCount(uint64_t x) {
    int c = 0;
    while (x != ZERO) {
        x &= (x - UINT64_C(1));
        c++;
    }
    return c;
}

/*
 * Rotatate 64-bit variable x by y bits. Note that this is not a shift-operation but a real
 * rotate-left
 */
static inline uint64_t rol(uint64_t x, unsigned int y) {
    __asm__ ("rolq %1, %0" : "+g" (x) : "cJ" ((unsigned char) y));
    return x;
}

/*
 * Compute the bit indexes of the board from top to bottom (left to right in the rows) and from left to
 * right (from top to bottom in each column)
 */
void initBOARDBits() {
    int i, startRow = 4, startCol = 24, nrow = 1, j, k, l = 0, m;
    // start with top cell and move line by line through
    // the board (from left to right in each row).
    for (i = 0; i < NUMROWS; i++) {
        k = startRow;
        m = startCol;
        for (j = 0; j < nrow; j++, k = (k + 10) % 64, m--) {
            BOARDBITS[l++] = k;
            HORLINES[i] |= (UINT64_C(1) << k);
            VERTLINES[i] |= (UINT64_C(1) << m);
        }
        nrow = (i < 4 ? nrow + 2 : nrow - 2);

        // % operator is a remainder operator and not a modulo operator
        startRow = mod((i < 4 ? startRow - 11 : startRow + 9), 64);
        startCol = mod((i < 4 ? startCol + 11 : startCol + 9), 64);
    }
}

/*
 * Initialize the bit-masks that describe the boundary and the actual board.
 */
void initBoardnBoundary() {
    for (int i = 0; i < NUMBOUNDARYBITS; i++)
        BORDER |= (UINT64_C(1) << BOUNDARYBITS[i]);
    for (int i = 0; i < NUMBOARDBITS; i++)
        BOARD |= (UINT64_C(1) << BOARDBITS[i]);
}

/*
 * Init the bit-mask representing all 16 corner holes of the board
 */
void initCorners() {
    CORNERS = UINT64_C(0);
    for (int i = 0; i < NUMCORNERS; i++) {
        CORNERS |= (UINT64_C(1) << CORNERBITS[i]);
    }
}

/*
 * Initialize the transposition table
 */
void initHashTable() {
    for (int i = 0; i < HASHSIZE; i++) {
        hashTable[i].key = 0UL;
        hashTable[i].value = 0;
    }
}

void init() {
    initBOARDBits();
    initBoardnBoundary();
    initHashTable();
    initCorners();
}


/*
 * Function to print the board to console
 */
void printBoard(uint64_t b) {
    int l = 0, nrow = 1;
    printf("\n");
    for (int i = 0; i < NUMROWS; i++) {

        for (int k = 0; k < NUMROWS - nrow; k++)
            printf(" ");
        printf("|");
        for (int j = 0; j < nrow; j++) {
            //char symb = 'o';
            char symb = ((1UL << BOARDBITS[l++]) & b) != ZERO ? 'x' : 'o';
            printf("%c|", symb);
        }
        printf("\n");
        nrow = (i < 4 ? nrow + 2 : nrow - 2);
    }
    printf("\n");
}

/*
 * Removes one peg from the board and returns the modified board
 */
uint64_t removePeg(uint64_t b, int bit) {
    return b & (~(1UL << bit));
}

/*
 * Place a peg at a certain position and return the modified board
 */
uint64_t setPeg(uint64_t b, int bit) {
    return b | (1UL << bit);
}


/*
 * Function to compute the hash for a 64bit variable. Maybe Zobrist keys would work better (has to be investigated
 * in future).
 */
uint64_t getHash(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

/*
 * Mirror a board along the horizontal axis. This can be done quite fast.
 */
uint64_t mirrorHor(uint64_t b) {
    uint64_t m = b & HORLINES[4];
    m |= rol(b & HORLINES[0], 8 * DOWN); // First row
    m |= rol(b & HORLINES[1], 6 * DOWN); // Second row
    m |= rol(b & HORLINES[2], 4 * DOWN); // Third row
    m |= rol(b & HORLINES[3], 2 * DOWN); // Fourth row
    m |= rol(b & HORLINES[5], 2 * UP);   // Sixth row
    m |= rol(b & HORLINES[6], 4 * UP);   // Seventh row
    m |= rol(b & HORLINES[7], 6 * UP);   // Eighth row
    m |= rol(b & HORLINES[8], 8 * UP);   // Ninth row
    return m;
}

/*
 * Mirror a board along the vertical axis. This can be done quite fast.
 */
uint64_t mirrorVert(uint64_t b) {
    uint64_t m = b & VERTLINES[4];
    m |= rol(b & VERTLINES[0], 8 * RIGHT); // First Column
    m |= rol(b & VERTLINES[1], 6 * RIGHT); // Second Column
    m |= rol(b & VERTLINES[2], 4 * RIGHT); // Third Column
    m |= rol(b & VERTLINES[3], 2 * RIGHT); // Fourth Column
    m |= rol(b & VERTLINES[5], 2 * LEFT); // Sixth Column
    m |= rol(b & VERTLINES[6], 4 * LEFT); // Seventh Column
    m |= rol(b & VERTLINES[7], 6 * LEFT); // Eighth Column
    m |= rol(b & VERTLINES[8], 8 * LEFT); // Ninth Column
    return m;
}

/*
 * Compute all mirrored positions for a board b and return all in the array m
 */
void mirror(uint64_t b, uint64_t m[]) {
    m[0] = b;
    m[1] = mirrorVert(b);
    m[2] = mirrorHor(b);
    m[3] = mirrorHor(m[1]);
}

/*
 * Check if a position b or a mirrored equivalent is already stored in the transposition table. If yes, then
 * return the value for this position.
 */
int getTransposition(uint64_t b) {
    uint64_t m[NUMSYMMETRIES];
    mirror(b, m);

    for (int i = 0; i < NUMSYMMETRIES; i++) {
        uint64_t hash = getHash(m[i]);
        int hashIndex = ((int) hash & HASHMASK);
        if (hashTable[hashIndex].key == m[i])
            return hashTable[hashIndex].value;
    }
    return HASHMISS;
}

/*
 * After a position is completely evaluated, store the value of the position in the transposition table
 */
void putTransposition(uint64_t b, int value) {
    uint64_t hash = getHash(b);
    int hashIndex = ((int) hash & HASHMASK);
    hashTable[hashIndex].key = b;
    hashTable[hashIndex].value = value;
}

/*
 * Generates a list of possible moves in all directions for a board position b. Requires an
 *
 */
void generateMoves(uint64_t b, uint64_t *const allmv) {// Generate all possible moves
    int dir;
    uint64_t mv;
    for (int i = 0; i < 4; i++) {
        dir = DIRECTIONS[i];
        mv = rol(rol(b, dir) & BOARD & b, dir) & BOARD & (~b);
        uint64_t cmv = mv & CORNERS; // find all moves into corners
        uint64_t zmv = mv & UINT64_C(1); // Move into the center is not good as well

        // also some fields in each direction should be avoided first. These typically
        // violate a so called Pagoda function
        uint64_t dmv = UINT64_C(0);
        if (dir == DOWN) {
            dmv = mv & (UINT64_C(1) << 9 | UINT64_C(1) << 53 | UINT64_C(1) << 62);
        } else if (dir == UP) {
            dmv = mv & (UINT64_C(1) << 2 | UINT64_C(1) << 11 | UINT64_C(1) << 55);
        } else if (dir == LEFT) {
            dmv = mv & (UINT64_C(1) << 44 | UINT64_C(1) << 53 | UINT64_C(1) << 55);
        } else if (dir == RIGHT) {
            dmv = mv & (UINT64_C(1) << 9 | UINT64_C(1) << 11 | UINT64_C(1) << 20);
        }

        // Remove these moves from the initial move list and try them later,
        // since they are likely sub-optimal
        uint64_t mvLater = cmv | zmv | dmv;
        mv ^= mvLater;
        allmv[i] = mv;
        allmv[i + 4] = mvLater;
    }
}


int backtrack(uint64_t b);

/*
 * Try all moves coded in mv (contains the destination holes) for a board b in a direction dir. Return, if a terminal
 * position is reached.
 */
int tryMoves(uint64_t b, uint64_t mv, int dir) {
    while (mv != ZERO) {
        uint64_t x = ((mv - UINT64_C(1)) ^ mv) & mv;

        // perform move
        b |= x; // set peg at new position
        b &= ~rol(x, -dir); // remove jumped-over peg
        b &= ~rol(x, -2 * dir); // remove  peg from old position

        //recursion
        int res = backtrack(b);

        // undo move
        b &= ~x; // remove peg from new position again
        b |= rol(x, -dir); // add jumped-over peg again
        b |= rol(x, -2 * dir); // set peg to old position

        //printBoard(b);
        mv &= (mv - 1); // remove this move from the list

        if (res > 0 && res <= TERM_CRITERION) {
            printf("Move: %d, %d", dir, bitPos(x));
            printBoard(b);
            return res;
        }

    }
    return 0; // no solution found
}

/*
 * Backtracking function to solve the board. It investigates all moves in the 4 possible directions. It randomly
 * selects the first direction to enforce different searching order in case the solver is started several times.
 * The possible moves for one position can be found very fast with only a dew bitwise operations.
 */
int backtrack(uint64_t b) {
    // first check transposition table for this particular position
    int value = getTransposition(b);
    if (value != HASHMISS)
        return value;

    // will contain all possible moves later. Indexes 0-3 will contain the most promising moves in all 4 directions,
    // and indexes 4-7 will contain the less promising moves in all directions
    const int numTrys = 8;
    uint64_t allmv[numTrys];
    int dir;

    // Find all possible moves, sorted according to some characteristics
    generateMoves(b, allmv);


    uint64_t mv;
    int res, nomv = 0;
    // Try all possible moves.
    for (int i = 0; i < numTrys; i++) {
        mv = allmv[i];
        dir = DIRECTIONS[i & 3]; // = i % 4
        if (mv != ZERO) {
            res = tryMoves(b, mv, dir);
            if (res > 0 && res <= TERM_CRITERION) {
                // Not neccessary to put position in transposition table
                return res;
            }
        } else // if no moves in specific direction is possible
            nomv++;
    }

    int ret = 0;
    if (nomv == numTrys) { // if no move in any direction was possible
        ret = bitCount(b); // count number of pegs left
    }

    putTransposition(b, ret);
    return ret; // no move could lead to a solution

}


/*
 * Root-node of the solver. Initializes the board and all other necessary variables
 * and then starts an exhaustive search.
 */
void solve() {
    init();

    // initial board
    uint64_t b = BOARD;
    b = removePeg(b, 57); // remove one peg

    // Start back-tracking
    backtrack(b);
}

int main() {
    printf("Lets start solving the Diamond-41 peg solitaire problem...");
    fflush(stdout);
    time_t start, end;
    double elapsed;  // seconds
    start = time(NULL);
    srand(start); // initialize random generator
    solve();
    end = time(NULL);
    elapsed = difftime(end, start);

    printf("Time in minutes: %f\n", elapsed / 60.0);

    return 0;
}



//    b = removePeg(b, 4);
//    b = removePeg(b, 13);
//    b = removePeg(b, 2);
//    b = removePeg(b, 55);
//    b = removePeg(b, 44);
//    b = removePeg(b, 1);
//    b = removePeg(b, 10);
//    b = removePeg(b, 63);
//    b = removePeg(b, 62);
//    b = removePeg(b, 51);
//    b = removePeg(b, 9);

/*           |o|
           |o|o|o|
         |o|o|o|o|o|
       |x|o|o|o|o|o|o|
     |o|o|o|o|o|o|o|o|o|
       |o|o|o|o|x|o|o|
         |o|o|o|x|o|
           |o|x|o|
             |x|
 */
//printBoard(b);