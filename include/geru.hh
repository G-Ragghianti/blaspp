#ifndef BLAS_GERU_HH
#define BLAS_GERU_HH

#include "blas_fortran.hh"
#include "blas_util.hh"
#include "ger.hh"

#include <limits>

namespace blas {

// =============================================================================
// Overloaded wrappers for s, d, c, z precisions.

// -----------------------------------------------------------------------------
/// @ingroup geru
inline
void geru(
    blas::Layout layout,
    int64_t m, int64_t n,
    float alpha,
    float const *x, int64_t incx,
    float const *y, int64_t incy,
    float       *A, int64_t lda )
{
    ger( layout, m, n, alpha, x, incx, y, incy, A, lda );
}

// -----------------------------------------------------------------------------
/// @ingroup geru
inline
void geru(
    blas::Layout layout,
    int64_t m, int64_t n,
    double alpha,
    double const *x, int64_t incx,
    double const *y, int64_t incy,
    double       *A, int64_t lda )
{
    ger( layout, m, n, alpha, x, incx, y, incy, A, lda );
}

// -----------------------------------------------------------------------------
/// @ingroup geru
inline
void geru(
    blas::Layout layout,
    int64_t m, int64_t n,
    std::complex<float> alpha,
    std::complex<float> const *x, int64_t incx,
    std::complex<float> const *y, int64_t incy,
    std::complex<float>       *A, int64_t lda )
{
    // check arguments
    blas_error_if( layout != Layout::ColMajor &&
                   layout != Layout::RowMajor );
    blas_error_if( m < 0 );
    blas_error_if( n < 0 );
    blas_error_if( incx == 0 );
    blas_error_if( incy == 0 );

    if (layout == Layout::ColMajor)
        blas_error_if( lda < m );
    else
        blas_error_if( lda < n );

    // check for overflow in native BLAS integer type, if smaller than int64_t
    if (sizeof(int64_t) > sizeof(blas_int)) {
        blas_error_if( m              > std::numeric_limits<blas_int>::max() );
        blas_error_if( n              > std::numeric_limits<blas_int>::max() );
        blas_error_if( lda            > std::numeric_limits<blas_int>::max() );
        blas_error_if( std::abs(incx) > std::numeric_limits<blas_int>::max() );
        blas_error_if( std::abs(incy) > std::numeric_limits<blas_int>::max() );
    }

    blas_int m_    = (blas_int) m;
    blas_int n_    = (blas_int) n;
    blas_int lda_  = (blas_int) lda;
    blas_int incx_ = (blas_int) incx;
    blas_int incy_ = (blas_int) incy;

    if (layout == Layout::RowMajor) {
        // swap m <=> n, x <=> y
        BLAS_cgeru( &n_, &m_, &alpha, y, &incy_, x, &incx_, A, &lda_ );
    }
    else {
        BLAS_cgeru( &m_, &n_, &alpha, x, &incx_, y, &incy_, A, &lda_ );
    }
}

// -----------------------------------------------------------------------------
/// @ingroup geru
inline
void geru(
    blas::Layout layout,
    int64_t m, int64_t n,
    std::complex<double> alpha,
    std::complex<double> const *x, int64_t incx,
    std::complex<double> const *y, int64_t incy,
    std::complex<double>       *A, int64_t lda )
{
    // check arguments
    blas_error_if( layout != Layout::ColMajor &&
                   layout != Layout::RowMajor );
    blas_error_if( m < 0 );
    blas_error_if( n < 0 );
    blas_error_if( incx == 0 );
    blas_error_if( incy == 0 );

    if (layout == Layout::ColMajor)
        blas_error_if( lda < m );
    else
        blas_error_if( lda < n );

    // check for overflow in native BLAS integer type, if smaller than int64_t
    if (sizeof(int64_t) > sizeof(blas_int)) {
        blas_error_if( m              > std::numeric_limits<blas_int>::max() );
        blas_error_if( n              > std::numeric_limits<blas_int>::max() );
        blas_error_if( lda            > std::numeric_limits<blas_int>::max() );
        blas_error_if( std::abs(incx) > std::numeric_limits<blas_int>::max() );
        blas_error_if( std::abs(incy) > std::numeric_limits<blas_int>::max() );
    }

    blas_int m_    = (blas_int) m;
    blas_int n_    = (blas_int) n;
    blas_int lda_  = (blas_int) lda;
    blas_int incx_ = (blas_int) incx;
    blas_int incy_ = (blas_int) incy;

    if (layout == Layout::RowMajor) {
        // swap m <=> n, x <=> y
        BLAS_zgeru( &n_, &m_, &alpha, y, &incy_, x, &incx_, A, &lda_ );
    }
    else {
        BLAS_zgeru( &m_, &n_, &alpha, x, &incx_, y, &incy_, A, &lda_ );
    }
}

// =============================================================================
/// General matrix rank-1 update,
///     \f[ A = \alpha x y^T + A, \f]
/// where alpha is a scalar, x and y are vectors,
/// and A is an m-by-n matrix.
///
/// Generic implementation for arbitrary data types.
///
/// @param[in] layout
///     Matrix storage, Layout::ColMajor or Layout::RowMajor.
///
/// @param[in] m
///     Number of rows of the matrix A. m >= 0.
///
/// @param[in] n
///     Number of columns of the matrix A. n >= 0.
///
/// @param[in] alpha
///     Scalar alpha. If alpha is zero, A is not updated.
///
/// @param[in] x
///     The m-element vector x, in an array of length (m-1)*abs(incx) + 1.
///
/// @param[in] incx
///     Stride between elements of x. incx must not be zero.
///     If incx < 0, uses elements of x in reverse order: x(n-1), ..., x(0).
///
/// @param[in] y
///     The n-element vector y, in an array of length (n-1)*abs(incy) + 1.
///
/// @param[in] incy
///     Stride between elements of y. incy must not be zero.
///     If incy < 0, uses elements of y in reverse order: y(n-1), ..., y(0).
///
/// @param[in, out] A
///     The m-by-n matrix A, stored in an lda-by-n array [RowMajor: m-by-lda].
///
/// @param[in] lda
///     Leading dimension of A. lda >= max(1, m) [RowMajor: lda >= max(1, n)].
///
/// @ingroup geru

template< typename TA, typename TX, typename TY >
void geru(
    blas::Layout layout,
    int64_t m, int64_t n,
    blas::scalar_type<TA, TX, TY> alpha,
    TX const *x, int64_t incx,
    TY const *y, int64_t incy,
    TA *A, int64_t lda )
{
    typedef blas::scalar_type<TA, TX, TY> scalar_t;

    #define A(i_, j_) A[ (i_) + (j_)*lda ]

    // constants
    const scalar_t zero = 0;

    // check arguments
    blas_error_if( layout != Layout::ColMajor &&
                   layout != Layout::RowMajor );
    blas_error_if( m < 0 );
    blas_error_if( n < 0 );
    blas_error_if( incx == 0 );
    blas_error_if( incy == 0 );

    if (layout == Layout::ColMajor)
        blas_error_if( lda < m );
    else
        blas_error_if( lda < n );

    // quick return
    if (m == 0 || n == 0 || alpha == zero)
        return;

    // for row-major, simply swap dimensions and x <=> y
    // this doesn't work in the complex gerc case because y gets conj
    if (layout == Layout::RowMajor) {
        geru( Layout::ColMajor, n, m, alpha, y, incy, x, incx, A, lda );
        return;
    }

    if (incx == 1 && incy == 1) {
        // unit stride
        for (int64_t j = 0; j < n; ++j) {
            // note: NOT skipping if y[j] is zero, for consistent NAN handling
            scalar_t tmp = alpha * y[j];
            for (int64_t i = 0; i < m; ++i) {
                A(i, j) += x[i] * tmp;
            }
        }
    }
    else if (incx == 1) {
        // x unit stride, y non-unit stride
        int64_t jy = (incy > 0 ? 0 : (-n + 1)*incy);
        for (int64_t j = 0; j < n; ++j) {
            scalar_t tmp = alpha * y[jy];
            for (int64_t i = 0; i < m; ++i) {
                A(i, j) += x[i] * tmp;
            }
            jy += incy;
        }
    }
    else {
        // x and y non-unit stride
        int64_t kx = (incx > 0 ? 0 : (-m + 1)*incx);
        int64_t jy = (incy > 0 ? 0 : (-n + 1)*incy);
        for (int64_t j = 0; j < n; ++j) {
            scalar_t tmp = alpha * y[jy];
            int64_t ix = kx;
            for (int64_t i = 0; i < m; ++i) {
                A(i, j) += x[ix] * tmp;
                ix += incx;
            }
            jy += incy;
        }
    }

    #undef A
}

}  // namespace blas

#endif        //  #ifndef BLAS_GER_HH
