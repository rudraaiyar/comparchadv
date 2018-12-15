#include <stdlib.h>
#include <emmintrin.h>

//If A=[aij] is an m×n matrix and B=[bij] is an n×p matrix, the product AB is an m×p matrix.
//AB=[cij] , where cij=ai1b1j+ai2b2j+...+ainbnj .
//m is row /colm length

void reorder(int m, int n, float *A, float *C)
{ //naive ikj so opt: jki, kij, ijk
    //for( int i = 0; i < m; i++ )
       // for( int k = 0; k < n; k++ )
            for( int j = 0; j < m; j++ ) //j to m
                for( int k = 0; k < n; k++ ) //k to n
                    for( int i = 0; i < m; i++ ) //i to p=m
                        C[i+j*m] += A[i+k*m] * A[j+k*m]; //same as naive
}
/********************************************************************************/
void unroll(int m, int n, float *A, float *C)
{
    //ikj is based off naive
    //https://en.wikipedia.org/wiki/Loop_unrolling
    //http://wgropp.cs.illinois.edu/courses/cs598-s15/lectures/lecture11.pdf
    /*Do kk=1,n,stride
     do ii=1,n,stride
     do j=1,n-2,2
     do i=ii,min(n,ii+stride-1),2
     do k=kk,min(n,kk+stride-1)
     c(i,j) += a(i,k) * b(k,j)
     c(i+1,j) += a(i+1,k)* b(k,j)
     c(i,j+1) += a(i,k) * b(k,j+1)
     c(i+1,j+1) += a(i+1,k)* b(k,j+1) */
    
    
    for (int i = 0; i < m; i++){
        for (int k = 0; k < n; k++){
            for (int j = 0; j < m; j += 5) { //unroll limit
                int distEnd = m - j; //compare value of unroll distance to end of roll
                 if (distEnd > 4){
                    C[i + j * m] += A[i + k * m] * A[j + k * m];
                    C[i + (j + 1) * m] += A[i + k * m] * A[(j + 1) + k * m]; //unroll 5
                    C[i + (j + 2) * m] += A[i + k * m] * A[(j + 2) + k * m];
                    C[i + (j + 3) * m] += A[i + k * m] * A[(j + 3) + k * m];
                    C[i + (j + 4) * m] += A[i + k * m] * A[(j + 4) + k * m];
                }else if (distEnd == 4 ){
                    C[i + j * m] += A[i + k * m] * A[j + k * m];
                    C[i + (j + 1) * m] += A[i + k * m] * A[(j + 1) + k * m]; //unroll 4
                    C[i + (j + 2) * m] += A[i + k * m] * A[(j + 2) + k * m];
                    C[i + (j + 3) * m] += A[i + k * m] * A[(j + 3) + k * m];
                }else  if (distEnd == 3) {
                    C[i + j * m] += A[i + k * m] * A[j + k * m];
                    C[i + (j + 1) * m] += A[i + k * m] * A[(j + 1) + k * m]; //unroll 3
                    C[i + (j + 2) * m] += A[i + k * m] * A[(j + 2) + k * m];
                }else if (distEnd == 2) {
                    C[i + j * m] += A[i + k * m] * A[j + k * m];
                    C[i + (j + 1) * m] += A[i + k * m] * A[(j + 1) + k * m]; //unroll 2
                }else if (distEnd == 1){
                    C[i + j * m] += A[i + k * m] * A[j + k * m];  //naive
                }
            }
        }
    }
}
/********************************************************************************/
void sse(int m, int n, float *A, float *C) //Streaming SIMD Extensions
{
    //https://www.cs.uaf.edu/2009/fall/cs301/lecture/11_13_sse_intrinsics.html
    //https://felix.abecassis.me/2011/09/cpp-getting-started-with-sse/
    //https://www.cs.uaf.edu/2009/fall/cs301/lecture/11_13_sse_intrinsics.html
    
    //void _mm_store_ps(float *dest,__m128 src) --Store 4 floats to an aligned address.
    //__m128 _mm_add_ps(__m128 a,__m128 b)--Add corresponding floats (also "sub")
    //__m128 _mm_mul_ps(__m128 a,__m128 b)    Multiply corresponding floats (also "div", but it's slow)
    //__m128 _mm_load_ps(float *src) --Load 4 floats from a 16-byte aligned address.
    
    __m128 aline, bline; //load line from a, b
    //"FAILURE: error in matrix multiply exceeds an acceptable margin"
    //Off by: 9.195366, from the reference: 9.195368, at n = 32, m = 32

    if (m != 32 || n != 32) { //check if in bounds *
        for (int k = 0; k < n; k++)
            for (int j = 0; j < m; j++)
                for (int i = 0; i < m; i++)
                    C[i + j * m] += A[i + k * m] * A[j + k * m]; //naive
        return;
    }
    
    for (int k = 0; k < n; k++) {
        for (int j = 0; j < m; j++) {
            for (int i = 0; i < m; i += 4) {
                //C[i + j * m] += A[i + k * m] * A[j + k * m]; //naive WONT GO IN THIS LOOP
                aline = _mm_load_ps(A + i + k * m); //vec4(A + i + k * m)
                bline = _mm_load1_ps(A + j + k * m); //vec4(A + j + k * m)
                __m128 dotprod =_mm_mul_ps(aline, bline); //multiply both
                __m128 loadprod= _mm_load_ps(C + i + j * m); //load 4 floats 16b addr
                __m128 addprod = _mm_add_ps(dotprod,loadprod); // add
                _mm_store_ps((C + i + j * m), addprod);//store addprod in dest val of C...
            }
        }
    }
}
/********************************************************************************/
void dgemm( int m, int n, float *A, float *C )
{
    //uncomment/comment each to run individually
    
    //reorder(m, n, A, C); //run with 3,2
    //unroll(m, n, A, C); //run with 3 but somehow 1 produced a good result? (sometimes doesnt work) or close to naive
    sse(m, n, A, C); //run with 2,3
}
