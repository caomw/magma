/*
    -- clMAGMA (version 2.0.2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date May 2016
  
       @generated from testing/testing_ztrtri_gpu.cpp normal z -> s, Mon May  2 23:31:11 2016
       
       @author Mark Gates
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "flops.h"
#include "magma_v2.h"
#include "magma_lapack.h"
#include "testings.h"

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing strtri
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float *h_A, *h_R;
    magmaFloat_ptr d_A;
    float c_neg_one = MAGMA_S_NEG_ONE;
    magma_int_t N, n2, lda, ldda, info;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    float      work[1], error, norm;
    magma_int_t status = 0;

    magma_opts opts;
    opts.parse_opts( argc, argv );
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    
    float tol = opts.tolerance * lapackf77_slamch("E");
    
    printf("%% uplo = %s\n", lapack_uplo_const(opts.uplo) );
    printf("%%   N   CPU Gflop/s (sec)   GPU Gflop/s (sec)   ||R||_F / ||A||_F\n");
    printf("%%================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            lda    = N;
            n2     = lda*N;
            ldda   = magma_roundup( N, opts.align );  // multiple of 32 by default
            gflops = FLOPS_STRTRI( N ) / 1e9;
            
            TESTING_MALLOC_CPU( h_A, float, n2 );
            TESTING_MALLOC_PIN( h_R, float, n2 );
            TESTING_MALLOC_DEV( d_A, float, ldda*N );
            
            /* Initialize the matrix */
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            magma_smake_hpd( N, h_A, lda );
            lapackf77_slacpy( MagmaFullStr, &N, &N, h_A, &lda, h_R, &lda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            /* factorize matrix */
            lapackf77_spotrf( lapack_uplo_const(opts.uplo), &N, h_A, &lda, &info );
            magma_ssetmatrix( N, N, h_A, lda, d_A, ldda, opts.queue );
            //magma_spotrf_gpu( opts.uplo, N, d_A, ldda, &info );
            
            // check for exact singularity
            //magma_sgetmatrix( N, N, d_A, ldda, h_R, lda, opts.queue );
            //h_R[ 10 + 10*lda ] = MAGMA_S_MAKE( 0.0, 0.0 );
            //magma_ssetmatrix( N, N, h_R, lda, d_A, ldda, opts.queue );
            
            gpu_time = magma_wtime();
            magma_strtri_gpu( opts.uplo, opts.diag, N, d_A, ldda, &info );
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0) {
                printf("magma_strtri_gpu returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            }
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                // done above
                //lapackf77_spotrf( lapack_uplo_const(opts.uplo), &N, h_A, &lda, &info );
                
                cpu_time = magma_wtime();
                lapackf77_strtri( lapack_uplo_const(opts.uplo), lapack_diag_const(opts.diag), &N, h_A, &lda, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0) {
                    printf("lapackf77_strtri returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                }
                
                /* =====================================================================
                   Check the result compared to LAPACK
                   =================================================================== */
                magma_sgetmatrix( N, N, d_A, ldda, h_R, lda, opts.queue );
                if ( opts.verbose ) {
                    printf( "A=" );  magma_sprint( N, N, h_A, lda );
                    printf( "R=" );  magma_sprint( N, N, h_R, lda );
                }
                norm  = lapackf77_slantr("f", lapack_uplo_const(opts.uplo), MagmaNonUnitStr, &N, &N, h_A, &lda, work);
                blasf77_saxpy(&n2, &c_neg_one, h_A, &ione, h_R, &ione);
                error = lapackf77_slantr("f", lapack_uplo_const(opts.uplo), MagmaNonUnitStr, &N, &N, h_R, &lda, work) / norm;
                if ( opts.verbose ) {
                    printf( "diff=" );  magma_sprint( N, N, h_R, lda );
                }
                bool okay = (error < tol);
                status += ! okay;
                printf("%5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
                       (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time,
                       error, (okay ? "ok" : "failed") );
            }
            else {
                printf("%5d     ---   (  ---  )   %7.2f (%7.2f)     ---\n",
                       (int) N, gpu_perf, gpu_time );
            }
            
            TESTING_FREE_CPU( h_A );
            TESTING_FREE_PIN( h_R );
            TESTING_FREE_DEV( d_A );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    opts.cleanup();
    TESTING_FINALIZE();
    return status;
}
