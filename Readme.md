You can find a blog post on this on https://www.markusthill.github.io

Many of us might now the board game peg solitaire and might even have one of its many variants at home. Peg solitaire is a one-player game played on a board with $$n$$ holes and $$n-1$$ pegs. The number of holes depends on the board variant. For example, the English variant consists of 33 holes while the typical diamond variant consists of 41 holes. The rules of the game are rather easy. In each move the player selects one peg and jumps -- either vertically or horizontally, not diagonally -- with this peg over a directly neighboring one into an empty hole. The neighboring peg is then removed, leaving an empty hole. So, in each move, one peg jumps 2 holes further and the peg in-between is removed. Once no move is possible any longer, the game is over. This is the case when there is no pair of pegs which are orthogonally adjacent or if only one peg is left. In the latter case the game is won.
The English variant, as shown below, has one additional rule: In order to win, it is not sufficient that only one peg is left in the end; this peg also has to be located in the center of the board. The English variant is shown in the figure below.

![English Peg Solitaire]({{ site.url }}/slides/peg-solitaire/solitaire1.png){: .image-center width="400px"}

Even though the rules of the game are rather simple, finding a solution is not trivial. Many players need quite a few attempts in order to find the solution for the English peg solitaire. The solution for the diamond shaped board is even more tricky.


## Efficiently Solving the Diamond-41 Board
As the name suggests, the Diamond-41 board consists of 41 holes. As I was told -- in contrast to the English variant -- the only initial empty hole is not located in the center of the board but has to be placed at a slightly different position in order to be able to solve the game (as I found later, the solver was not able to solve the game for an initially empty hole in the center of the board). The initial position is as follows:

![English Peg Solitaire]({{ site.url }}/slides/peg-solitaire2/Folie01.png){: .image-center width="400px"}

In the beginning, only two moves are possible. Since there are many corners on this board it is rather difficult to find a solution where only one peg is left over in the end. Also for backtracking algorithms the computational effort is enormous in order to solve this problem. Without the utilization of some advanced approaches, an algorithm could run for many days until a solution is found.

#### Bit-Boards
One major improvement of a classical algorithm can be achieved when instead of arrays, so called bit-boards are used for representing a position. For peg solitaire, the usage of bit-boards is especially easy, since each hole can either be empty (binary 0) or filled with a peg (binary 1). In this way, only 41 bit are required to encode any board position. Hence, a 64 bit variable is sufficient. There are also enough bits left over which can be used to encode the boundary of the board. This is, as we will see later, important for checking if a move is performed within the allowed boundaries. Bit boards have the advantage that the whole board can be processed at once with bitwise operations. If the encoding of each individual hole is done in a suitable way, many operations (such as the generation of valid moves for each position, mirroring the board along a certain axis, counting pegs, etc.) can be executed with a minimum amount of CPU cycles. Additionally, the representation is very memory efficient so that positions can be easily stored in a hash table or other data structures.

I designed the following layout for the bit-board representation of the Diamond-41 variant of peg solitaire.

![English Peg Solitaire]({{ site.url }}/slides/peg-solitaire2/0bitboard.png){: .image-center width="400px"}

Each number represents the corresponding bit in the bit-board. The center hole is placed at bit 0. The boundary of the board is shown by the squares in above diagram. Moves are not allowed to end up in any of these bits. Note that many boundary bit-numbers appear twice in the diagram. This is due to the special layout, which I will explain in the following. However, it is not necessary to have a separate bit for every point on the boundary, since it is only used to check if a move leaves the board.

The advantage of this arrangement is that all pegs can be easily moved vertically or horizontally. For example, a bitwise left rotation of the board by one would move all pegs up vertically by one. Accordingly, pegs are moved down by a right rotation of one (or left rotation of -1) and moved horizontally by bitwise rotations of the bit-board by 10 and -10. As we will see, this is especially helpful when we want to find all possible moves. This move generation can be done in very few CPU cycles, even for very complex board situations. Furthermore, mirroring of the board along the vertical and horizontal axis can also be done comparably fast. The only tradeoff is that rotations of a position cannot be computed easily. One solution for this problem could be, to always maintain two bit-boards: one for the original position and one for a rotated position (by 90 degrees). All other symmetric positions (8 overall) could then be constructed with vertical/horizontal flips.

For example, the remaining number of pegs can be computed with an efficient function that counts the number of set bits in a bit-field. Since we are normally interested to count the remaining pegs after no move is possible any more, there are typically not too many pegs left and the following function computes the number very fast:

```c
int bitCount(uint64_t x) {
    int c = 0;
    while (x != ZERO) {
        x &= (x - UINT64_C(1));
        c++;
    }
    return c;
}
```

If only three bits are set in a bit-field, above function would only require 3 iterations in order to count the set bits.

#### Move Generation

Even more important is the method to compute all the possible moves for a given board position `b`. All possible moves in one direction (in total 4 directions) can simply be computed with:

```c
uint64_t mv = rol(rol(b, dir) & BOARD & b, dir) & BOARD & (~b);
```

where `b` is an arbitrary board positon, `rol` is a bitwise rotation left (implemented with some inline assembler), `dir` is the specified direction (+1 for up, -1 for down, +10 for right, -10 for left) and `BOARD` is a bit-mask which masks all 41 holes of the board. The variable `mv` contains all the target holes for the allowed moves in the specified direction.
After all the possible moves (in `mv`) for a direction `dir` are known, each move can also be performed with a few bitwise operations. We can extract the next move, which should be performed with:

```c
// Get next move from all possibilities mv
uint64_t x = ((mv - 1) ^ mv) & mv;
```

In order to perform the move, we have to set a peg to the target position, we have to remove the "jumped-over" peg and then remove the peg from the old position:

```c
// perform move
b |= x; // set peg at new position
b &= ~rol(x, -dir); // remove jumped-over peg
b &= ~rol(x, -2 * dir); // remove  peg from old position
```

Undoing a move can be done accordingly. All of these steps require only a few bitwise operations. A solver with an array-based data structure for the board would likely perform the moves slightly faster, but I suppose that the move generation itself (which is commonly an expensive task) is done much faster with this bit-board design.

#### Move Ordering
It is as simple as that to find all possible moves in one direction. However, I added a few lines of code which allow a better move ordering, which is essential for finding a solution faster. With a standard move ordering the search might take many days. Hence, it might be reasonable to spend some CPU time on sorting the moves a little, in order to try more promising moves first. For example, the move ordering defers moves which end up in the corners of the board, since it is typically not that easy to get out of the corner again. Also some other types of moves are ordered to the back of the list, if they do not appear promising.

#### Symmetries & Transposition Tables
When traversing the search tree many positions repeat since permutations of a move sequence can lead to identical positions. Furthermore, the Diamond-41 board is symmetric, having mirror and rotation symmetries. Hence, we can save a lot of computation time if we already know the values of repeating positions and/or of their symmetric equivalents. The value of a position is the minimum number of remaining stones of the final state when an optimal move sequence is performed -- starting from this particular position. For example if we know that position $$b_1$$ has a corresponding value of $$3$$, then a position $$b_2$$, which is identical to $$b_1$$ after rotation, will also have a value of $$3$$. If we had a possibility to store the already known values of positions, then we could save some computation time when we observe a (symmetrically) equivalent position again, by simply retrieving the stored value and returning it. With such an approach, we could prune the search tree significantly and avoid redundant calculations without loosing any information. A common technique used for board games such as chess, checkers, etc., are the so called transposition tables. The idea is to define a suitable hash function which takes the board position (and no historic information of the move sequence which led to this position) and returns a hash value that can be mapped to an index in order to store the game-theoretic value of a position in a hash table. When we observe a new position we can then simply calculate the corresponding hash and check if there is an entry in the hash table. If we find an entry, we are lucky and can stop the search for the current position and cut off the connected sub-tree. If not, then we have to traverse the connected sub tree and then store the retrieved value afterwards in the table. For our problem, an entry in the table could look like this:

```c
struct HashElement {
    uint64_t key;
    int value;
};
```

Since we are not performing an Alpha-Beta search or similar, there is no need to store further information rather than a key and a value. Note that we also have to store a key, which in this case is simply the position itself, since many board positions will map to the same hash table index. This is due to the fact that the hash function itself is typically not injective as well as the modulo operation required to break down the hash value to an index. Since it is not possible to prevent collisions it is always necessary to compare the key of the stored element with the current board position. From this problem another question arises: What do we do, if we want to store a key-value pair in the transposition table, but the corresponding slot is already occupied by another position? The answer for our case is simple: We simply overwrite the older entry in the table. This approach is often sub-optimal, since we might destroy valuable information (especially the values of positions located close to the root node were expensive to compute and could be replaced by those of positions which are very close to a leaf node of the tree), but in our case it still worked decently.
Commonly, the size of the hash table is chosen to be a power of two, since the modulo operation to map a hash value to an table index turns out to be simply a bitwise AND:

```c
  int hashIndex = ((int) hash & HASHMASK); // = hash % pow(2,n)
```

where `HASHMASK` is the size of the table minus one ($$2^n - 1$$). One last detail has to be mentioned: How do we define the hash function? Typically, for many board games the so called Zobrist keys are used. These are a clever way to encode a position by XOR-ing (exclusive or) random integers. For each possible move, one random number is generated initially and then used throughout the search. Whenever a move is performed, the current Zobrist key $$Z$$ will be linked by an XOR and the corresponding random number of the move. This approach utilizes the associative and commutative property of the exclusive or as well as the involutory property ($$ x \oplus x = 0 $$ ). In my program I did not use Zobrist keys for reasons of simplicity (but they can also be implemented with relatively small effort). Instead I use a simple hash function of the form

```c
/*
 * Function to compute the hash for a 64bit variable. Maybe Zobrist keys would
 * // work better (has to be investigated in future).
 ****/
uint64_t getHash(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}
```

which is based on [this interesting blog post by David Stafford](http://zimbry.blogspot.de/2011/09/better-bit-mixing-improving-on.html).

#### Finally, the Solution of the Diamond-41 Peg Solitaire Board...

After we put all our components for the solver together, we can finally start running it (the whole source code is listed below). Initially, I ran the solver with a modified termination condition: solutions with $$n$$ (e.g. $$n=5$$) left-over pegs in the final state were also accepted. A solver can find such a solution much faster and can be tested in this way. After everything seemed to work as intended, the solver was started for $$n=1$$ (a solution is searched-for where only one peg remains in the end). Surprisingly, the solver found the solution much faster than expected: After about 6 minutes of computation, an optimal move sequence was found. This move sequence is shown in the slideshow below.

<center><iframe class="slideshow-iframe" src="{{ site.url }}/slides/peg-solitaire2.html"
style="width:400px" frameborder="0" scrolling="no" onload="resizeIframe(this)"></iframe></center>
