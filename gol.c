/*
 * CONWAYS GAME OF LIFE
 * --------------------
 *
 * Author: floppyist
 * Date: 03.05.2024  
 *
 * 1 IDEA:
 * ------
 *
 * 0 - Dead Cell
 * 1 - Living Cell
 *
 * | 0 | 0 | 0 |
 * | 1 | 1 | 1 |
 * | 1 | 0 | 0 |
 *
 * If a observed cell (above the one in the middle) have one the following constellations:
 * Neighbours < 2      => Cell [dies] in next gen
 * Neighbours = 2 or 3 => Cell [lives] in next gen
 * Neighbours > 3      => Cell [dies] in next gen
 * Neighbours = 3      => Cell [created] in next gen
 */

#include <asm-generic/ioctls.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

// Boolean to check if ctrl-c was pressed
bool running = true;

/* 
 * This struct is already defined by the C library. It contains four datatypes
 * to store the data coming from ioctl.
 *
 * ws_col: Terminal width in cols 
 * ws_row: Terminal height in rows 
 *
 * ws_xpixel: Terminal width in pixels
 * ws_ypixel: Terminal height in pixels 
 */
struct winsize size;

struct coords {
    int x;
    int y;
} coords;

/* This history is needed to detect repetitions. */
unsigned long *hash_history = NULL;

/* A history of at least 45 should cover every spinning or looping cell-cluster. */
/* Highest loop: Pentadecathlon: 15 */
/* Before: Pulsar: 3 */
/* Calculation: 15 * 3 = 45 (worst case?) */
unsigned int hash_history_size = 45;
unsigned int hash_history_pointer = 0;

unsigned long getHash( char *arr ) {
    long h = 5381;

    int arr_size = coords.x * coords.y;

    while ( arr_size-- ) {
        h += ( h << 5 );
        h ^= *arr++;
    }

    return h;
}

/*
 * Returns calculated index value (1D-Array) from given coordinates.
 */
int getIndexFromCoords( int *x, int *y ) {
    return ( *x + ( *y * size.ws_col ) );
}

/*
 * Returns calculated coordinates (2D-Array) from given index.
 */
struct coords getCoordsFromIndex( int *index ) {
    coords.x = *index % size.ws_col;
    coords.y = ( *index - coords.x ) / size.ws_col;

    return coords;
}

/*
 * Fills up array randomly with living cells and adds a border to make neighbour checks far
 * more easy.
 */
char *fillArray( char *arr ) {
    srand( time( NULL ) );

    for ( int i = 0; i < ( size.ws_col * size.ws_row ); i++ ) {
        coords = getCoordsFromIndex( &i ); 
        int x = coords.x;
        int y = coords.y;

        if ( y == 0 || y == ( size.ws_row - 1 ) ) {
            arr[i] = '.';
            continue;
        }

        if ( x == 0 || x == ( size.ws_col - 1 ) ) {
            arr[i] = '.';
            continue;
        }

        if ( ( rand() % 13 ) < 7 ) {
            arr[i] = 'o'; 
        } else {
            arr[i] = ' ';
        }
    }

    return arr;
}

/*
 * Simple array printer which adds whitespaces and newlines to make the field almost cubic.
 */
void printArray( char *arr ) {
    for ( int i = 0; i < ( size.ws_col * size.ws_row ); i++ ) {
        coords = getCoordsFromIndex( &i ); 
        int x = coords.x;
        int y = coords.y;

        if ( x % size.ws_col == 0 ) {
            printf( "\n  " );
        }

        printf( "%c ", arr[i] );
    }

    printf( "\n ");
}

/*
 * Calculates all cell neighbours and returns the amount. If there is hit an border value,
 * then return -1 to make it catchable.
 *
 * The idea was to check a 3x3 submatrix with the following two for-loops instead of hardcoded
 * neighbour coordinates.
 */
int getCellNeighbours( char *arr, int *index ) {
    int neighbour_count = 0;

    coords = getCoordsFromIndex( index );
    int x = coords.x;
    int y = coords.y;

    /* check if array index points to a border cell */
    if ( arr[ *index ] == '.' ) { 
        return -1;
    }

    /* check 3x3 submatrix */
    for ( int i_x = -1; i_x < 2; i_x++ ) {
        for ( int i_y = -1; i_y < 2; i_y++ ) {

            /* these two variables are describing the currently pointed value in the 3x3 submatrix */
            int temp_x = x + i_x;
            int temp_y = y + i_y;

            /* return to next value, if checking cell is pointed */
            if ( temp_x == x && temp_y == y ) {
                continue;
            }

            if ( arr[ getIndexFromCoords( &temp_x, &temp_y ) ] == 'o' ) {
                neighbour_count += 1;
            }
        }
    }

    return neighbour_count;
}

/*
 * Creates a new array with the next generation and returns it.
 */
char *calculateNextGen( char* arr ) {
    char *nextGen = NULL;
    nextGen = malloc( sizeof( nextGen ) * size.ws_row * size.ws_col );

    if ( nextGen == NULL ) {
        perror( "  Unable to allocate array memory. " );
        exit( 1 );
    }

    for ( int i = 0; i < ( size.ws_row * size.ws_col ); i++ ) {
        if ( arr[i] == '.' ) {
            nextGen[i] = '.';
            continue;
        }

        int neighbours = getCellNeighbours( arr, &i );

        if ( arr[i] == 'o' ) {
            /* 
             * Neighbours = 2 or 3 => Cell [lives] in next gen 
             * For 3 its always a living cell which is queried down below.
             */
            if ( neighbours == 2 ) {
                nextGen[i] = 'o';
                continue;
            }
        }

        /* Neighbours < 2 => Cell [dies] in next gen */
        if ( neighbours < 2 ) {
            nextGen[i] = ' ';
            continue;
        }

        /* Neighbours = 3 => Cell [created] in next gen */
        if ( neighbours == 3 ) {
            nextGen[i] = 'o';
            continue;
        }

        nextGen[i] = ' ';
    }

    return nextGen;
}

/*
 * Simple handler for ctrl-c combination to beautify terminal.
 */
void runHandler() {
    running = false;
}

/*
 * Check for repetitive behaviour in the calculated generation-maps. 
 */
bool isNonRepetitive( char *arr ) {
    unsigned long curr_hash = getHash( arr );

    for ( int i = 0; i < hash_history_size; i++ ) {
        if ( curr_hash == hash_history[i] ) {
            return false;
        } 
    }

    hash_history[ hash_history_pointer ] = curr_hash;
    
    if ( hash_history_pointer < hash_history_size - 1 ) {
        hash_history_pointer += 1;
    } else {
        hash_history_pointer = 0;
    }

    return true;
}

void printHelp() {
    printf( "\nConways Game of Life\n" );

    printf( "\nUsage:" );
    printf( "\n ./gol [options]\n" );

    printf( "\nExample:" );
    printf( "\n ./gol -h" );

    printf( "\nOptions:" );
    printf( "\n -h, --help shows this help dialog" );

    printf( "\n\n" );
}

int main( int argc, char **argv ) {
    // Setup draws per second
    int fps = 60;

    // Disable stats/gui
    bool gui = false;

    // Stores the highest living gen
    int last_gen = 0;
    int highest_gen = 0;

    /* First clearing whole terminal.. */
    system( "clear" );

    /* ..create the main array for the cell universe.. */
    char *arr = NULL;

    /* ..fetch terminal height and width.. */
    /* Filedescriptor, Command, Storage */
    ioctl( 0, TIOCGWINSZ, &size );

    /* ..setup basic size.. */
    /* +2 adds borders to the matrix to simplfy neighbour checks later */
    if ( gui ) {
        size.ws_row = size.ws_row - 9 - hash_history_size;
    } else {
        size.ws_row = size.ws_row - 2;
    }

    /* division by 2 is possible, because the int cuts the decimals */
    size.ws_col = ( size.ws_col - 4 ) / 2;

    /* ..reserve memory.. */
    arr = malloc( size.ws_col * size.ws_row * sizeof( arr ) );
    hash_history = malloc( hash_history_size * sizeof( arr ) );

    /* ..and check if it was successully allocated. */
    if ( arr == NULL || hash_history == NULL ) { 
        perror( "  Unable to allocate array memory. " );
        exit( 1 );
    } 

    /* register handler for ctrl-c command */
    signal( SIGINT, runHandler );

    /* simple generation counter */
    int generation = 0;

    /* endless loop until ctrl-c was pressed */
    while ( running ) {
        if ( generation == 0 ) {
            arr = fillArray( arr );
        }

        if ( gui ) {
            printf( "\n  Cols : %i", size.ws_col );
            printf( "\n  Rows : %i", size.ws_row );
            printf( "\n  Gen  : %i (max: %i)", generation, highest_gen );
            printf( "\n  FPS  : %i", fps );
            printf( "\n\n  Hash-History:" );

            for ( int i = 0; i < hash_history_size; i++ ) {
                if ( i == hash_history_pointer ) {
                    printf( "\x1B[34m" );
                }

                printf( "\n  [%i]:\t%li\x1B[0m", i, hash_history[i] );
            }

            printf( "\n" );
        }

        arr = calculateNextGen( arr );
        printArray( arr );

        /* Predefine latest generation (amount of iterations) */
        last_gen = generation;

        generation += 1;
        
        /* Checks if the current matrix is not repetetive. */
        if ( !isNonRepetitive( arr ) ) {
            hash_history = malloc( hash_history_size * sizeof( hash_history ) );

            /* Stores the highest amount of iterations per seed. */
            if ( last_gen > highest_gen ) {
                highest_gen = last_gen;
            }

            /* Set generation back to 0 if a repetetive seed was found. */
            generation = 0;
        }

        /* Prevent "jumpy" drawing if fps is too high. Set to your needs/hardware. */
        usleep( 1000000 / fps );
    }
    
    free( arr );
    free( hash_history );

    printf( "\n" );
    system( "clear" );
}

