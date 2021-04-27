#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <sys/time.h>
// avoid using sys/time.h in new code...
#include <time.h>

// Returns the current system time in microseconds 
long long get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000) + tv.tv_usec;

}

using namespace std;

#define BLOCK_SIZE 16
#define BLOCK_SIZE_C BLOCK_SIZE
#define BLOCK_SIZE_R BLOCK_SIZE

#define STR_SIZE	256

/* to use in vectorizable loop */
#define QUADW_SVE_VL 128
#define WORD_SIZE 32
#define SVE_VL 16 

/* maximum power density possible (say 300W for a 10mm x 10mm chip)	*/
#define MAX_PD	(3.0e6)
/* required precision in degrees	*/
#define PRECISION	0.001
#define SPEC_HEAT_SI 1.75e6
#define K_SI 100
/* capacitance fitting factor	*/
#define FACTOR_CHIP	0.5
#define OPEN
//#define NUM_THREAD 4

typedef float FLOAT;

/* chip parameters	*/
const FLOAT t_chip = 0.0005;
const FLOAT chip_height = 0.016;
const FLOAT chip_width = 0.016;

/* ambient temperature, assuming no package at all	*/
const FLOAT amb_temp = 80.0;

int num_omp_threads;



long long second_for=0;
#ifdef TIME
long long single_iteration_time=0, first_for=0;
long long count_first_for=0, count_second_for=0;
#endif

// reset the current Gem5 statistics, i.e., it sets all counters to zero.
#define M5resetstats()                                                \
    asm volatile(                                                     \
        "reset_stats:\n\t"                                            \
        "mov x0, #0; mov x1, #0; .inst 0XFF000110 | (0x40 << 16);" :: \
	: "x0", "x1")

// dump the current Gem5 statistics
#define M5dumpstats()                                                 \
asm volatile(                                                         \
        "dump_stats:\n\t"                                             \
        "mov x0, #0; mov x1, #0; .inst 0XFF000110 | (0x41 << 16);" :: \
        : "x0", "x1")

// dump the current Gem5 statistics, and reset counters to zero
#define M5resetdumpstats()                                            \
    asm volatile(                                                     \
        "reset_and_dump_stats:\n\t"                                   \
        "mov x0, #0; mov x1, #0; .inst 0XFF000110 | (0x42 << 16);" :: \
	: "x0", "x1")

/* Single iteration of the transient solver in the grid model.
 * advances the solution of the discretized difference equations 
 * by one time step
 */
void single_iteration(FLOAT *result, FLOAT *temp, FLOAT *power, int row, int col,
					  FLOAT Cap_1, FLOAT Rx_1, FLOAT Ry_1, FLOAT Rz_1, 
					  FLOAT step)
{
    FLOAT delta;
    int r, c;
    int chunk;
    int num_chunk = row*col / (BLOCK_SIZE_R * BLOCK_SIZE_C); // Number of chunks, each chunk has 16x16 elements
    int chunks_in_row = col/BLOCK_SIZE_C;
    int chunks_in_col = row/BLOCK_SIZE_R;

    #ifdef SVE
    // Row limit to process in the assembly code; only process inner chunks
    int esize = col - 2 * BLOCK_SIZE_C; 

    // Number of elements to load in a register
    // For SVE_VL=8, 32 words are loaded
    int words_reg = SVE_VL * QUADW_SVE_VL / WORD_SIZE; 
    #endif

    for ( chunk = 0; chunk < num_chunk; ++chunk )
    {
        int r_start = BLOCK_SIZE_R*(chunk/chunks_in_col);   // starting row of actual chunk
        int c_start = BLOCK_SIZE_C*(chunk%chunks_in_row);   // starting column of actual chunk
        int r_end = r_start + BLOCK_SIZE_R > row ? row : r_start + BLOCK_SIZE_R;
        int c_end = c_start + BLOCK_SIZE_C > col ? col : c_start + BLOCK_SIZE_C;

        /* Se o chunk tiver nos cantos da matriz, nao podemos fazer o 'for' normal, e´ preciso de fazer caso a caso */
        if ( r_start == 0 || c_start == 0 || r_end == row || c_end == col )
        {
            #ifdef TIME
            long long start_time = get_time();
            count_first_for++;
            #endif

            for ( r = r_start; r < r_start + BLOCK_SIZE_R; ++r ) {
                for ( c = c_start; c < c_start + BLOCK_SIZE_C; ++c ) {
                    /* Corner 1 */
                    if ( (r == 0) && (c == 0) ) {
                        delta = (Cap_1) *
                                        ( 
                                        power[0] +                              // 
                                        (temp[1] - temp[0])   * Rx_1 +          // 
                                        (temp[col] - temp[0]) * Ry_1 +          // 
                                        (amb_temp - temp[0])  * Rz_1            // 
                                        );
                    }	
                    /* Corner 2 */
                    else if ((r == 0) && (c == col-1)) {
                        delta = (Cap_1) *
                                        ( 
                                        power[c] +                              // 
                                        (temp[c-1] - temp[c])   * Rx_1 +        //  
                                        (temp[c+col] - temp[c]) * Ry_1 +        //  
                                        (amb_temp - temp[c])    * Rz_1          //  
                                        );
                    }	
                    /* Corner 3 */
                    else if ((r == row-1) && (c == col-1)) {
                        delta = (Cap_1) *
                                        (
                                        power[r*col+c] + 
                                        (temp[r*col+c-1] - temp[r*col+c])   * Rx_1 + 
                                        (temp[(r-1)*col+c] - temp[r*col+c]) * Ry_1 + 
                                        (amb_temp - temp[r*col+c])          * Rz_1
                                        );					
                    }	
                    /* Corner 4	*/
                    else if ((r == row-1) && (c == 0)) {
                        delta = (Cap_1) *
                                        (
                                        power[r*col] + 
                                        (temp[r*col+1] - temp[r*col])   * Rx_1 + 
                                        (temp[(r-1)*col] - temp[r*col]) * Ry_1 + 
                                        (amb_temp - temp[r*col])        * Rz_1
                                        );
                    }	
                    /* Edge 1 */
                    else if (r == 0) {
                        delta = (Cap_1) *
                                        (
                                        power[c] + 
                                        (temp[c+1] + temp[c-1] - 2.0*temp[c])   * Rx_1 + 
                                        (temp[col+c] - temp[c])                 * Ry_1 + 
                                        (amb_temp - temp[c])                    * Rz_1
                                        );
                    }	
                    /* Edge 2 */
                    else if (c == col-1) {
                        delta = (Cap_1) *
                                        (
                                        power[r*col+c] + 
                                        (temp[(r+1)*col+c] + temp[(r-1)*col+c] - 2.0*temp[r*col+c]) * Ry_1 + 
                                        (temp[r*col+c-1] - temp[r*col+c]) * Rx_1 + 
                                        (amb_temp - temp[r*col+c]) * Rz_1
                                        );
                    }	
                    /* Edge 3 */
                    else if (r == row-1) {
                        delta = (Cap_1) *
                                        (
                                        power[r*col+c] + 
                                        (temp[r*col+c+1] + temp[r*col+c-1] - 2.0*temp[r*col+c]) * Rx_1 + 
                                        (temp[(r-1)*col+c] - temp[r*col+c]) * Ry_1 + 
                                        (amb_temp - temp[r*col+c]) * Rz_1
                                        );
                }	
                    /* Edge 4 */
                    else if (c == 0) {
                        delta = (Cap_1) *
                                        (
                                        power[r*col] + 
                                        (temp[(r+1)*col] + temp[(r-1)*col] - 2.0*temp[r*col]) * Ry_1 + 
                                        (temp[r*col+1] - temp[r*col]) * Rx_1 + 
                                        (amb_temp - temp[r*col]) * Rz_1
                                        );
                    }
                    
                    result[r*col+c] = temp[r*col+c] + delta;
                }
            }

            #ifdef TIME
            long long end_time = get_time();
            first_for += (float) (end_time* 1000*1000-start_time* 1000*1000);
            #endif

            continue;
        }
            

        long long start_time = get_time();
        #ifdef TIME
        count_second_for++;
        #endif

        #ifndef SVE // Scalar

        for ( r = r_start; r < r_start + BLOCK_SIZE_R; ++r ) {
            for ( c = c_start; c < c_start + BLOCK_SIZE_C; ++c ) {
            /* Update Temperatures */
                result[r*col+c] = temp[r*col+c] + 
                    (
                    Cap_1 * (
                        power[r*col+c] + 
                        (temp[(r+1)*col+c] + temp[(r-1)*col+c] - 2.f*temp[r*col+c]) * Ry_1 + 
                        (temp[r*col+c+1] + temp[r*col+c-1] - 2.f*temp[r*col+c]) * Rx_1 + 
                        (amb_temp - temp[r*col+c]) * Rz_1
                        )
                    );
            }
        }

        #else // SVE
        // WITH UNROLLING FACTOR 4

        for ( r = r_start; r < r_start + BLOCK_SIZE_R; r+=4 ) {
            // vectorizing only column loop
            // pointers for elements in vectors temp, power and result
            float* temp_actual      = &temp[    r * col + c_start];
            float* temp_down        = &temp[(r+1) * col + c_start];
            float* temp_up          = &temp[(r-1) * col + c_start];
            float* temp_left        = &temp[    r * col + c_start - 1];
            float* temp_right       = &temp[    r * col + c_start + 1];
            float* power_actual     = &power[   r * col + c_start];
            float* result_actual    = &result[  r * col + c_start];
	    	    
            asm volatile(
                "mov x4, #0                                         \n\t"   // iterator
                "mov x5, %[col]                                     \n\t"   // iterator + col
                "mov x6, %[col2]                                    \n\t"   // iterator + col * 2
                "mov x7, %[col3]                                    \n\t"   // iterator + col * 3

		        "whilelt p0.s, x4, %[words_reg]                         \n\t"   // limit for words processing
                "ld1rw z0.s, p0/z, %[Rx_1]                          \n\t"
                "ld1rw z1.s, p0/z, %[Ry_1]                          \n\t"
                "ld1rw z2.s, p0/z, %[Rz_1]                          \n\t"
                "ld1rw z3.s, p0/z, %[amb_temp]                      \n\t"
                "ld1rw z4.s, p0/z, %[cap_1]                         \n\t"

		        "whilelt p1.s, x4, %[size]                              \n\t"
                ".loop_column:                                      \n\t"
                    "ld1w z5.s,  p0/z, [%[temp], x4, lsl #2]        \n\t"   // temp[r*col+c] or temp
                    "ld1w z10.s,  p0/z, [%[temp], x5, lsl #2]       \n\t"
                    "ld1w z15.s,  p0/z, [%[temp], x6, lsl #2]       \n\t"
                    "ld1w z20.s,  p0/z, [%[temp], x7, lsl #2]       \n\t"
                    
                    // (amb_temp - temp) * Rz //
                    "mov z6.d, z3.d                                 \n\t"   // load amb_temp to register for operation
                    "mov z11.d, z3.d                                \n\t"
                    "mov z16.d, z3.d                                \n\t"
                    "mov z21.d, z3.d                                \n\t"

                    "fsub z6.s, p0/m, z6.s, z5.s                    \n\t"   // amb_temp - temp
                    "fsub z11.s, p0/m, z11.s, z10.s                 \n\t"
                    "fsub z16.s, p0/m, z16.s, z15.s                 \n\t"
                    "fsub z21.s, p0/m, z21.s, z20.s                 \n\t"

                    "fmul z6.s, p0/m, z6.s, z2.s                    \n\t"   // (amb_temp - temp) * Rz
                    "fmul z11.s, p0/m, z11.s, z2.s                  \n\t"
                    "fmul z16.s, p0/m, z16.s, z2.s                  \n\t"
                    "fmul z21.s, p0/m, z21.s, z2.s                  \n\t"

                    // (temp_down + temp_up - 2temp) * Ry // 
                    "ld1w z7.s, p0/z, [%[temp_up], x4, lsl #2]      \n\t"   // temp[(r-1)*col+c] or temp_up
                    "ld1w z8.s, p0/z, [%[temp_down], x4, lsl #2]    \n\t"   // temp[(r+1)*col+c] or temp_down
                    "ld1w z12.s, p0/z, [%[temp_up], x5, lsl #2]     \n\t"
                    "ld1w z13.s, p0/z, [%[temp_down], x5, lsl #2]   \n\t"
                    "ld1w z17.s, p0/z, [%[temp_up], x6, lsl #2]     \n\t"
                    "ld1w z18.s, p0/z, [%[temp_down], x6, lsl #2]   \n\t"
                    "ld1w z22.s, p0/z, [%[temp_up], x7, lsl #2]     \n\t"
                    "ld1w z23.s, p0/z, [%[temp_down], x7, lsl #2]   \n\t"

                    "fadd z8.s, p0/m, z8.s, z7.s                    \n\t"   // temp_up + temp_down
                    "fadd z13.s, p0/m, z13.s, z12.s                 \n\t"
                    "fadd z18.s, p0/m, z18.s, z17.s                 \n\t"
                    "fadd z23.s, p0/m, z23.s, z22.s                 \n\t"

                    "mov z7.d, z5.d                                 \n\t"   // load temp to a register
                    "mov z12.d, z10.d                               \n\t"
                    "mov z17.d, z15.d                               \n\t"
                    "mov z22.d, z20.d                               \n\t"

                    "fadd z7.s, p0/m, z7.s, z7.s                    \n\t"   // 2temp
                    "fadd z12.s, p0/m, z12.s, z12.s                 \n\t"
                    "fadd z17.s, p0/m, z17.s, z17.s                 \n\t"
                    "fadd z22.s, p0/m, z22.s, z22.s                 \n\t" 

                    "fsub z8.s, p0/m, z8.s, z7.s                    \n\t"   // (temp_up+temp_down) - 2temp
                    "fsub z13.s, p0/m, z13.s, z12.s                 \n\t"
                    "fsub z18.s, p0/m, z18.s, z17.s                 \n\t"
                    "fsub z23.s, p0/m, z23.s, z22.s                 \n\t"

                    "fmul z8.s, p0/m, z8.s, z2.s                    \n\t"   // (temp_down+temp_up-2temp) * Ry
                    "fmul z13.s, p0/m, z13.s, z2.s                  \n\t"
                    "fmul z18.s, p0/m, z18.s, z12.s                 \n\t"
                    "fmul z23.s, p0/m, z23.s, z2.s                  \n\t"

                    "fadd z6.s, p0/m, z6.s, z8.s                    \n\t"   // [(...) * Rz] + [(...) * Ry]
                    "fadd z11.s, p0/m, z11.s, z13.s                 \n\t"
                    "fadd z16.s, p0/m, z16.s, z18.s                 \n\t"
                    "fadd z21.s, p0/m, z21.s, z23.s                 \n\t"

                    // (temp_right + temp_left - 2temp) * Rx //
                    "ld1w z8.s, p0/z, [%[temp_left], x4, lsl #2]    \n\t"   // temp[r*col+c-1] or temp_left
                    "ld1w z9.s, p0/z, [%[temp_right], x4, lsl #2]   \n\t"   // temp[r*col+c+1] or temp_right
                    "ld1w z13.s, p0/z, [%[temp_left], x5, lsl #2]   \n\t"
                    "ld1w z14.s, p0/z, [%[temp_right], x5, lsl #2]  \n\t"
                    "ld1w z18.s, p0/z, [%[temp_left], x6, lsl #2]   \n\t"
                    "ld1w z19.s, p0/z, [%[temp_right], x6, lsl #2]  \n\t"
                    "ld1w z23.s, p0/z, [%[temp_left], x7, lsl #2]   \n\t"
                    "ld1w z24.s, p0/z, [%[temp_right], x7, lsl #2]  \n\t"
                    
                    "fadd z9.s, p0/m, z9.s, z8.s                    \n\t"   // temp_left + temp_right
                    "fadd z14.s, p0/m, z14.s, z13.s                 \n\t"
                    "fadd z19.s, p0/m, z19.s, z18.s                 \n\t"
                    "fadd z24.s, p0/m, z24.s, z23.s                 \n\t" 
                    
                    "fsub z9.s, p0/m, z9.s, z7.s                    \n\t"   // [temp_left+temp_right] - (2temp)
                    "fsub z14.s, p0/m, z14.s, z12.s                 \n\t"
                    "fsub z19.s, p0/m, z19.s, z17.s                 \n\t"
                    "fsub z24.s, p0/m, z24.s, z22.s                 \n\t"

                    "fmul z9.s, p0/m, z9.s, z0.s                    \n\t"   // (temp_right+temp_left-2temp) * Rx
                    "fmul z14.s, p0/m, z14.s, z0.s                  \n\t"
                    "fmul z19.s, p0/m, z19.s, z0.s                  \n\t"
                    "fmul z24.s, p0/m, z24.s, z0.s                  \n\t"

                    "fadd z6.s, p0/m, z6.s, z9.s                    \n\t"   // (...) * Rx + (...) * Rz + (...) * Ry
                    "fadd z11.s, p0/m, z11.s, z14.s                 \n\t"
                    "fadd z16.s, p0/m, z16.s, z19.s                 \n\t"
                    "fadd z21.s, p0/m, z21.s, z24.s                 \n\t"

                    "ld1w z7.s, p0/z, [%[power], x4, lsl #2]        \n\t"   // power[r*col+c-1]
                    "ld1w z12.s, p0/z, [%[power], x5, lsl #2]       \n\t"
                    "ld1w z17.s, p0/z, [%[power], x6, lsl #2]       \n\t"
                    "ld1w z22.s, p0/z, [%[power], x7, lsl #2]       \n\t" 

                    "fadd z6.s, p0/m, z6.s, z7.s                    \n\t"   // [ power + (...) ]
                    "fadd z11.s, p0/m, z11.s, z12.s                 \n\t"
                    "fadd z16.s, p0/m, z16.s, z17.s                 \n\t"
                    "fadd z21.s, p0/m, z21.s, z22.s                 \n\t"

                    // temp + Cap_1 * [...]
                    "fmul z6.s, p0/m, z6.s, z4.s                    \n\t"
                    "fmul z11.s, p0/m, z11.s, z4.s                  \n\t"
                    "fmul z16.s, p0/m, z16.s, z4.s                  \n\t"
                    "fmul z21.s, p0/m, z21.s, z4.s                  \n\t"

                    "fadd z6.s, p0/m, z6.s, z5.s                    \n\t"
                    "fadd z11.s, p0/m, z11.s, z5.s                  \n\t"
                    "fadd z16.s, p0/m, z16.s, z5.s                  \n\t"
                    "fadd z21.s, p0/m, z21.s, z5.s                  \n\t"

                    "st1w z6.s, p0, [%[result], x4, lsl #2]         \n\t"   // stores final result
                    "st1w z11.s, p0, [%[result], x5, lsl #2]        \n\t"
                    "st1w z16.s, p0, [%[result], x6, lsl #2]        \n\t"
                    "st1w z21.s, p0, [%[result], x7, lsl #2]        \n\t"
                    // Increment x4 and x5 with the number of single words that fit in a vector register
                    // x4 += nx128-bits / 32 bits where 'n' will be 8
                    "incw x4                                        \n\t"
                    "incw x5                                        \n\t"
                    "incw x6                                        \n\t"
                    "incw x7                                        \n\t"
		        // loops until the row of all inner chunks is processed
                "whilelt p1.s, x4, %[size]                          \n\t"
                "b.first .loop_column                               \n\t"
                :   [result]        "+r" (result_actual)
                :   [temp]          "r"  (temp_actual), 
                    [temp_down]     "r"  (temp_down), 
                    [temp_up]       "r"  (temp_up), 
                    [temp_left]     "r"  (temp_left), 
                    [temp_right]    "r"  (temp_right), 
                    [power]         "r"  (power_actual),
                    [size]          "r"  (esize),
		            [words_reg]     "r"  (words_reg) 
                    , 
                    [Rx_1]      "m" (Rx_1), 
                    [Ry_1]      "m" (Ry_1), 
                    [Rz_1]      "m" (Rz_1),
                    [amb_temp]  "m" (amb_temp), 
                    [cap_1]     "m" (Cap_1),
                    [col] "r" (col), [col2] "r" (col*2), [col3] "r" (col*3)
                :   "x4", 
                    "memory", 
                    "p0", 
                    "z0", "z1", "z2", "z3", "z4", 
                    "z5", "z6", "z7", "z8", "z9"
                    ,
                    "x5",
                    "z10", "z11", "z12", "z13", "z14",
                    "x6",
                    "z15", "z16", "z17", "z18", "z19",
                    "x7",
                    "z20", "z21", "z22", "z23", "z24"
            );
        }
        chunk += (col / BLOCK_SIZE_C) - 3; // increment a row of inner chunks
        #endif // SVE

        long long end_time = get_time();
        second_for += (end_time* 1000000-start_time* 1000000);
    }
}

/* Transient solver driver routine: simply converts the heat 
 * transfer differential equations to difference equations 
 * and solves the difference equations by iterating
 */
void compute_tran_temp(FLOAT *result, int num_iterations, FLOAT *temp, FLOAT *power, int row, int col) 
{
	#ifdef VERBOSE
	int i = 0;
	#endif

	FLOAT grid_height = chip_height / row;
	FLOAT grid_width = chip_width / col;

	FLOAT Cap = FACTOR_CHIP * SPEC_HEAT_SI * t_chip * grid_width * grid_height;
	FLOAT Rx = grid_width / (2.0 * K_SI * t_chip * grid_height);
	FLOAT Ry = grid_height / (2.0 * K_SI * t_chip * grid_width);
	FLOAT Rz = t_chip / (K_SI * grid_height * grid_width);

	FLOAT max_slope = MAX_PD / (FACTOR_CHIP * t_chip * SPEC_HEAT_SI);
    FLOAT step = PRECISION / max_slope / 1000.0;

    FLOAT Rx_1=1.f/Rx;
    FLOAT Ry_1=1.f/Ry;
    FLOAT Rz_1=1.f/Rz;
    FLOAT Cap_1 = step/Cap;
    
	#ifdef VERBOSE
	fprintf(stdout, "total iterations: %d s\tstep size: %g s\n", num_iterations, step);
	fprintf(stdout, "Rx: %g\tRy: %g\tRz: %g\tCap: %g\n", Rx, Ry, Rz, Cap);
	#endif

    int array_size = row*col;
    {
        FLOAT* r = result;
        FLOAT* t = temp;
        long long max=0;

        for (int i = 0; i < num_iterations ; i++)
        {
            #ifdef VERBOSE
            fprintf(stdout, "iteration %d\n", i++);
            #endif

            #ifdef TIME
            long long start_time = get_time();
            #endif

            #ifdef SVE
            M5resetstats();
            #endif
            //for(int j=0; j<1000000; j++) printf("%d",(int)r[j%64] ^(int)t[j%64]);
            single_iteration(r, t, power, row, col, Cap_1, Rx_1, Ry_1, Rz_1, step);
            #ifdef SVE
            M5resetdumpstats();
            #endif

            #ifdef TIME
            long long end_time = get_time();
            single_iteration_time += end_time-start_time;
            #endif

            FLOAT* tmp = t;
            t = r;
            r = tmp;
        }

    }
	#ifdef VERBOSE
	fprintf(stdout, "iteration %d\n", i++);
	#endif
}

void fatal(const char *s)
{
	fprintf(stderr, "error: %s\n", s);
	exit(1);
}

void writeoutput(FLOAT *vect, int grid_rows, int grid_cols, char *file) {

    int i,j, index=0;
    FILE *fp;
    char str[STR_SIZE];

    if( (fp = fopen(file, "w" )) == 0 )
        printf( "The file was not opened\n" );


    for (i=0; i < grid_rows; i++) 
        for (j=0; j < grid_cols; j++)
        {
            
            sprintf(str, "%d\t%g\n", index, vect[i*grid_cols+j]);
            fputs(str,fp);
            index++;
        }

    fclose(fp);	
}

void read_input(FLOAT *vect, int grid_rows, int grid_cols, char *file)
{
  	int i, index;
	FILE *fp;
	char str[STR_SIZE];
	FLOAT val;

	fp = fopen (file, "r");
	if (!fp)
		fatal("file could not be opened for reading");

	for (i=0; i < grid_rows * grid_cols; i++) {
	    if(fgets(str, STR_SIZE, fp)!=NULL) {
		if (feof(fp))
			fatal("not enough lines in file");
		if ((sscanf(str, "%f", &val) != 1) )
			fatal("invalid file format");
		vect[i] = val;
	    }
	    else fatal("Not enough lines in file");
	}

	fclose(fp);	
}

void usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s <grid_rows> <grid_cols> <sim_time> <no. of threads><temp_file> <power_file>\n", argv[0]);
	fprintf(stderr, "\t<grid_rows>  - number of rows in the grid (positive integer)\n");
	fprintf(stderr, "\t<grid_cols>  - number of columns in the grid (positive integer)\n");
	fprintf(stderr, "\t<sim_time>   - number of iterations\n");
	fprintf(stderr, "\t<no. of threads>   - number of threads\n");
	fprintf(stderr, "\t<temp_file>  - name of the file containing the initial temperature values of each cell\n");
	fprintf(stderr, "\t<power_file> - name of the file containing the dissipated power values of each cell\n");
    fprintf(stderr, "\t<output_file> - name of the output file\n");
	exit(1);
}

int main(int argc, char **argv)
{

	int grid_rows, grid_cols, sim_time, i;
	FLOAT *temp, *power, *result;
	char *tfile, *pfile, *ofile;
	
	/* check validity of inputs	*/
	if (argc != 8)
		usage(argc, argv);
	if ((grid_rows       = atoi(argv[1])) <= 0 ||
		(grid_cols       = atoi(argv[2])) <= 0 ||
		(sim_time        = atoi(argv[3])) <= 0 || 
		(num_omp_threads = atoi(argv[4])) <= 0
		)
		usage(argc, argv);

	/* allocate memory for the temperature and power arrays	*/
	temp   = (FLOAT *) calloc (grid_rows * grid_cols, sizeof(FLOAT));
	power  = (FLOAT *) calloc (grid_rows * grid_cols, sizeof(FLOAT));
	result = (FLOAT *) calloc (grid_rows * grid_cols, sizeof(FLOAT));
	if(!temp || !power || !result)
		fatal("unable to allocate memory");

	/* read initial temperatures and input power	*/
	tfile = argv[5];
	pfile = argv[6];
	ofile = argv[7];

	read_input(temp,  grid_rows, grid_cols, tfile);
	read_input(power, grid_rows, grid_cols, pfile);

	printf("\nStart computing the transient temperature...\n");

    long long start_time = get_time();

    compute_tran_temp(result,sim_time, temp, power, grid_rows, grid_cols);

    long long end_time = get_time();

    printf("Ending simulation.\n");
    printf("\nVectorizable loop time: ");
    printf("%.2f us.\n", (float) second_for/ (1000*1000) );
    printf("Transient temperature computing time: ");
    printf("%.2f us.\n\n", ((float) (end_time - start_time)) );


    #ifdef OUTPUT
    writeoutput((1&sim_time) ? result : temp, grid_rows, grid_cols, ofile);
    #endif

	/* output results	*/
    #ifdef VERBOSE
	fprintf(stdout, "Final Temperatures:\n");
	for(i=0; i < grid_rows * grid_cols; i++)
	fprintf(stdout, "%d\t%g\n", i, temp[i]);
    #endif

	/* cleanup	*/
	free(temp);
	free(power);
	free(result);

    #ifdef TIME
    printf("avg 'single_iteration' time: %.2f us.\n", 
            ((float) (single_iteration_time/sim_time)) );
    printf("avg 'single_iteration' edge/corner chunks time: %.2f us in %d chunks.\n", 
            ((float) (first_for/count_first_for)) / (1000*1000), count_first_for);
    printf("avg 'single_iteration' normal chunks time: \t%.2f us in %d chunks.\n", 
            ((float) (second_for/count_second_for)) / (1000*1000), count_second_for);

    printf("\n");
    printf("sum of all edge/corner chunks:  %.2f us\n", (float) first_for/ (1000*1000));
    printf("sum of all normal chunks: \t%.2f us\n", (float) second_for/ (1000*1000));
    #endif

	return 0;
}

