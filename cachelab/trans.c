/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    REQUIRES(M > 0);
    REQUIRES(N > 0);

    /**
     * 8 variables that hold several cached integers to prevent cache MISS
     * that is caused by previous EVICTION.
     */
    int buf0, buf1, buf2, buf3, buf4, buf5, buf6, buf7;

    if ((32 == N) && (32 == M)) {
        /**
         * For 32 * 32 matrix. 
         *
         * Since cache block size is 32 bytes, each cache line will hold 8 int
         * type integers. We can divide the matrix into 8 * 8 blocks. The cache
         * can hold the entire block.
         *
         * However, because the set indexes of diagonal blocks of A and B are
         * the same, there exist some conflicts. To solve this, we can use local
         * variables to temporarily buffer a cache line, in which one line of
         * the 8 * 8 block of A are stored. In this way, we can quickly access
         * this line even after it got evicted.
         */
        for (int col_start = 0; col_start < M; col_start += 8) {
            for (int row_start = 0; row_start < N; row_start += 8) {       
                // For each 8 * 8 block.
                for (int i = row_start; i < row_start + 8; ++i) {
                    // Buffer a line of the 8 * 8 block of A.
                    buf0 = A[i][col_start];
                    buf1 = A[i][col_start + 1];
                    buf2 = A[i][col_start + 2];
                    buf3 = A[i][col_start + 3];
                    buf4 = A[i][col_start + 4];
                    buf5 = A[i][col_start + 5];
                    buf6 = A[i][col_start + 6];
                    buf7 = A[i][col_start + 7];
                    /**
                     * Here, the buffered line is also in cache. But when we
                     * transpose diagonal blocks, this line will be evicted
                     * after the following data accessing due to set index
                     * conflicts.
                     */
                    B[col_start][i] = buf0;
                    B[col_start + 1][i] = buf1;
                    B[col_start + 2][i] = buf2;
                    B[col_start + 3][i] = buf3;
                    B[col_start + 4][i] = buf4;
                    B[col_start + 5][i] = buf5;
                    B[col_start + 6][i] = buf6;
                    B[col_start + 7][i] = buf7;
                    /**
                     * Now all 8 lines of the 8 * 8 block of B are stored in
                     * cache. If we are transposing diagonal blocks, one of
                     * these 8 lines will be evicted in the next loop because A
                     * and B have conflicts.
                     */
                }
            }
        }
    } else if ((64 == N) && (64 == M)) {
        /**
         * For 64 * 64 matrix.
         *
         * This one is tricky. Because the width and height are doubled, we will
         * have conflicts if we want to cache a 8 * 8 block. The largest block
         * without conflicts are 4 * 4 block. But the result is unacceptable
         * (about 1700 misses) because the cache is not fully utilized.
         *
         * To fully utilize the cache, we can use a 8 * 8 block with 4 * 4
         * sub-blocks in it. When transposing a 4 * 4 sub-block, only half of
         * the cache line is used, so the other half can be used to buffer the
         * result of another 4 * 4 block.
         *
         * But for diagonal blocks, conflicts are still too many. Using cache as
         * a buffer cannot decrease miss numbers. Instead, we can use two 4 * 4
         * non-diagonal sub-blocks in B to buffer the transposing result. Notice
         * that we will always use the same two sub-blocks, so the cost of
         * loading is negligible.
         *
         * See details in the following code and comments.
         */
        // Transpose diagonal blocks first.
        for (int col_start = 0; col_start < 64; col_start += 8) {
            // For each 8 * 8 block.
            int row_start = col_start;
            /**
             * - If we are transposing diagonal block A[8~15][8~15], we use
             *   B[0~4][16~23] as a buffer.
             * - Otherwise, we use B[0~4][8~15] as a buffer.
             *
             * So the starting column of the buffer block will either be 8 
             * or 16.
             */
            int buf_col_start = (col_start != 8)? 8 : 16;
            // Transpose the upper two 4 * 4 blocks.
            for (int i = row_start; i < row_start + 4; ++i) {
                // One row of the upper left sub-block of A.
                buf0 = A[i][col_start];
                buf1 = A[i][col_start + 1];
                buf2 = A[i][col_start + 2];
                buf3 = A[i][col_start + 3];
                // One row of the upper right sub-block of A.
                buf4 = A[i][col_start + 4];
                buf5 = A[i][col_start + 5];
                buf6 = A[i][col_start + 6];
                buf7 = A[i][col_start + 7];
                // Transpose the upper left sub-block of A.
                B[col_start][i] = buf0;
                B[col_start + 1][i] = buf1;
                B[col_start + 2][i] = buf2;
                B[col_start + 3][i] = buf3;
                // Transpose the upper right sub-block of A, stored in buffer.
                B[0][buf_col_start + i - row_start] = buf4;
                B[1][buf_col_start + i - row_start] = buf5;
                B[2][buf_col_start + i - row_start] = buf6;
                B[3][buf_col_start + i - row_start] = buf7;
            }
            // Transpose the lower two 4 * 4 blocks.
            for (int i = row_start + 4; i < row_start + 8; ++i) {
                // One row of the lower left sub-block of A.
                buf0 = A[i][col_start];
                buf1 = A[i][col_start + 1];
                buf2 = A[i][col_start + 2];
                buf3 = A[i][col_start + 3];
                // One row of the lower right sub-block of A.
                buf4 = A[i][col_start + 4];
                buf5 = A[i][col_start + 5];
                buf6 = A[i][col_start + 6];
                buf7 = A[i][col_start + 7];
                // Transpose the lower left sub-block of A.
                B[col_start][i] = buf0;
                B[col_start + 1][i] = buf1;
                B[col_start + 2][i] = buf2;
                B[col_start + 3][i] = buf3;
                // Transpose the lower right sub-block of A, stored in buffer.
                B[0][buf_col_start + i - row_start] = buf4;
                B[1][buf_col_start + i - row_start] = buf5;
                B[2][buf_col_start + i - row_start] = buf6;
                B[3][buf_col_start + i - row_start] = buf7;
            }
            // Move buffered blocks back.
            for (int j = row_start; j < row_start + 8; ++j) {
                B[col_start + 4][j] = B[0][buf_col_start + j - row_start];
                B[col_start + 5][j] = B[1][buf_col_start + j - row_start];
                B[col_start + 6][j] = B[2][buf_col_start + j - row_start];
                B[col_start + 7][j] = B[3][buf_col_start + j - row_start];
            }
        }
        // Then we transpose non-diagonal blocks.
        for (int col_start = 0; col_start < 64; col_start += 8) {
            for (int row_start = 0; row_start < 64; row_start += 8) {
                if (row_start == col_start) {
                    continue; // skip diagonal blocks
                }
                // Transpose the upper two 4 * 4 sub-blocks.
                for (int i = row_start; i < row_start + 4; ++i) {
                    // One row of the upper left sub-block of A.
                    buf0 = A[i][col_start];
                    buf1 = A[i][col_start + 1];
                    buf2 = A[i][col_start + 2];
                    buf3 = A[i][col_start + 3];
                    // One row of the upper right sub-block of A.
                    buf4 = A[i][col_start + 4];
                    buf5 = A[i][col_start + 5];
                    buf6 = A[i][col_start + 6];
                    buf7 = A[i][col_start + 7];
                    // Transpose the upper left sub-block of A.
                    B[col_start][i] = buf0;
                    B[col_start + 1][i] = buf1;
                    B[col_start + 2][i] = buf2;
                    B[col_start + 3][i] = buf3;
                    /**
                     * Transpose the upper right sub-block of A,
                     * buffered in cache.
                     */
                    B[col_start][i + 4] = buf4;
                    B[col_start + 1][i + 4] = buf5;
                    B[col_start + 2][i + 4] = buf6;
                    B[col_start + 3][i + 4] = buf7;
                }
                // Transpose the lower left sub-block of A.
                for (int i = col_start; i < col_start + 4; ++i) {
                    /**
                     * Row of the transposed upper right sub-block of A, 
                     * it is buffered in cache, now we take them out.
                     */
                    buf0 = B[i][row_start + 4];
                    buf1 = B[i][row_start + 5];
                    buf2 = B[i][row_start + 6];
                    buf3 = B[i][row_start + 7];
                    // Transpose the lower left sub-block of A.
                    B[i][row_start + 4] = A[row_start + 4][i];
                    B[i][row_start + 5] = A[row_start + 5][i];
                    B[i][row_start + 6] = A[row_start + 6][i];
                    B[i][row_start + 7] = A[row_start + 7][i];
                    // Move buffered blocks to correct position.
                    B[i + 4][row_start] = buf0;
                    B[i + 4][row_start + 1] = buf1;
                    B[i + 4][row_start + 2] = buf2;
                    B[i + 4][row_start + 3] = buf3;
                }
                // Transpose the lower right sub-block of A.
                for (int i = col_start + 4; i < col_start + 8; ++i) {
                    // One row of the lower right sub-block.
                    buf4 = A[row_start + 4][i];
                    buf5 = A[row_start + 5][i];
                    buf6 = A[row_start + 6][i];
                    buf7 = A[row_start + 7][i];
                    // Transpose the lower right sub-block.
                    B[i][row_start + 4] = buf4;
                    B[i][row_start + 5] = buf5;
                    B[i][row_start + 6] = buf6;
                    B[i][row_start + 7] = buf7;
                }
            }
        }
    } else if ((67 == N) && (61 == M)) {
        /**
         * For 61 * 67 matrix.
         *
         * I use the algorithm that was introduced in 32 * 32 matrix for all 
         * 8 * 8 blocks. For those integers on "edge", I use normal transpose
         * algorithm. It passes :).
         */
        for (int col_start = 0; col_start < 56; col_start += 8) {
            for (int row_start = 0; row_start < 64; row_start += 8) {       
                for (int i = row_start; i < row_start + 8; ++i) {
                    buf0 = A[i][col_start];
                    buf1 = A[i][col_start + 1];
                    buf2 = A[i][col_start + 2];
                    buf3 = A[i][col_start + 3];
                    buf4 = A[i][col_start + 4];
                    buf5 = A[i][col_start + 5];
                    buf6 = A[i][col_start + 6];
                    buf7 = A[i][col_start + 7];
                    B[col_start][i] = buf0;
                    B[col_start + 1][i] = buf1;
                    B[col_start + 2][i] = buf2;
                    B[col_start + 3][i] = buf3;
                    B[col_start + 4][i] = buf4;
                    B[col_start + 5][i] = buf5;
                    B[col_start + 6][i] = buf6;
                    B[col_start + 7][i] = buf7;
                }
            }
        }
        // "edge" integers, transpose normally.
        for (int i = 0; i < 56; ++i) {
            B[i][64] = A[64][i];
            B[i][65] = A[65][i];
            B[i][66] = A[66][i];
        }
        for (int i = 0; i < 67; ++i) {
            B[56][i] = A[i][56];
            B[57][i] = A[i][57];
            B[58][i] = A[i][58];
            B[59][i] = A[i][59];
            B[60][i] = A[i][60];
        }
    } else {
        // Other cases.
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < M; ++j) {
                B[j][i] = A[i][j];
            }
        }    
    }

    ENSURES(is_transpose(M, N, A, B));
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

