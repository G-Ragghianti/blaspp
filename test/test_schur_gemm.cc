// Copyright (c) 2017-2021, University of Tennessee. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// This program is free software: you can redistribute it and/or modify it under
// the terms of the BSD 3-Clause license. See the accompanying LICENSE file.

#include "test.hh"
#include "blas/flops.hh"
#include "print_matrix.hh"
#include "check_gemm.hh"

// -----------------------------------------------------------------------------
template< typename TA, typename TB, typename TC >
void test_schur_gemm_work( Params& params, bool run )
{
    using namespace testsweeper;
    using namespace blas;
    using namespace blas::batch;
    using scalar_t = blas::scalar_type< TA, TB, TC >;
    using real_t = blas::real_type< scalar_t >;
    typedef long long lld;

    // get & mark input values
    blas::Layout layout = Layout::ColMajor; //params.layout();
    blas::Op transA_    = Op::NoTrans; //params.transA();
    blas::Op transB_    = Op::NoTrans; //params.transB();
    scalar_t alpha_     = params.alpha();
    scalar_t beta_      = params.beta();
    int64_t m_          = params.dim.m();
    int64_t n_          = params.dim.n();
    int64_t k_          = params.dim.k(); //Used as the tile size nb.
    int64_t device  = params.device();
    int64_t align   = params.align();
    int64_t verbose = params.verbose();

    // mark non-standard output values
    params.gflops();
    params.ref_time();
    params.ref_gflops();

    if (! run)
        return;

    if (blas::get_device_count() == 0) {
        printf("skipping: no GPU devices or no GPU support\n" );
        return;
    }

    // Round m_ and n_ down to a multiple of k, since we are not dealing with
    // cleanup of partial tiles around the edge of the matrix.
    m_ = floor( (float)m_/k_ ) * k_;
    n_ = floor( (float)n_/k_ ) * k_;
    params.dim.m() = m_;
    params.dim.n() = n_;

    // setup
    int64_t Am = (transA_ == Op::NoTrans ? m_ : k_);
    int64_t An = (transA_ == Op::NoTrans ? k_ : m_);
    int64_t Bm = (transB_ == Op::NoTrans ? k_ : n_);
    int64_t Bn = (transB_ == Op::NoTrans ? n_ : k_);
    int64_t Cm = m_;
    int64_t Cn = n_;
    if (layout == Layout::RowMajor) {
        std::swap( Am, An );
        std::swap( Bm, Bn );
        std::swap( Cm, Cn );
    }

    int64_t mt = int64_t( m_ / k_ );
    int64_t nt = int64_t( n_ / k_ );
    size_t batch = mt*nt;

    int64_t lda_ = roundup( Am, align );
    int64_t ldb_ = roundup( Bm, align );
    int64_t ldc_ = roundup( Cm, align );
    size_t size_A = size_t(lda_)*An;
    size_t size_B = size_t(ldb_)*Bn;
    size_t size_C = size_t(ldc_)*Cn;
    TA* A    = new TA[ size_A ];
    TB* B    = new TB[ size_B ];
    TC* C    = new TC[ size_C ];
    TC* Cref = nullptr;
    if (params.ref() == 'y' || params.check() == 'y')
        Cref = new TC[ size_C ];

    // device specifics
    blas::Queue queue( device, batch );
    TA* dA = blas::device_malloc<TA>( size_A );
    TB* dB = blas::device_malloc<TB>( size_B );
    TC* dC = blas::device_malloc<TC>( size_C );

    // pointer arrays
    std::vector<TA*>   dAarray;
    std::vector<TB*>   dBarray;
    std::vector<TC*>   dCarray;

    // wrap scalar arguments in std::vector
    std::vector<blas::Op> transA(1, transA_);
    std::vector<blas::Op> transB(1, transB_);
    std::vector<int64_t>  k(1, k_);
    std::vector<int64_t>  ldda(1, lda_);
    std::vector<int64_t>  lddb(1, ldb_);
    std::vector<int64_t>  lddc(1, ldc_);
    std::vector<scalar_t> alpha(1, alpha_);
    std::vector<scalar_t> beta(1, beta_);

    int64_t idist = 1;
    int iseed[4] = { 0, 0, 0, 1 };
    lapack_larnv( idist, iseed, size_A, A );
    lapack_larnv( idist, iseed, size_B, B );
    lapack_larnv( idist, iseed, size_C, C );
    if (Cref != nullptr)
        lapack_lacpy( "g", Cm, Cn, C, ldc_, Cref, ldc_ );

    blas::device_setmatrix(Am, An, A, lda_, dA, lda_, queue);
    blas::device_setmatrix(Bm, Bn, B, ldb_, dB, ldb_, queue);
    blas::device_setmatrix(Cm, Cn, C, ldc_, dC, ldc_, queue);
    queue.sync();

    // norms for error check
    real_t work[1];
    real_t Anorm = lapack_lange( "f", Am, An, A, lda_, work );
    real_t Bnorm = lapack_lange( "f", Bm, Bn, B, ldb_, work );
    real_t Cnorm = lapack_lange( "f", Cm, Cn, C, ldc_, work );

    // Batch version (light blue line)
    double time_with_setup = get_wtime();
    // construct dAarray, dBarray, dCarray (on host) with pointers to tiles in dA, dB, dC
    for (int64_t j = 0; j < nt; ++j) {
        for (int64_t i = 0; i < mt; ++i) {
            dAarray.push_back( &dA[ i * k_ ] );  // i-th block row
            dBarray.push_back( &dB[ j * k_ * ldb_ ] );  // j-th block col
            dCarray.push_back( &dC[ i * k_ + j * k_ * ldc_ ] );  // (i, j)-th block
        }
    }

    // Run test.
    testsweeper::flush_cache( params.cache() );
    std::vector<int64_t> info;  // empty info vector (no checks)
    double time = get_wtime();
    blas::batch::gemm( layout, transA, transB, k, k, k, alpha, dAarray, ldda,
                       dBarray, lddb, beta, dCarray, lddc, batch, info, queue );
    queue.sync();
    double t = get_wtime();
    time_with_setup = t - time_with_setup;
    time = t - time;

    double gflop = Gflop < scalar_t >::gemm( m_, n_, k_ );
    params.time()   = time;
    params.gflops() = gflop / time;
    blas::device_getmatrix(Cm, Cn, dC, ldc_, C, ldc_, queue);
    queue.sync();

    if (params.ref() == 'y' || params.check() == 'y') {
        // Run reference (dark blue line)
        testsweeper::flush_cache( params.cache() );
        blas::device_setmatrix(Cm, Cn, Cref, ldc_, dC, ldc_, queue);
        queue.sync();

        double time_ref = get_wtime();
        blas::gemm( layout, transA_, transB_, m_, n_, k_, alpha_, dA, lda_, dB, ldb_,
                    beta_, dC, ldc_, queue );
        queue.sync();
        time_ref = get_wtime() - time_ref;
        params.ref_time()   = time_ref;
        params.ref_gflops() = gflop / time_ref;

        blas::device_getmatrix(Cm, Cn, dC, ldc_, Cref, ldc_, queue);
        queue.sync();

        // Error
        real_t error;
        bool okay;
        check_gemm( Cm, Cn, k_, alpha_, beta_, Anorm, Bnorm, Cnorm,
                    Cref, ldc_, C, ldc_, verbose, &error, &okay );

        params.error() = error;
        params.okay() = okay;

        delete[] Cref;
    }

    delete[] A;
    delete[] B;
    delete[] C;

    blas::device_free( dA );
    blas::device_free( dB );
    blas::device_free( dC );
}

// -----------------------------------------------------------------------------
void test_schur_gemm( Params& params, bool run )
{
    switch (params.datatype()) {
        case testsweeper::DataType::Single:
            test_schur_gemm_work< float, float, float >( params, run );
            break;

        case testsweeper::DataType::Double:
            test_schur_gemm_work< double, double, double >( params, run );
            break;

        case testsweeper::DataType::SingleComplex:
            test_schur_gemm_work< std::complex<float>, std::complex<float>,
                            std::complex<float> >( params, run );
            break;

        case testsweeper::DataType::DoubleComplex:
            test_schur_gemm_work< std::complex<double>, std::complex<double>,
                            std::complex<double> >( params, run );
            break;

        default:
            throw std::exception();
            break;
    }
}
