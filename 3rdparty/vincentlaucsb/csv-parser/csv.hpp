#pragma once
/*
CSV for C++, version 5.3.0
https://github.com/vincentlaucsb/csv-parser

MIT License

Copyright (c) 2017-2026 Vincent La

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef CSV_HPP
#define CSV_HPP



#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

/** @file
 *  @brief Shared exception message templates and throw helpers.
 */


#include <set>
#include <stdexcept>
#include <string>
#include <system_error>

/** @file
 *  A standalone header file containing shared code
 */

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <limits>
#include <memory>
#if !defined(CSV_ENABLE_THREADS) || CSV_ENABLE_THREADS
#include <mutex>
#endif

#if defined(_WIN32)
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# undef max
# undef min
#elif defined(__linux__)
# include <unistd.h>
#endif

 /** Helper macro which should be #defined as "inline"
  *  in the single header version
  */
#define CSV_INLINE inline
#include <type_traits>

#if defined(__EMSCRIPTEN__)
#undef CSV_ENABLE_THREADS
#define CSV_ENABLE_THREADS 0
#elif !defined(CSV_ENABLE_THREADS)
#define CSV_ENABLE_THREADS 1
#endif

// Minimal portability macros (Hedley subset) with CSV_ prefix.
#if defined(CSV_CODE_COVERAGE)
    #define CSV_CONST
    #define CSV_PURE
    #define CSV_FORCE_INLINE inline
    #define CSV_PRIVATE
    #define CSV_NON_NULL(...)
#elif defined(__clang__) || defined(__GNUC__)
    #define CSV_CONST __attribute__((__const__))
    #define CSV_PURE __attribute__((__pure__))
    #define CSV_FORCE_INLINE inline __attribute__((__always_inline__))
    #if defined(_WIN32)
        #define CSV_PRIVATE
    #else
        #define CSV_PRIVATE __attribute__((__visibility__("hidden")))
    #endif
    #define CSV_NON_NULL(...) __attribute__((__nonnull__(__VA_ARGS__)))
#elif defined(_MSC_VER)
    #define CSV_CONST
    #define CSV_PURE
    #define CSV_FORCE_INLINE __forceinline
    #define CSV_PRIVATE
    #define CSV_NON_NULL(...)
#else
    #define CSV_CONST
    #define CSV_PURE
    #define CSV_FORCE_INLINE inline
    #define CSV_PRIVATE
    #define CSV_NON_NULL(...)
#endif

// MSVC-specific warning suppression helpers. Use __pragma so the macros
// work inside other macro bodies (where #pragma is not allowed).
#ifdef _MSC_VER
#  define CSV_MSVC_PUSH_DISABLE(w) __pragma(warning(push)) __pragma(warning(disable: w))
#  define CSV_MSVC_POP             __pragma(warning(pop))
#else
#  define CSV_MSVC_PUSH_DISABLE(w)
#  define CSV_MSVC_POP
#endif

// This library uses C++ exceptions for error reporting in public APIs.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND) || defined(__EXCEPTIONS)
    #define CSV_EXCEPTIONS_ENABLED 1
#else
    #define CSV_EXCEPTIONS_ENABLED 0
#endif

#if !CSV_EXCEPTIONS_ENABLED
    #error "csv-parser requires C++ exceptions. Enable exception handling (for example, remove -fno-exceptions or use /EHsc)."
#endif

// Detect C++ standard version BEFORE namespace to properly include string_view
// MSVC: __cplusplus == 199711L unless /Zc:__cplusplus is set; use _MSVC_LANG instead.
#if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
#  define CSV_CPLUSPLUS _MSVC_LANG
#else
#  define CSV_CPLUSPLUS __cplusplus
#endif

#if CSV_CPLUSPLUS >= 202302L
#define CSV_HAS_CXX23
#endif

#if CSV_CPLUSPLUS >= 202002L
#define CSV_HAS_CXX20
#endif

#if CSV_CPLUSPLUS >= 201703L
#define CSV_HAS_CXX17
#endif

#if CSV_CPLUSPLUS >= 201402L
#define CSV_HAS_CXX14
#endif

// Annotate intentional switch fallthroughs in parser hot loops without
// reshaping the control flow just to appease compiler diagnostics.
#if defined(CSV_HAS_CXX17)
#define CSV_FALLTHROUGH [[fallthrough]]
#elif defined(__clang__) && defined(__has_cpp_attribute)
#if __has_cpp_attribute(clang::fallthrough)
#define CSV_FALLTHROUGH [[clang::fallthrough]]
#else
#define CSV_FALLTHROUGH ((void)0)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 7
#define CSV_FALLTHROUGH __attribute__((fallthrough))
#else
#define CSV_FALLTHROUGH ((void)0)
#endif

// Include string_view BEFORE csv namespace to avoid namespace pollution issues
#ifdef CSV_HAS_CXX17
#include <string_view>
#else
// Copyright 2017-2020 by Martin Moene
//
// string-view lite, a C++17-like string_view for C++98 and later.
// For more information see https://github.com/martinmoene/string-view-lite
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#ifndef NONSTD_SV_LITE_H_INCLUDED
#define NONSTD_SV_LITE_H_INCLUDED

#define string_view_lite_MAJOR  1
#define string_view_lite_MINOR  8
#define string_view_lite_PATCH  0

#define string_view_lite_VERSION  nssv_STRINGIFY(string_view_lite_MAJOR) "." nssv_STRINGIFY(string_view_lite_MINOR) "." nssv_STRINGIFY(string_view_lite_PATCH)

#define nssv_STRINGIFY(  x )  nssv_STRINGIFY_( x )
#define nssv_STRINGIFY_( x )  #x

// string-view lite configuration:

#define nssv_STRING_VIEW_DEFAULT  0
#define nssv_STRING_VIEW_NONSTD   1
#define nssv_STRING_VIEW_STD      2

// tweak header support:

#ifdef __has_include
# if __has_include(<nonstd/string_view.tweak.hpp>)
#  include <nonstd/string_view.tweak.hpp>
# endif
#define nssv_HAVE_TWEAK_HEADER  1
#else
#define nssv_HAVE_TWEAK_HEADER  0
//# pragma message("string_view.hpp: Note: Tweak header not supported.")
#endif

// string_view selection and configuration:

#if !defined( nssv_CONFIG_SELECT_STRING_VIEW )
# define nssv_CONFIG_SELECT_STRING_VIEW  ( nssv_HAVE_STD_STRING_VIEW ? nssv_STRING_VIEW_STD : nssv_STRING_VIEW_NONSTD )
#endif

#ifndef  nssv_CONFIG_STD_SV_OPERATOR
# define nssv_CONFIG_STD_SV_OPERATOR  0
#endif

#ifndef  nssv_CONFIG_USR_SV_OPERATOR
# define nssv_CONFIG_USR_SV_OPERATOR  1
#endif

#ifdef   nssv_CONFIG_CONVERSION_STD_STRING
# define nssv_CONFIG_CONVERSION_STD_STRING_CLASS_METHODS   nssv_CONFIG_CONVERSION_STD_STRING
# define nssv_CONFIG_CONVERSION_STD_STRING_FREE_FUNCTIONS  nssv_CONFIG_CONVERSION_STD_STRING
#endif

#ifndef  nssv_CONFIG_CONVERSION_STD_STRING_CLASS_METHODS
# define nssv_CONFIG_CONVERSION_STD_STRING_CLASS_METHODS  1
#endif

#ifndef  nssv_CONFIG_CONVERSION_STD_STRING_FREE_FUNCTIONS
# define nssv_CONFIG_CONVERSION_STD_STRING_FREE_FUNCTIONS  1
#endif

#ifndef  nssv_CONFIG_NO_STREAM_INSERTION
# define nssv_CONFIG_NO_STREAM_INSERTION  0
#endif

#ifndef  nssv_CONFIG_CONSTEXPR11_STD_SEARCH
# define nssv_CONFIG_CONSTEXPR11_STD_SEARCH  1
#endif

// Control presence of exception handling (try and auto discover):

#ifndef nssv_CONFIG_NO_EXCEPTIONS
# if defined(_MSC_VER)
#  include <cstddef>    // for _HAS_EXCEPTIONS
# endif
# if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || (_HAS_EXCEPTIONS)
#  define nssv_CONFIG_NO_EXCEPTIONS  0
# else
#  define nssv_CONFIG_NO_EXCEPTIONS  1
# endif
#endif

// C++ language version detection (C++23 is speculative):
// Note: VC14.0/1900 (VS2015) lacks too much from C++14.

#ifndef   nssv_CPLUSPLUS
# if defined(_MSVC_LANG ) && !defined(__clang__)
#  define nssv_CPLUSPLUS  (_MSC_VER == 1900 ? 201103L : _MSVC_LANG )
# else
#  define nssv_CPLUSPLUS  __cplusplus
# endif
#endif

#define nssv_CPP98_OR_GREATER  ( nssv_CPLUSPLUS >= 199711L )
#define nssv_CPP11_OR_GREATER  ( nssv_CPLUSPLUS >= 201103L )
#define nssv_CPP11_OR_GREATER_ ( nssv_CPLUSPLUS >= 201103L )
#define nssv_CPP14_OR_GREATER  ( nssv_CPLUSPLUS >= 201402L )
#define nssv_CPP17_OR_GREATER  ( nssv_CPLUSPLUS >= 201703L )
#define nssv_CPP20_OR_GREATER  ( nssv_CPLUSPLUS >= 202002L )
#define nssv_CPP23_OR_GREATER  ( nssv_CPLUSPLUS >= 202300L )

// use C++17 std::string_view if available and requested:

#if nssv_CPP17_OR_GREATER && defined(__has_include )
# if __has_include( <string_view> )
#  define nssv_HAVE_STD_STRING_VIEW  1
# else
#  define nssv_HAVE_STD_STRING_VIEW  0
# endif
#else
# define  nssv_HAVE_STD_STRING_VIEW  0
#endif

#define  nssv_USES_STD_STRING_VIEW  ( (nssv_CONFIG_SELECT_STRING_VIEW == nssv_STRING_VIEW_STD) || ((nssv_CONFIG_SELECT_STRING_VIEW == nssv_STRING_VIEW_DEFAULT) && nssv_HAVE_STD_STRING_VIEW) )

#define nssv_HAVE_STARTS_WITH ( nssv_CPP20_OR_GREATER || !nssv_USES_STD_STRING_VIEW )
#define nssv_HAVE_ENDS_WITH     nssv_HAVE_STARTS_WITH

//
// Use C++17 std::string_view:
//

#if nssv_USES_STD_STRING_VIEW

#include <string_view>

// Extensions for std::string:

#if nssv_CONFIG_CONVERSION_STD_STRING_FREE_FUNCTIONS

#include <string>

namespace nonstd {

template< class CharT, class Traits, class Allocator = std::allocator<CharT> >
std::basic_string<CharT, Traits, Allocator>
to_string( std::basic_string_view<CharT, Traits> v, Allocator const & a = Allocator() )
{
    return std::basic_string<CharT,Traits, Allocator>( v.begin(), v.end(), a );
}

template< class CharT, class Traits, class Allocator >
std::basic_string_view<CharT, Traits>
to_string_view( std::basic_string<CharT, Traits, Allocator> const & s )
{
    return std::basic_string_view<CharT, Traits>( s.data(), s.size() );
}

// Literal operators sv and _sv:

#if nssv_CONFIG_STD_SV_OPERATOR

using namespace std::literals::string_view_literals;

#endif

#if nssv_CONFIG_USR_SV_OPERATOR

inline namespace literals {
inline namespace string_view_literals {


constexpr std::string_view operator""_sv( const char* str, size_t len ) noexcept  // (1)
{
    return std::string_view{ str, len };
}

constexpr std::u16string_view operator""_sv( const char16_t* str, size_t len ) noexcept  // (2)
{
    return std::u16string_view{ str, len };
}

constexpr std::u32string_view operator""_sv( const char32_t* str, size_t len ) noexcept  // (3)
{
    return std::u32string_view{ str, len };
}

constexpr std::wstring_view operator""_sv( const wchar_t* str, size_t len ) noexcept  // (4)
{
    return std::wstring_view{ str, len };
}

}} // namespace literals::string_view_literals

#endif // nssv_CONFIG_USR_SV_OPERATOR

} // namespace nonstd

#endif // nssv_CONFIG_CONVERSION_STD_STRING_FREE_FUNCTIONS

namespace nonstd {

using std::string_view;
using std::wstring_view;
using std::u16string_view;
using std::u32string_view;
using std::basic_string_view;

// literal "sv" and "_sv", see above

using std::operator==;
using std::operator!=;
using std::operator<;
using std::operator<=;
using std::operator>;
using std::operator>=;

using std::operator<<;

} // namespace nonstd

#else // nssv_HAVE_STD_STRING_VIEW

//
// Before C++17: use string_view lite:
//

// Compiler versions:
//
// MSVC++  6.0  _MSC_VER == 1200  nssv_COMPILER_MSVC_VERSION ==  60  (Visual Studio 6.0)
// MSVC++  7.0  _MSC_VER == 1300  nssv_COMPILER_MSVC_VERSION ==  70  (Visual Studio .NET 2002)
// MSVC++  7.1  _MSC_VER == 1310  nssv_COMPILER_MSVC_VERSION ==  71  (Visual Studio .NET 2003)
// MSVC++  8.0  _MSC_VER == 1400  nssv_COMPILER_MSVC_VERSION ==  80  (Visual Studio 2005)
// MSVC++  9.0  _MSC_VER == 1500  nssv_COMPILER_MSVC_VERSION ==  90  (Visual Studio 2008)
// MSVC++ 10.0  _MSC_VER == 1600  nssv_COMPILER_MSVC_VERSION == 100  (Visual Studio 2010)
// MSVC++ 11.0  _MSC_VER == 1700  nssv_COMPILER_MSVC_VERSION == 110  (Visual Studio 2012)
// MSVC++ 12.0  _MSC_VER == 1800  nssv_COMPILER_MSVC_VERSION == 120  (Visual Studio 2013)
// MSVC++ 14.0  _MSC_VER == 1900  nssv_COMPILER_MSVC_VERSION == 140  (Visual Studio 2015)
// MSVC++ 14.1  _MSC_VER >= 1910  nssv_COMPILER_MSVC_VERSION == 141  (Visual Studio 2017)
// MSVC++ 14.2  _MSC_VER >= 1920  nssv_COMPILER_MSVC_VERSION == 142  (Visual Studio 2019)

#if defined(_MSC_VER ) && !defined(__clang__)
# define nssv_COMPILER_MSVC_VER      (_MSC_VER )
# define nssv_COMPILER_MSVC_VERSION  (_MSC_VER / 10 - 10 * ( 5 + (_MSC_VER < 1900 ) ) )
#else
# define nssv_COMPILER_MSVC_VER      0
# define nssv_COMPILER_MSVC_VERSION  0
#endif

#define nssv_COMPILER_VERSION( major, minor, patch )  ( 10 * ( 10 * (major) + (minor) ) + (patch) )

#if defined( __apple_build_version__ )
# define nssv_COMPILER_APPLECLANG_VERSION  nssv_COMPILER_VERSION(__clang_major__, __clang_minor__, __clang_patchlevel__)
# define nssv_COMPILER_CLANG_VERSION       0
#elif defined( __clang__ )
# define nssv_COMPILER_APPLECLANG_VERSION  0
# define nssv_COMPILER_CLANG_VERSION       nssv_COMPILER_VERSION(__clang_major__, __clang_minor__, __clang_patchlevel__)
#else
# define nssv_COMPILER_APPLECLANG_VERSION  0
# define nssv_COMPILER_CLANG_VERSION       0
#endif

#if defined(__GNUC__) && !defined(__clang__)
# define nssv_COMPILER_GNUC_VERSION  nssv_COMPILER_VERSION(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#else
# define nssv_COMPILER_GNUC_VERSION  0
#endif

// half-open range [lo..hi):
#define nssv_BETWEEN( v, lo, hi ) ( (lo) <= (v) && (v) < (hi) )

// Presence of language and library features:

#ifdef _HAS_CPP0X
# define nssv_HAS_CPP0X  _HAS_CPP0X
#else
# define nssv_HAS_CPP0X  0
#endif

// Unless defined otherwise below, consider VC14 as C++11 for string-view-lite:

#if nssv_COMPILER_MSVC_VER >= 1900
# undef  nssv_CPP11_OR_GREATER
# define nssv_CPP11_OR_GREATER  1
#endif

#define nssv_CPP11_90   (nssv_CPP11_OR_GREATER_ || nssv_COMPILER_MSVC_VER >= 1500)
#define nssv_CPP11_100  (nssv_CPP11_OR_GREATER_ || nssv_COMPILER_MSVC_VER >= 1600)
#define nssv_CPP11_110  (nssv_CPP11_OR_GREATER_ || nssv_COMPILER_MSVC_VER >= 1700)
#define nssv_CPP11_120  (nssv_CPP11_OR_GREATER_ || nssv_COMPILER_MSVC_VER >= 1800)
#define nssv_CPP11_140  (nssv_CPP11_OR_GREATER_ || nssv_COMPILER_MSVC_VER >= 1900)
#define nssv_CPP11_141  (nssv_CPP11_OR_GREATER_ || nssv_COMPILER_MSVC_VER >= 1910)

#define nssv_CPP14_000  (nssv_CPP14_OR_GREATER)
#define nssv_CPP17_000  (nssv_CPP17_OR_GREATER)

// Presence of C++11 language features:

#define nssv_HAVE_CONSTEXPR_11          nssv_CPP11_140
#define nssv_HAVE_EXPLICIT_CONVERSION   nssv_CPP11_140
#define nssv_HAVE_INLINE_NAMESPACE      nssv_CPP11_140
#define nssv_HAVE_IS_DEFAULT            nssv_CPP11_140
#define nssv_HAVE_IS_DELETE             nssv_CPP11_140
#define nssv_HAVE_NOEXCEPT              nssv_CPP11_140
#define nssv_HAVE_NULLPTR               nssv_CPP11_100
#define nssv_HAVE_REF_QUALIFIER         nssv_CPP11_140
#define nssv_HAVE_UNICODE_LITERALS      nssv_CPP11_140
#define nssv_HAVE_USER_DEFINED_LITERALS nssv_CPP11_140
#define nssv_HAVE_WCHAR16_T             nssv_CPP11_100
#define nssv_HAVE_WCHAR32_T             nssv_CPP11_100

#if ! ( ( nssv_CPP11_OR_GREATER && nssv_COMPILER_CLANG_VERSION ) || nssv_BETWEEN( nssv_COMPILER_CLANG_VERSION, 300, 400 ) )
# define nssv_HAVE_STD_DEFINED_LITERALS  nssv_CPP11_140
#else
# define nssv_HAVE_STD_DEFINED_LITERALS  0
#endif

// Presence of C++14 language features:

#define nssv_HAVE_CONSTEXPR_14          nssv_CPP14_000

// Presence of C++17 language features:

#define nssv_HAVE_NODISCARD             nssv_CPP17_000

// Presence of C++ library features:

#define nssv_HAVE_STD_HASH              nssv_CPP11_120

// Presence of compiler intrinsics:

// Providing char-type specializations for compare() and length() that
// use compiler intrinsics can improve compile- and run-time performance.
//
// The challenge is in using the right combinations of builtin availability
// and its constexpr-ness.
//
// | compiler | __builtin_memcmp (constexpr) | memcmp  (constexpr) |
// |----------|------------------------------|---------------------|
// | clang    | 4.0              (>= 4.0   ) | any     (?        ) |
// | clang-a  | 9.0              (>= 9.0   ) | any     (?        ) |
// | gcc      | any              (constexpr) | any     (?        ) |
// | msvc     | >= 14.2 C++17    (>= 14.2  ) | any     (?        ) |

#define nssv_HAVE_BUILTIN_VER     ( (nssv_CPP17_000 && nssv_COMPILER_MSVC_VERSION >= 142) || nssv_COMPILER_GNUC_VERSION > 0 || nssv_COMPILER_CLANG_VERSION >= 400 || nssv_COMPILER_APPLECLANG_VERSION >= 900 )
#define nssv_HAVE_BUILTIN_CE      (  nssv_HAVE_BUILTIN_VER )

#define nssv_HAVE_BUILTIN_MEMCMP  ( (nssv_HAVE_CONSTEXPR_14 && nssv_HAVE_BUILTIN_CE) || !nssv_HAVE_CONSTEXPR_14 )
#define nssv_HAVE_BUILTIN_STRLEN  ( (nssv_HAVE_CONSTEXPR_11 && nssv_HAVE_BUILTIN_CE) || !nssv_HAVE_CONSTEXPR_11 )

#ifdef __has_builtin
# define nssv_HAVE_BUILTIN( x )  __has_builtin( x )
#else
# define nssv_HAVE_BUILTIN( x )  0
#endif

#if nssv_HAVE_BUILTIN(__builtin_memcmp) || nssv_HAVE_BUILTIN_VER
# define nssv_BUILTIN_MEMCMP  __builtin_memcmp
#else
# define nssv_BUILTIN_MEMCMP  memcmp
#endif

#if nssv_HAVE_BUILTIN(__builtin_strlen) || nssv_HAVE_BUILTIN_VER
# define nssv_BUILTIN_STRLEN  __builtin_strlen
#else
# define nssv_BUILTIN_STRLEN  strlen
#endif

// C++ feature usage:

#if nssv_HAVE_CONSTEXPR_11
# define nssv_constexpr  constexpr
#else
# define nssv_constexpr  /*constexpr*/
#endif

#if  nssv_HAVE_CONSTEXPR_14
# define nssv_constexpr14  constexpr
#else
# define nssv_constexpr14  /*constexpr*/
#endif

#if nssv_HAVE_EXPLICIT_CONVERSION
# define nssv_explicit  explicit
#else
# define nssv_explicit  /*explicit*/
#endif

#if nssv_HAVE_INLINE_NAMESPACE
# define nssv_inline_ns  inline
#else
# define nssv_inline_ns  /*inline*/
#endif

#if nssv_HAVE_NOEXCEPT
# define nssv_noexcept  noexcept
#else
# define nssv_noexcept  /*noexcept*/
#endif

//#if nssv_HAVE_REF_QUALIFIER
//# define nssv_ref_qual  &
//# define nssv_refref_qual  &&
//#else
//# define nssv_ref_qual  /*&*/
//# define nssv_refref_qual  /*&&*/
//#endif

#if nssv_HAVE_NULLPTR
# define nssv_nullptr  nullptr
#else
# define nssv_nullptr  NULL
#endif

#if nssv_HAVE_NODISCARD
# define nssv_nodiscard  [[nodiscard]]
#else
# define nssv_nodiscard  /*[[nodiscard]]*/
#endif

// Additional includes:

#include <algorithm>
#include <cassert>
#include <iterator>
#include <limits>
#include <string>   // std::char_traits<>

#if ! nssv_CONFIG_NO_STREAM_INSERTION
# include <ostream>
#endif

#if ! nssv_CONFIG_NO_EXCEPTIONS
# include <stdexcept>
#endif

#if nssv_CPP11_OR_GREATER
# include <type_traits>
#endif

// Clang, GNUC, MSVC warning suppression macros:

#if defined(__clang__)
# pragma clang diagnostic ignored "-Wreserved-user-defined-literal"
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wuser-defined-literals"
#elif nssv_COMPILER_GNUC_VERSION >= 480
#  pragma  GCC  diagnostic push
#  pragma  GCC  diagnostic ignored "-Wliteral-suffix"
#endif // __clang__

#if nssv_COMPILER_MSVC_VERSION >= 140
# define nssv_SUPPRESS_MSGSL_WARNING(expr)        [[gsl::suppress(expr)]]
# define nssv_SUPPRESS_MSVC_WARNING(code, descr)  __pragma(warning(suppress: code) )
# define nssv_DISABLE_MSVC_WARNINGS(codes)        __pragma(warning(push))  __pragma(warning(disable: codes))
#else
# define nssv_SUPPRESS_MSGSL_WARNING(expr)
# define nssv_SUPPRESS_MSVC_WARNING(code, descr)
# define nssv_DISABLE_MSVC_WARNINGS(codes)
#endif

#if defined(__clang__)
# define nssv_RESTORE_WARNINGS()  _Pragma("clang diagnostic pop")
#elif nssv_COMPILER_GNUC_VERSION >= 480
#  define nssv_RESTORE_WARNINGS()  _Pragma("GCC diagnostic pop")
#elif nssv_COMPILER_MSVC_VERSION >= 140
# define nssv_RESTORE_WARNINGS()  __pragma(warning(pop ))
#else
# define nssv_RESTORE_WARNINGS()
#endif

// Suppress the following MSVC (GSL) warnings:
// - C4455, non-gsl   : 'operator ""sv': literal suffix identifiers that do not
//                      start with an underscore are reserved
// - C26472, gsl::t.1 : don't use a static_cast for arithmetic conversions;
//                      use brace initialization, gsl::narrow_cast or gsl::narow
// - C26481: gsl::b.1 : don't use pointer arithmetic. Use span instead

nssv_DISABLE_MSVC_WARNINGS( 4455 26481 26472 )
//nssv_DISABLE_CLANG_WARNINGS( "-Wuser-defined-literals" )
//nssv_DISABLE_GNUC_WARNINGS( -Wliteral-suffix )

namespace nonstd { namespace sv_lite {

//
// basic_string_view declaration:
//

template
<
    class CharT,
    class Traits = std::char_traits<CharT>
>
class basic_string_view;

namespace detail {

// support constexpr comparison in C++14;
// for C++17 and later, use provided traits:

template< typename CharT >
inline nssv_constexpr14 int compare( CharT const * s1, CharT const * s2, std::size_t count )
{
    while ( count-- != 0 )
    {
        if ( *s1 < *s2 ) return -1;
        if ( *s1 > *s2 ) return +1;
        ++s1; ++s2;
    }
    return 0;
}

#if nssv_HAVE_BUILTIN_MEMCMP

// specialization of compare() for char, see also generic compare() above:

inline nssv_constexpr14 int compare( char const * s1, char const * s2, std::size_t count )
{
    return nssv_BUILTIN_MEMCMP( s1, s2, count );
}

#endif

#if nssv_HAVE_BUILTIN_STRLEN

// specialization of length() for char, see also generic length() further below:

inline nssv_constexpr std::size_t length( char const * s )
{
    return nssv_BUILTIN_STRLEN( s );
}

#endif

#if defined(__OPTIMIZE__)

// gcc, clang provide __OPTIMIZE__
// Expect tail call optimization to make length() non-recursive:

template< typename CharT >
inline nssv_constexpr std::size_t length( CharT * s, std::size_t result = 0 )
{
    return *s == '\0' ? result : length( s + 1, result + 1 );
}

#else // OPTIMIZE

// non-recursive:

template< typename CharT >
inline nssv_constexpr14 std::size_t length( CharT * s )
{
    std::size_t result = 0;
    while ( *s++ != '\0' )
    {
       ++result;
    }
    return result;
}

#endif // OPTIMIZE

#if nssv_CPP11_OR_GREATER && ! nssv_CPP17_OR_GREATER
#if defined(__OPTIMIZE__)

// gcc, clang provide __OPTIMIZE__
// Expect tail call optimization to make search() non-recursive:

template< class CharT, class Traits = std::char_traits<CharT> >
constexpr const CharT* search( basic_string_view<CharT, Traits> haystack, basic_string_view<CharT, Traits> needle )
{
    return haystack.starts_with( needle ) ? haystack.begin() :
        haystack.empty() ? haystack.end() : search( haystack.substr(1), needle );
}

#else // OPTIMIZE

// non-recursive:

#if nssv_CONFIG_CONSTEXPR11_STD_SEARCH

template< class CharT, class Traits = std::char_traits<CharT> >
constexpr const CharT* search( basic_string_view<CharT, Traits> haystack, basic_string_view<CharT, Traits> needle )
{
    return std::search( haystack.begin(), haystack.end(), needle.begin(), needle.end() );
}

#else // nssv_CONFIG_CONSTEXPR11_STD_SEARCH

template< class CharT, class Traits = std::char_traits<CharT> >
nssv_constexpr14 const CharT* search( basic_string_view<CharT, Traits> haystack, basic_string_view<CharT, Traits> needle )
{
    while ( needle.size() <= haystack.size() )
    {
        if  ( haystack.starts_with(needle) )
        {
            return haystack.cbegin();
        }
        haystack = basic_string_view<CharT, Traits>{ haystack.begin() + 1, haystack.size() - 1U };
    }
    return haystack.cend();
}
#endif // nssv_CONFIG_CONSTEXPR11_STD_SEARCH

#endif // OPTIMIZE
#endif // nssv_CPP11_OR_GREATER && ! nssv_CPP17_OR_GREATER

} // namespace detail

//
// basic_string_view:
//

template
<
    class CharT,
    class Traits /* = std::char_traits<CharT> */
>
class basic_string_view
{
public:
    // Member types:

    typedef Traits traits_type;
    typedef CharT  value_type;

    typedef CharT       * pointer;
    typedef CharT const * const_pointer;
    typedef CharT       & reference;
    typedef CharT const & const_reference;

    typedef const_pointer iterator;
    typedef const_pointer const_iterator;
    typedef std::reverse_iterator< const_iterator > reverse_iterator;
    typedef std::reverse_iterator< const_iterator > const_reverse_iterator;

    typedef std::size_t     size_type;
    typedef std::ptrdiff_t  difference_type;

    // 24.4.2.1 Construction and assignment:

    nssv_constexpr basic_string_view() nssv_noexcept
        : data_( nssv_nullptr )
        , size_( 0 )
    {}

#if nssv_CPP11_OR_GREATER
    nssv_constexpr basic_string_view( basic_string_view const & other ) nssv_noexcept = default;
#else
    nssv_constexpr basic_string_view( basic_string_view const & other ) nssv_noexcept
        : data_( other.data_)
        , size_( other.size_)
    {}
#endif

    nssv_constexpr basic_string_view( CharT const * s, size_type count ) nssv_noexcept // non-standard noexcept
        : data_( s )
        , size_( count )
    {}

    nssv_constexpr basic_string_view( CharT const * s) nssv_noexcept // non-standard noexcept
        : data_( s )
#if nssv_CPP17_OR_GREATER
        , size_( Traits::length(s) )
#elif nssv_CPP11_OR_GREATER
        , size_( detail::length(s) )
#else
        , size_( Traits::length(s) )
#endif
    {}

#if  nssv_HAVE_NULLPTR
# if nssv_HAVE_IS_DELETE
    nssv_constexpr basic_string_view( std::nullptr_t ) nssv_noexcept = delete;
# else
    private: nssv_constexpr basic_string_view( std::nullptr_t ) nssv_noexcept; public:
# endif
#endif

    // Assignment:

#if nssv_CPP11_OR_GREATER
    nssv_constexpr14 basic_string_view & operator=( basic_string_view const & other ) nssv_noexcept = default;
#else
    nssv_constexpr14 basic_string_view & operator=( basic_string_view const & other ) nssv_noexcept
    {
        data_ = other.data_;
        size_ = other.size_;
        return *this;
    }
#endif

    // 24.4.2.2 Iterator support:

    nssv_constexpr const_iterator begin()  const nssv_noexcept { return data_;         }
    nssv_constexpr const_iterator end()    const nssv_noexcept { return data_ + size_; }

    nssv_constexpr const_iterator cbegin() const nssv_noexcept { return begin(); }
    nssv_constexpr const_iterator cend()   const nssv_noexcept { return end();   }

    nssv_constexpr const_reverse_iterator rbegin()  const nssv_noexcept { return const_reverse_iterator( end() );   }
    nssv_constexpr const_reverse_iterator rend()    const nssv_noexcept { return const_reverse_iterator( begin() ); }

    nssv_constexpr const_reverse_iterator crbegin() const nssv_noexcept { return rbegin(); }
    nssv_constexpr const_reverse_iterator crend()   const nssv_noexcept { return rend();   }

    // 24.4.2.3 Capacity:

    nssv_constexpr size_type size()     const nssv_noexcept { return size_; }
    nssv_constexpr size_type length()   const nssv_noexcept { return size_; }
    nssv_constexpr size_type max_size() const nssv_noexcept { return (std::numeric_limits< size_type >::max)(); }

    // since C++20
    nssv_nodiscard nssv_constexpr bool empty() const nssv_noexcept
    {
        return 0 == size_;
    }

    // 24.4.2.4 Element access:

    nssv_constexpr const_reference operator[]( size_type pos ) const
    {
        return data_at( pos );
    }

    nssv_constexpr14 const_reference at( size_type pos ) const
    {
#if nssv_CONFIG_NO_EXCEPTIONS
        assert( pos < size() );
#else
        if ( pos >= size() )
        {
            throw std::out_of_range("nonstd::string_view::at()");
        }
#endif
        return data_at( pos );
    }

    nssv_constexpr const_reference front() const { return data_at( 0 );          }
    nssv_constexpr const_reference back()  const { return data_at( size() - 1 ); }

    nssv_constexpr const_pointer   data()  const nssv_noexcept { return data_; }

    // 24.4.2.5 Modifiers:

    nssv_constexpr14 void remove_prefix( size_type n )
    {
        assert( n <= size() );
        data_ += n;
        size_ -= n;
    }

    nssv_constexpr14 void remove_suffix( size_type n )
    {
        assert( n <= size() );
        size_ -= n;
    }

    nssv_constexpr14 void swap( basic_string_view & other ) nssv_noexcept
    {
        const basic_string_view tmp(other);
        other = *this;
        *this = tmp;
    }

    // 24.4.2.6 String operations:

    size_type copy( CharT * dest, size_type n, size_type pos = 0 ) const
    {
#if nssv_CONFIG_NO_EXCEPTIONS
        assert( pos <= size() );
#else
        if ( pos > size() )
        {
            throw std::out_of_range("nonstd::string_view::copy()");
        }
#endif
        const size_type rlen = (std::min)( n, size() - pos );

        (void) Traits::copy( dest, data() + pos, rlen );

        return rlen;
    }

    nssv_constexpr14 basic_string_view substr( size_type pos = 0, size_type n = npos ) const
    {
#if nssv_CONFIG_NO_EXCEPTIONS
        assert( pos <= size() );
#else
        if ( pos > size() )
        {
            throw std::out_of_range("nonstd::string_view::substr()");
        }
#endif
        return basic_string_view( data() + pos, (std::min)( n, size() - pos ) );
    }

    // compare(), 6x:

    nssv_constexpr14 int compare( basic_string_view other ) const nssv_noexcept // (1)
    {
#if nssv_CPP17_OR_GREATER
        if ( const int result = Traits::compare( data(), other.data(), (std::min)( size(), other.size() ) ) )
#else
        if ( const int result = detail::compare( data(), other.data(), (std::min)( size(), other.size() ) ) )
#endif
        {
            return result;
        }

        return size() == other.size() ? 0 : size() < other.size() ? -1 : 1;
    }

    nssv_constexpr int compare( size_type pos1, size_type n1, basic_string_view other ) const // (2)
    {
        return substr( pos1, n1 ).compare( other );
    }

    nssv_constexpr int compare( size_type pos1, size_type n1, basic_string_view other, size_type pos2, size_type n2 ) const // (3)
    {
        return substr( pos1, n1 ).compare( other.substr( pos2, n2 ) );
    }

    nssv_constexpr int compare( CharT const * s ) const // (4)
    {
        return compare( basic_string_view( s ) );
    }

    nssv_constexpr int compare( size_type pos1, size_type n1, CharT const * s ) const // (5)
    {
        return substr( pos1, n1 ).compare( basic_string_view( s ) );
    }

    nssv_constexpr int compare( size_type pos1, size_type n1, CharT const * s, size_type n2 ) const // (6)
    {
        return substr( pos1, n1 ).compare( basic_string_view( s, n2 ) );
    }

    // 24.4.2.7 Searching:

    // starts_with(), 3x, since C++20:

    nssv_constexpr bool starts_with( basic_string_view v ) const nssv_noexcept  // (1)
    {
        return size() >= v.size() && compare( 0, v.size(), v ) == 0;
    }

    nssv_constexpr bool starts_with( CharT c ) const nssv_noexcept  // (2)
    {
        return starts_with( basic_string_view( &c, 1 ) );
    }

    nssv_constexpr bool starts_with( CharT const * s ) const  // (3)
    {
        return starts_with( basic_string_view( s ) );
    }

    // ends_with(), 3x, since C++20:

    nssv_constexpr bool ends_with( basic_string_view v ) const nssv_noexcept  // (1)
    {
        return size() >= v.size() && compare( size() - v.size(), npos, v ) == 0;
    }

    nssv_constexpr bool ends_with( CharT c ) const nssv_noexcept  // (2)
    {
        return ends_with( basic_string_view( &c, 1 ) );
    }

    nssv_constexpr bool ends_with( CharT const * s ) const  // (3)
    {
        return ends_with( basic_string_view( s ) );
    }

    // find(), 4x:

    nssv_constexpr14 size_type find( basic_string_view v, size_type pos = 0 ) const nssv_noexcept  // (1)
    {
        return assert( v.size() == 0 || v.data() != nssv_nullptr )
            , pos >= size()
            ? npos : to_pos(
#if nssv_CPP11_OR_GREATER && ! nssv_CPP17_OR_GREATER
                detail::search( substr(pos), v )
#else
                std::search( cbegin() + pos, cend(), v.cbegin(), v.cend(), Traits::eq )
#endif
            );
    }

    nssv_constexpr size_type find( CharT c, size_type pos = 0 ) const nssv_noexcept  // (2)
    {
        return find( basic_string_view( &c, 1 ), pos );
    }

    nssv_constexpr size_type find( CharT const * s, size_type pos, size_type n ) const  // (3)
    {
        return find( basic_string_view( s, n ), pos );
    }

    nssv_constexpr size_type find( CharT const * s, size_type pos = 0 ) const  // (4)
    {
        return find( basic_string_view( s ), pos );
    }

    // rfind(), 4x:

    nssv_constexpr14 size_type rfind( basic_string_view v, size_type pos = npos ) const nssv_noexcept  // (1)
    {
        if ( size() < v.size() )
        {
            return npos;
        }

        if ( v.empty() )
        {
            return (std::min)( size(), pos );
        }

        const_iterator last   = cbegin() + (std::min)( size() - v.size(), pos ) + v.size();
        const_iterator result = std::find_end( cbegin(), last, v.cbegin(), v.cend(), Traits::eq );

        return result != last ? size_type( result - cbegin() ) : npos;
    }

    nssv_constexpr14 size_type rfind( CharT c, size_type pos = npos ) const nssv_noexcept  // (2)
    {
        return rfind( basic_string_view( &c, 1 ), pos );
    }

    nssv_constexpr14 size_type rfind( CharT const * s, size_type pos, size_type n ) const  // (3)
    {
        return rfind( basic_string_view( s, n ), pos );
    }

    nssv_constexpr14 size_type rfind( CharT const * s, size_type pos = npos ) const  // (4)
    {
        return rfind( basic_string_view( s ), pos );
    }

    // find_first_of(), 4x:

    nssv_constexpr size_type find_first_of( basic_string_view v, size_type pos = 0 ) const nssv_noexcept  // (1)
    {
        return pos >= size()
            ? npos
            : to_pos( std::find_first_of( cbegin() + pos, cend(), v.cbegin(), v.cend(), Traits::eq ) );
    }

    nssv_constexpr size_type find_first_of( CharT c, size_type pos = 0 ) const nssv_noexcept  // (2)
    {
        return find_first_of( basic_string_view( &c, 1 ), pos );
    }

    nssv_constexpr size_type find_first_of( CharT const * s, size_type pos, size_type n ) const  // (3)
    {
        return find_first_of( basic_string_view( s, n ), pos );
    }

    nssv_constexpr size_type find_first_of(  CharT const * s, size_type pos = 0 ) const  // (4)
    {
        return find_first_of( basic_string_view( s ), pos );
    }

    // find_last_of(), 4x:

    nssv_constexpr size_type find_last_of( basic_string_view v, size_type pos = npos ) const nssv_noexcept  // (1)
    {
        return empty()
            ? npos
            : pos >= size()
            ? find_last_of( v, size() - 1 )
            : to_pos( std::find_first_of( const_reverse_iterator( cbegin() + pos + 1 ), crend(), v.cbegin(), v.cend(), Traits::eq ) );
    }

    nssv_constexpr size_type find_last_of( CharT c, size_type pos = npos ) const nssv_noexcept  // (2)
    {
        return find_last_of( basic_string_view( &c, 1 ), pos );
    }

    nssv_constexpr size_type find_last_of( CharT const * s, size_type pos, size_type count ) const  // (3)
    {
        return find_last_of( basic_string_view( s, count ), pos );
    }

    nssv_constexpr size_type find_last_of( CharT const * s, size_type pos = npos ) const  // (4)
    {
        return find_last_of( basic_string_view( s ), pos );
    }

    // find_first_not_of(), 4x:

    nssv_constexpr size_type find_first_not_of( basic_string_view v, size_type pos = 0 ) const nssv_noexcept  // (1)
    {
        return pos >= size()
            ? npos
            : to_pos( std::find_if( cbegin() + pos, cend(), not_in_view( v ) ) );
    }

    nssv_constexpr size_type find_first_not_of( CharT c, size_type pos = 0 ) const nssv_noexcept  // (2)
    {
        return find_first_not_of( basic_string_view( &c, 1 ), pos );
    }

    nssv_constexpr size_type find_first_not_of( CharT const * s, size_type pos, size_type count ) const  // (3)
    {
        return find_first_not_of( basic_string_view( s, count ), pos );
    }

    nssv_constexpr size_type find_first_not_of( CharT const * s, size_type pos = 0 ) const  // (4)
    {
        return find_first_not_of( basic_string_view( s ), pos );
    }

    // find_last_not_of(), 4x:

    nssv_constexpr size_type find_last_not_of( basic_string_view v, size_type pos = npos ) const nssv_noexcept  // (1)
    {
        return empty()
            ? npos
            : pos >= size()
            ? find_last_not_of( v, size() - 1 )
            : to_pos( std::find_if( const_reverse_iterator( cbegin() + pos + 1 ), crend(), not_in_view( v ) ) );
    }

    nssv_constexpr size_type find_last_not_of( CharT c, size_type pos = npos ) const nssv_noexcept  // (2)
    {
        return find_last_not_of( basic_string_view( &c, 1 ), pos );
    }

    nssv_constexpr size_type find_last_not_of( CharT const * s, size_type pos, size_type count ) const  // (3)
    {
        return find_last_not_of( basic_string_view( s, count ), pos );
    }

    nssv_constexpr size_type find_last_not_of( CharT const * s, size_type pos = npos ) const  // (4)
    {
        return find_last_not_of( basic_string_view( s ), pos );
    }

    // Constants:

#if nssv_CPP17_OR_GREATER
    static nssv_constexpr size_type npos = size_type(-1);
#elif nssv_CPP11_OR_GREATER
    enum : size_type { npos = size_type(-1) };
#else
    enum { npos = size_type(-1) };
#endif

private:
    struct not_in_view
    {
        const basic_string_view v;

        nssv_constexpr explicit not_in_view( basic_string_view v_ ) : v( v_ ) {}

        nssv_constexpr bool operator()( CharT c ) const
        {
            return npos == v.find_first_of( c );
        }
    };

    nssv_constexpr size_type to_pos( const_iterator it ) const
    {
        return it == cend() ? npos : size_type( it - cbegin() );
    }

    nssv_constexpr size_type to_pos( const_reverse_iterator it ) const
    {
        return it == crend() ? npos : size_type( crend() - it - 1 );
    }

    nssv_constexpr const_reference data_at( size_type pos ) const
    {
#if nssv_BETWEEN( nssv_COMPILER_GNUC_VERSION, 1, 500 )
        return data_[pos];
#else
        return assert( pos < size() ), data_[pos];
#endif
    }

private:
    const_pointer data_;
    size_type     size_;

public:
#if nssv_CONFIG_CONVERSION_STD_STRING_CLASS_METHODS

    template< class Allocator >
    basic_string_view( std::basic_string<CharT, Traits, Allocator> const & s ) nssv_noexcept
        : data_( s.data() )
        , size_( s.size() )
    {}

#if nssv_HAVE_EXPLICIT_CONVERSION

    template< class Allocator >
    explicit operator std::basic_string<CharT, Traits, Allocator>() const
    {
        return to_string( Allocator() );
    }

#endif // nssv_HAVE_EXPLICIT_CONVERSION

#if nssv_CPP11_OR_GREATER

    template< class Allocator = std::allocator<CharT> >
    std::basic_string<CharT, Traits, Allocator>
    to_string( Allocator const & a = Allocator() ) const
    {
        return std::basic_string<CharT, Traits, Allocator>( begin(), end(), a );
    }

#else

    std::basic_string<CharT, Traits>
    to_string() const
    {
        return std::basic_string<CharT, Traits>( begin(), end() );
    }

    template< class Allocator >
    std::basic_string<CharT, Traits, Allocator>
    to_string( Allocator const & a ) const
    {
        return std::basic_string<CharT, Traits, Allocator>( begin(), end(), a );
    }

#endif // nssv_CPP11_OR_GREATER

#endif // nssv_CONFIG_CONVERSION_STD_STRING_CLASS_METHODS
};

//
// Non-member functions:
//

// 24.4.3 Non-member comparison functions:
// lexicographically compare two string views (function template):

template< class CharT, class Traits >
nssv_constexpr bool operator== (
    basic_string_view <CharT, Traits> lhs,
    basic_string_view <CharT, Traits> rhs ) nssv_noexcept
{ return lhs.size() == rhs.size() && lhs.compare( rhs ) == 0; }

template< class CharT, class Traits >
nssv_constexpr bool operator!= (
    basic_string_view <CharT, Traits> lhs,
    basic_string_view <CharT, Traits> rhs ) nssv_noexcept
{ return !( lhs == rhs ); }

template< class CharT, class Traits >
nssv_constexpr bool operator< (
    basic_string_view <CharT, Traits> lhs,
    basic_string_view <CharT, Traits> rhs ) nssv_noexcept
{ return lhs.compare( rhs ) < 0; }

template< class CharT, class Traits >
nssv_constexpr bool operator<= (
    basic_string_view <CharT, Traits> lhs,
    basic_string_view <CharT, Traits> rhs ) nssv_noexcept
{ return lhs.compare( rhs ) <= 0; }

template< class CharT, class Traits >
nssv_constexpr bool operator> (
    basic_string_view <CharT, Traits> lhs,
    basic_string_view <CharT, Traits> rhs ) nssv_noexcept
{ return lhs.compare( rhs ) > 0; }

template< class CharT, class Traits >
nssv_constexpr bool operator>= (
    basic_string_view <CharT, Traits> lhs,
    basic_string_view <CharT, Traits> rhs ) nssv_noexcept
{ return lhs.compare( rhs ) >= 0; }

// Let S be basic_string_view<CharT, Traits>, and sv be an instance of S.
// Implementations shall provide sufficient additional overloads marked
// constexpr and noexcept so that an object t with an implicit conversion
// to S can be compared according to Table 67.

#if ! nssv_CPP11_OR_GREATER || nssv_BETWEEN( nssv_COMPILER_MSVC_VERSION, 100, 141 )

// accommodate for older compilers:

// ==

template< class CharT, class Traits>
nssv_constexpr bool operator==(
    basic_string_view<CharT, Traits> lhs,
    CharT const * rhs ) nssv_noexcept
{ return lhs.size() == detail::length( rhs ) && lhs.compare( rhs ) == 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator==(
    CharT const * lhs,
    basic_string_view<CharT, Traits> rhs ) nssv_noexcept
{ return detail::length( lhs ) == rhs.size() && rhs.compare( lhs ) == 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator==(
    basic_string_view<CharT, Traits> lhs,
    std::basic_string<CharT, Traits> rhs ) nssv_noexcept
{ return lhs.size() == rhs.size() && lhs.compare( rhs ) == 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator==(
    std::basic_string<CharT, Traits> rhs,
    basic_string_view<CharT, Traits> lhs ) nssv_noexcept
{ return lhs.size() == rhs.size() && lhs.compare( rhs ) == 0; }

// !=

template< class CharT, class Traits>
nssv_constexpr bool operator!=(
    basic_string_view<CharT, Traits> lhs,
    CharT const * rhs ) nssv_noexcept
{ return !( lhs == rhs ); }

template< class CharT, class Traits>
nssv_constexpr bool operator!=(
    CharT const * lhs,
    basic_string_view<CharT, Traits> rhs ) nssv_noexcept
{ return !( lhs == rhs ); }

template< class CharT, class Traits>
nssv_constexpr bool operator!=(
    basic_string_view<CharT, Traits> lhs,
    std::basic_string<CharT, Traits> rhs ) nssv_noexcept
{ return !( lhs == rhs ); }

template< class CharT, class Traits>
nssv_constexpr bool operator!=(
    std::basic_string<CharT, Traits> rhs,
    basic_string_view<CharT, Traits> lhs ) nssv_noexcept
{ return !( lhs == rhs ); }

// <

template< class CharT, class Traits>
nssv_constexpr bool operator<(
    basic_string_view<CharT, Traits> lhs,
    CharT const * rhs ) nssv_noexcept
{ return lhs.compare( rhs ) < 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator<(
    CharT const * lhs,
    basic_string_view<CharT, Traits> rhs ) nssv_noexcept
{ return rhs.compare( lhs ) > 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator<(
    basic_string_view<CharT, Traits> lhs,
    std::basic_string<CharT, Traits> rhs ) nssv_noexcept
{ return lhs.compare( rhs ) < 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator<(
    std::basic_string<CharT, Traits> rhs,
    basic_string_view<CharT, Traits> lhs ) nssv_noexcept
{ return rhs.compare( lhs ) > 0; }

// <=

template< class CharT, class Traits>
nssv_constexpr bool operator<=(
    basic_string_view<CharT, Traits> lhs,
    CharT const * rhs ) nssv_noexcept
{ return lhs.compare( rhs ) <= 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator<=(
    CharT const * lhs,
    basic_string_view<CharT, Traits> rhs ) nssv_noexcept
{ return rhs.compare( lhs ) >= 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator<=(
    basic_string_view<CharT, Traits> lhs,
    std::basic_string<CharT, Traits> rhs ) nssv_noexcept
{ return lhs.compare( rhs ) <= 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator<=(
    std::basic_string<CharT, Traits> rhs,
    basic_string_view<CharT, Traits> lhs ) nssv_noexcept
{ return rhs.compare( lhs ) >= 0; }

// >

template< class CharT, class Traits>
nssv_constexpr bool operator>(
    basic_string_view<CharT, Traits> lhs,
    CharT const * rhs ) nssv_noexcept
{ return lhs.compare( rhs ) > 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator>(
    CharT const * lhs,
    basic_string_view<CharT, Traits> rhs ) nssv_noexcept
{ return rhs.compare( lhs ) < 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator>(
    basic_string_view<CharT, Traits> lhs,
    std::basic_string<CharT, Traits> rhs ) nssv_noexcept
{ return lhs.compare( rhs ) > 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator>(
    std::basic_string<CharT, Traits> rhs,
    basic_string_view<CharT, Traits> lhs ) nssv_noexcept
{ return rhs.compare( lhs ) < 0; }

// >=

template< class CharT, class Traits>
nssv_constexpr bool operator>=(
    basic_string_view<CharT, Traits> lhs,
    CharT const * rhs ) nssv_noexcept
{ return lhs.compare( rhs ) >= 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator>=(
    CharT const * lhs,
    basic_string_view<CharT, Traits> rhs ) nssv_noexcept
{ return rhs.compare( lhs ) <= 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator>=(
    basic_string_view<CharT, Traits> lhs,
    std::basic_string<CharT, Traits> rhs ) nssv_noexcept
{ return lhs.compare( rhs ) >= 0; }

template< class CharT, class Traits>
nssv_constexpr bool operator>=(
    std::basic_string<CharT, Traits> rhs,
    basic_string_view<CharT, Traits> lhs ) nssv_noexcept
{ return rhs.compare( lhs ) <= 0; }

#else // newer compilers:

#define nssv_BASIC_STRING_VIEW_I(T,U)  typename std::decay< basic_string_view<T,U> >::type

#if defined(_MSC_VER)       // issue 40
# define nssv_MSVC_ORDER(x)  , int=x
#else
# define nssv_MSVC_ORDER(x)  /*, int=x*/
#endif

// ==

template< class CharT, class Traits  nssv_MSVC_ORDER(1) >
nssv_constexpr bool operator==(
         basic_string_view  <CharT, Traits> lhs,
    nssv_BASIC_STRING_VIEW_I(CharT, Traits) rhs ) nssv_noexcept
{ return lhs.size() == rhs.size() && lhs.compare( rhs ) == 0; }

template< class CharT, class Traits  nssv_MSVC_ORDER(2) >
nssv_constexpr bool operator==(
    nssv_BASIC_STRING_VIEW_I(CharT, Traits) lhs,
         basic_string_view  <CharT, Traits> rhs ) nssv_noexcept
{ return lhs.size() == rhs.size() && lhs.compare( rhs ) == 0; }

// !=

template< class CharT, class Traits  nssv_MSVC_ORDER(1) >
nssv_constexpr bool operator!= (
         basic_string_view  < CharT, Traits > lhs,
    nssv_BASIC_STRING_VIEW_I( CharT, Traits ) rhs ) nssv_noexcept
{ return !( lhs == rhs ); }

template< class CharT, class Traits  nssv_MSVC_ORDER(2) >
nssv_constexpr bool operator!= (
    nssv_BASIC_STRING_VIEW_I( CharT, Traits ) lhs,
         basic_string_view  < CharT, Traits > rhs ) nssv_noexcept
{ return !( lhs == rhs ); }

// <

template< class CharT, class Traits  nssv_MSVC_ORDER(1) >
nssv_constexpr bool operator< (
         basic_string_view  < CharT, Traits > lhs,
    nssv_BASIC_STRING_VIEW_I( CharT, Traits ) rhs ) nssv_noexcept
{ return lhs.compare( rhs ) < 0; }

template< class CharT, class Traits  nssv_MSVC_ORDER(2) >
nssv_constexpr bool operator< (
    nssv_BASIC_STRING_VIEW_I( CharT, Traits ) lhs,
         basic_string_view  < CharT, Traits > rhs ) nssv_noexcept
{ return lhs.compare( rhs ) < 0; }

// <=

template< class CharT, class Traits  nssv_MSVC_ORDER(1) >
nssv_constexpr bool operator<= (
         basic_string_view  < CharT, Traits > lhs,
    nssv_BASIC_STRING_VIEW_I( CharT, Traits ) rhs ) nssv_noexcept
{ return lhs.compare( rhs ) <= 0; }

template< class CharT, class Traits  nssv_MSVC_ORDER(2) >
nssv_constexpr bool operator<= (
    nssv_BASIC_STRING_VIEW_I( CharT, Traits ) lhs,
         basic_string_view  < CharT, Traits > rhs ) nssv_noexcept
{ return lhs.compare( rhs ) <= 0; }

// >

template< class CharT, class Traits  nssv_MSVC_ORDER(1) >
nssv_constexpr bool operator> (
         basic_string_view  < CharT, Traits > lhs,
    nssv_BASIC_STRING_VIEW_I( CharT, Traits ) rhs ) nssv_noexcept
{ return lhs.compare( rhs ) > 0; }

template< class CharT, class Traits  nssv_MSVC_ORDER(2) >
nssv_constexpr bool operator> (
    nssv_BASIC_STRING_VIEW_I( CharT, Traits ) lhs,
         basic_string_view  < CharT, Traits > rhs ) nssv_noexcept
{ return lhs.compare( rhs ) > 0; }

// >=

template< class CharT, class Traits  nssv_MSVC_ORDER(1) >
nssv_constexpr bool operator>= (
         basic_string_view  < CharT, Traits > lhs,
    nssv_BASIC_STRING_VIEW_I( CharT, Traits ) rhs ) nssv_noexcept
{ return lhs.compare( rhs ) >= 0; }

template< class CharT, class Traits  nssv_MSVC_ORDER(2) >
nssv_constexpr bool operator>= (
    nssv_BASIC_STRING_VIEW_I( CharT, Traits ) lhs,
         basic_string_view  < CharT, Traits > rhs ) nssv_noexcept
{ return lhs.compare( rhs ) >= 0; }

#undef nssv_MSVC_ORDER
#undef nssv_BASIC_STRING_VIEW_I

#endif // compiler-dependent approach to comparisons

// 24.4.4 Inserters and extractors:

#if ! nssv_CONFIG_NO_STREAM_INSERTION

namespace detail {

template< class Stream >
void write_padding( Stream & os, std::streamsize n )
{
    for ( std::streamsize i = 0; i < n; ++i )
        os.rdbuf()->sputc( os.fill() );
}

template< class Stream, class View >
Stream & write_to_stream( Stream & os, View const & sv )
{
    typename Stream::sentry sentry( os );

    if ( !sentry )
        return os;

    const std::streamsize length = static_cast<std::streamsize>( sv.length() );

    // Whether, and how, to pad:
    const bool      pad = ( length < os.width() );
    const bool left_pad = pad && ( os.flags() & std::ios_base::adjustfield ) == std::ios_base::right;

    if ( left_pad )
        write_padding( os, os.width() - length );

    // Write span characters:
    os.rdbuf()->sputn( sv.begin(), length );

    if ( pad && !left_pad )
        write_padding( os, os.width() - length );

    // Reset output stream width:
    os.width( 0 );

    return os;
}

} // namespace detail

template< class CharT, class Traits >
std::basic_ostream<CharT, Traits> &
operator<<(
    std::basic_ostream<CharT, Traits>& os,
    basic_string_view <CharT, Traits> sv )
{
    return detail::write_to_stream( os, sv );
}

#endif // nssv_CONFIG_NO_STREAM_INSERTION

// Several typedefs for common character types are provided:

typedef basic_string_view<char>      string_view;
typedef basic_string_view<wchar_t>   wstring_view;
#if nssv_HAVE_WCHAR16_T
typedef basic_string_view<char16_t>  u16string_view;
typedef basic_string_view<char32_t>  u32string_view;
#endif

}} // namespace nonstd::sv_lite

//
// 24.4.6 Suffix for basic_string_view literals:
//

#if nssv_HAVE_USER_DEFINED_LITERALS

namespace nonstd {
nssv_inline_ns namespace literals {
nssv_inline_ns namespace string_view_literals {

#if nssv_CONFIG_STD_SV_OPERATOR && nssv_HAVE_STD_DEFINED_LITERALS

nssv_constexpr nonstd::sv_lite::string_view operator""sv( const char* str, size_t len ) nssv_noexcept  // (1)
{
    return nonstd::sv_lite::string_view{ str, len };
}

nssv_constexpr nonstd::sv_lite::u16string_view operator""sv( const char16_t* str, size_t len ) nssv_noexcept  // (2)
{
    return nonstd::sv_lite::u16string_view{ str, len };
}

nssv_constexpr nonstd::sv_lite::u32string_view operator""sv( const char32_t* str, size_t len ) nssv_noexcept  // (3)
{
    return nonstd::sv_lite::u32string_view{ str, len };
}

nssv_constexpr nonstd::sv_lite::wstring_view operator""sv( const wchar_t* str, size_t len ) nssv_noexcept  // (4)
{
    return nonstd::sv_lite::wstring_view{ str, len };
}

#endif // nssv_CONFIG_STD_SV_OPERATOR && nssv_HAVE_STD_DEFINED_LITERALS

#if nssv_CONFIG_USR_SV_OPERATOR

nssv_constexpr nonstd::sv_lite::string_view operator""_sv( const char* str, size_t len ) nssv_noexcept  // (1)
{
    return nonstd::sv_lite::string_view{ str, len };
}

nssv_constexpr nonstd::sv_lite::u16string_view operator""_sv( const char16_t* str, size_t len ) nssv_noexcept  // (2)
{
    return nonstd::sv_lite::u16string_view{ str, len };
}

nssv_constexpr nonstd::sv_lite::u32string_view operator""_sv( const char32_t* str, size_t len ) nssv_noexcept  // (3)
{
    return nonstd::sv_lite::u32string_view{ str, len };
}

nssv_constexpr nonstd::sv_lite::wstring_view operator""_sv( const wchar_t* str, size_t len ) nssv_noexcept  // (4)
{
    return nonstd::sv_lite::wstring_view{ str, len };
}

#endif // nssv_CONFIG_USR_SV_OPERATOR

}}} // namespace nonstd::literals::string_view_literals

#endif

//
// Extensions for std::string:
//

#if nssv_CONFIG_CONVERSION_STD_STRING_FREE_FUNCTIONS

namespace nonstd {
namespace sv_lite {

// Exclude MSVC 14 (19.00): it yields ambiguous to_string():

#if nssv_CPP11_OR_GREATER && nssv_COMPILER_MSVC_VERSION != 140

template< class CharT, class Traits, class Allocator = std::allocator<CharT> >
std::basic_string<CharT, Traits, Allocator>
to_string( basic_string_view<CharT, Traits> v, Allocator const & a = Allocator() )
{
    return std::basic_string<CharT,Traits, Allocator>( v.begin(), v.end(), a );
}

#else

template< class CharT, class Traits >
std::basic_string<CharT, Traits>
to_string( basic_string_view<CharT, Traits> v )
{
    return std::basic_string<CharT, Traits>( v.begin(), v.end() );
}

template< class CharT, class Traits, class Allocator >
std::basic_string<CharT, Traits, Allocator>
to_string( basic_string_view<CharT, Traits> v, Allocator const & a )
{
    return std::basic_string<CharT, Traits, Allocator>( v.begin(), v.end(), a );
}

#endif // nssv_CPP11_OR_GREATER

template< class CharT, class Traits, class Allocator >
basic_string_view<CharT, Traits>
to_string_view( std::basic_string<CharT, Traits, Allocator> const & s )
{
    return basic_string_view<CharT, Traits>( s.data(), s.size() );
}

}} // namespace nonstd::sv_lite

#endif // nssv_CONFIG_CONVERSION_STD_STRING_FREE_FUNCTIONS

//
// make types and algorithms available in namespace nonstd:
//

namespace nonstd {

using sv_lite::basic_string_view;
using sv_lite::string_view;
using sv_lite::wstring_view;

#if nssv_HAVE_WCHAR16_T
using sv_lite::u16string_view;
#endif
#if nssv_HAVE_WCHAR32_T
using sv_lite::u32string_view;
#endif

// literal "sv"

using sv_lite::operator==;
using sv_lite::operator!=;
using sv_lite::operator<;
using sv_lite::operator<=;
using sv_lite::operator>;
using sv_lite::operator>=;

#if ! nssv_CONFIG_NO_STREAM_INSERTION
using sv_lite::operator<<;
#endif

#if nssv_CONFIG_CONVERSION_STD_STRING_FREE_FUNCTIONS
using sv_lite::to_string;
using sv_lite::to_string_view;
#endif

} // namespace nonstd

// 24.4.5 Hash support (C++11):

// Note: The hash value of a string view object is equal to the hash value of
// the corresponding string object.

#if nssv_HAVE_STD_HASH

#include <functional>

namespace std {

template<>
struct hash< nonstd::string_view >
{
public:
    std::size_t operator()( nonstd::string_view v ) const nssv_noexcept
    {
        return std::hash<std::string>()( std::string( v.data(), v.size() ) );
    }
};

template<>
struct hash< nonstd::wstring_view >
{
public:
    std::size_t operator()( nonstd::wstring_view v ) const nssv_noexcept
    {
        return std::hash<std::wstring>()( std::wstring( v.data(), v.size() ) );
    }
};

template<>
struct hash< nonstd::u16string_view >
{
public:
    std::size_t operator()( nonstd::u16string_view v ) const nssv_noexcept
    {
        return std::hash<std::u16string>()( std::u16string( v.data(), v.size() ) );
    }
};

template<>
struct hash< nonstd::u32string_view >
{
public:
    std::size_t operator()( nonstd::u32string_view v ) const nssv_noexcept
    {
        return std::hash<std::u32string>()( std::u32string( v.data(), v.size() ) );
    }
};

} // namespace std

#endif // nssv_HAVE_STD_HASH

nssv_RESTORE_WARNINGS()

#endif // nssv_HAVE_STD_STRING_VIEW
#endif // NONSTD_SV_LITE_H_INCLUDED

#endif

#ifdef CSV_HAS_CXX20
#include <ranges>
#endif

namespace csv {
#ifdef _MSC_VER
#pragma region Compatibility Macros
#endif
    /**
     *  @def IF_CONSTEXPR
     *  Expands to `if constexpr` in C++17 and `if` otherwise
     *
     *  @def CONSTEXPR_VALUE
     *  Expands to `constexpr` in C++17 and `const` otherwise.
     *  Mainly used for global variables.
     *
     *  @def CONSTEXPR
     *  Expands to `constexpr` in decent compilers and `inline` otherwise.
     *  Intended for functions and methods.
     */

// Allows static assertions without specifying a message
#define STATIC_ASSERT(x) static_assert(x, "Assertion failed")

#ifdef NDEBUG
    #define CSV_DEBUG_ASSERT(x) ((void)sizeof(x), (void)0)
#else
    #define CSV_DEBUG_ASSERT(x) assert(x)
#endif

#ifdef CSV_HAS_CXX17
     /** @typedef string_view
      *  The string_view class used by this library.
      */
    using string_view = std::string_view;
#else
     /** @typedef string_view
      *  The string_view class used by this library.
      */
    using string_view = nonstd::string_view;
#endif

#ifdef CSV_HAS_CXX17
    #define IF_CONSTEXPR if constexpr
    #define CONSTEXPR_VALUE constexpr

    #define CONSTEXPR_17 constexpr
#else
    #define IF_CONSTEXPR if
    #define CONSTEXPR_VALUE const

    #define CONSTEXPR_17 inline
#endif

#ifdef CSV_HAS_CXX14
    template<bool B, class T = void>
    using enable_if_t = std::enable_if_t<B, T>;

    #define CONSTEXPR_14 constexpr
    #define CONSTEXPR_VALUE_14 constexpr
#else
    template<bool B, class T = void>
    using enable_if_t = typename std::enable_if<B, T>::type;

    #define CONSTEXPR_14 inline
    #define CONSTEXPR_VALUE_14 const
#endif

    namespace internals {
        template<bool B, class T = void>
        using enable_if_t = csv::enable_if_t<B, T>;
    }

#ifdef CSV_HAS_CXX17
    template<typename F, typename... Args>
    using invoke_result_t = typename std::invoke_result<F, Args...>::type;
#else
    template<typename F, typename... Args>
    using invoke_result_t = typename std::result_of<F(Args...)>::type;
#endif

    template<typename... Ts>
    using void_t = void;

    template<typename F, typename ReturnType, typename Enable, typename... Args>
    struct is_invocable_returning_impl : std::false_type {};

    template<typename F, typename ReturnType, typename... Args>
    struct is_invocable_returning_impl<
        F,
        ReturnType,
        void_t<invoke_result_t<F, Args...>>,
        Args...
    > : std::integral_constant<
        bool,
        std::is_convertible<invoke_result_t<F, Args...>, ReturnType>::value
    > {};

    template<typename F, typename ReturnType, typename... Args>
    struct is_invocable_returning : is_invocable_returning_impl<F, ReturnType, void, Args...> {};

    // Resolves g++ bug with regard to constexpr methods.
    // Keep this gated to C++17+, since C++11/14 pedantic mode rejects constexpr
    // non-static members when the enclosing class is non-literal.
    // See: https://stackoverflow.com/questions/36489369/constexpr-non-static-member-function-with-non-constexpr-constructor-gcc-clang-d
#if defined(__GNUC__) && !defined(__clang__)
    #if defined(CSV_HAS_CXX17) && (((__GNUC__ == 7) && (__GNUC_MINOR__ >= 2)) || (__GNUC__ >= 8))
        #define CONSTEXPR constexpr
    #endif
#else
    #ifdef CSV_HAS_CXX17
        #define CONSTEXPR constexpr
    #endif
#endif

#ifndef CONSTEXPR
#define CONSTEXPR inline
#endif

#ifdef _MSC_VER
#pragma endregion
#endif

    namespace internals {
        template<typename T>
        class is_hashable {
        private:
            template<typename U>
            static auto test(int) -> decltype(
                std::hash<U>{}(std::declval<const U&>()),
                std::true_type{}
            );

            template<typename>
            static std::false_type test(...);

        public:
            static constexpr bool value = decltype(test<T>(0))::value;
        };

        template<typename T>
        class is_equality_comparable {
        private:
            template<typename U>
            static auto test(int) -> decltype(
                std::declval<const U&>() == std::declval<const U&>(),
                std::true_type{}
            );

            template<typename>
            static std::false_type test(...);

        public:
            static constexpr bool value = decltype(test<T>(0))::value;
        };

        template<typename T>
        class lazy_shared_ptr {
        public:
            lazy_shared_ptr() = default;
            lazy_shared_ptr(const lazy_shared_ptr&) = delete;
            lazy_shared_ptr& operator=(const lazy_shared_ptr&) = delete;

            lazy_shared_ptr(lazy_shared_ptr&& other) noexcept : value_(std::move(other.value_)) {}

            lazy_shared_ptr& operator=(lazy_shared_ptr&& other) noexcept {
                if (this != &other) {
                    value_ = std::move(other.value_);
                }

                return *this;
            }

            template<typename Factory>
            T& get_or_create(Factory&& factory) const {
#if CSV_ENABLE_THREADS
                std::call_once(init_once_, [this, &factory]() {
                    value_ = factory();
                });
#else
                if (!value_) {
                    value_ = factory();
                }
#endif
                return *value_;
            }

            T* get() const noexcept {
                return value_.get();
            }

        private:
            mutable std::shared_ptr<T> value_ = nullptr;
#if CSV_ENABLE_THREADS
            mutable std::once_flag init_once_;
#endif
        };

        #ifdef CSV_HAS_CXX20
        #ifdef _MSC_VER
        #pragma region CXX20 Concepts
        #endif

        template<typename T>
        concept csv_string_field_range =
            std::ranges::input_range<std::remove_reference_t<T>>
            && std::convertible_to<
                std::ranges::range_reference_t<std::remove_reference_t<T>>,
                csv::string_view
            >;

        template<typename T>
        concept has_to_sv_range = requires(const std::remove_reference_t<T>& value) {
            { value.to_sv_range() } -> std::ranges::input_range;
            requires std::convertible_to<
                std::ranges::range_reference_t<decltype(value.to_sv_range())>,
                csv::string_view
            >;
        };

        template<typename T>
        concept csv_row_like = csv_string_field_range<T> || has_to_sv_range<T>;

        template<typename T>
        concept csv_write_rows_input_range =
            std::ranges::input_range<std::remove_reference_t<T>>
            && csv_row_like<
                std::ranges::range_reference_t<std::remove_reference_t<T>>
            >;

        #ifdef _MSC_VER
        #pragma endregion
        #endif
        #endif

        // PAGE_SIZE macro could be already defined by the host system.
#if defined(PAGE_SIZE)
#undef PAGE_SIZE
#endif

// Get operating system specific details
#if defined(_WIN32)
        inline int getpagesize() {
            _SYSTEM_INFO sys_info = {};
            GetSystemInfo(&sys_info);
            return std::max(sys_info.dwPageSize, sys_info.dwAllocationGranularity);
        }

        const int PAGE_SIZE = getpagesize();
#elif defined(__linux__) 
        const int PAGE_SIZE = getpagesize();
#else
        /** Size of a memory page in bytes. Used by
         *  csv::internals::CSVFieldArray when allocating blocks.
         */
        const int PAGE_SIZE = 4096;
#endif

        /** Default chunk size for lazy-loading large CSV files
         * 
         * The worker thread reads this many bytes at a time by default (10MB).
         * 
         * CRITICAL INVARIANT: Field boundaries at chunk transitions must be preserved.
         * Bug #280 was caused by fields spanning chunk boundaries being corrupted.
         * 
         * @note Tests must write >10MB of data to cross chunk boundaries
         * @see parser/mmap.cpp MmapParser::next() for chunk transition logic
         */
        constexpr size_t CSV_CHUNK_SIZE_DEFAULT = 10000000; // 10MB

        /** Type used to represent the location of a CSV byte within a larger chunk. */
        typedef std::uint32_t CSVChunkIndex;

        CONSTEXPR_VALUE_14 CSVChunkIndex CSV_CHUNK_INDEX_MAX = (std::numeric_limits<CSVChunkIndex>::max)();

        CONSTEXPR_VALUE_14 size_t CSV_CHUNK_SIZE_MAX = CSV_CHUNK_INDEX_MAX;

        /** Minimum supported custom chunk size for CSVFormat::chunk_size().
         *
         * This lower bound allows memory-constrained environments to reduce parser
         * buffer size while avoiding pathological tiny-buffer overhead.
         */
        constexpr size_t CSV_CHUNK_SIZE_FLOOR = 500 * 1024; // 500KB

        /** Default minimum source size before speculative parallel parsing is considered. */
        constexpr size_t CSV_SPECULATIVE_PARALLEL_MIN_BYTES = 50ull * 1024ull * 1024ull; // 50MB

        template<typename T>
        inline bool is_equal(T a, T b, T epsilon = 0.001) {
            /** Returns true if two floating point values are about the same */
            static_assert(std::is_floating_point<T>::value, "T must be a floating point type.");
            return std::abs(a - b) < epsilon;
        }

        /**  @typedef ParseFlags
         *   An enum used for describing the significance of each character
         *   with respect to CSV parsing
         *
         *   @see quote_escape_flag
         */
        enum class ParseFlags {
            QUOTE_ESCAPE_QUOTE = 0, /**< A quote inside or terminating a quote_escaped field */
            QUOTE = 2 | 1,          /**< Characters which may signify a quote escape */
            NOT_SPECIAL = 4,        /**< Characters with no special meaning or escaped delimiters and newlines */
            DELIMITER = 4 | 1,      /**< Characters which signify a new field */
            CARRIAGE_RETURN = 4 | 2, /**< Characters which signify a carriage return */
            NEWLINE = 4 | 2 | 1     /**< Characters which signify a new row */
        };

        /** Transform the ParseFlags given the context of whether or not the current
         *  field is quote escaped */
        constexpr ParseFlags quote_escape_flag(ParseFlags flag, bool quote_escape) noexcept {
            return (ParseFlags)((int)flag & ~((int)ParseFlags::QUOTE * quote_escape));
        }

        // Assumed to be true by parsing functions: allows for testing
        // if an item is DELIMITER or NEWLINE with a >= statement
        STATIC_ASSERT(ParseFlags::DELIMITER < ParseFlags::CARRIAGE_RETURN);
        STATIC_ASSERT(ParseFlags::DELIMITER < ParseFlags::NEWLINE);
        STATIC_ASSERT(ParseFlags::CARRIAGE_RETURN < ParseFlags::NEWLINE);

        /** Optimizations for reducing branching in parsing loop
         *
         *  Idea: The meaning of all non-quote characters changes depending
         *  on whether or not the parser is in a quote-escaped mode (0 or 1)
         */
        STATIC_ASSERT(quote_escape_flag(ParseFlags::NOT_SPECIAL, false) == ParseFlags::NOT_SPECIAL);
        STATIC_ASSERT(quote_escape_flag(ParseFlags::QUOTE, false) == ParseFlags::QUOTE);
        STATIC_ASSERT(quote_escape_flag(ParseFlags::DELIMITER, false) == ParseFlags::DELIMITER);
        STATIC_ASSERT(quote_escape_flag(ParseFlags::CARRIAGE_RETURN, false) == ParseFlags::CARRIAGE_RETURN);
        STATIC_ASSERT(quote_escape_flag(ParseFlags::NEWLINE, false) == ParseFlags::NEWLINE);

        STATIC_ASSERT(quote_escape_flag(ParseFlags::NOT_SPECIAL, true) == ParseFlags::NOT_SPECIAL);
        STATIC_ASSERT(quote_escape_flag(ParseFlags::QUOTE, true) == ParseFlags::QUOTE_ESCAPE_QUOTE);
        STATIC_ASSERT(quote_escape_flag(ParseFlags::DELIMITER, true) == ParseFlags::NOT_SPECIAL);
        STATIC_ASSERT(quote_escape_flag(ParseFlags::CARRIAGE_RETURN, true) == ParseFlags::NOT_SPECIAL);
        STATIC_ASSERT(quote_escape_flag(ParseFlags::NEWLINE, true) == ParseFlags::NOT_SPECIAL);

        /** An array which maps ASCII chars to a parsing flag */
        using ParseFlagMap = std::array<ParseFlags, 256>;

        /** An array which maps ASCII chars to a flag indicating if it is whitespace */
        using WhitespaceMap = std::array<bool, 256>;
    }

    /** Integer indicating a requested column wasn't found. */
    constexpr int CSV_NOT_FOUND = -1;

    /** Offset to convert char into array index. */
    constexpr unsigned CHAR_OFFSET = std::numeric_limits<char>::is_signed ? 128 : 0;
}


namespace csv {
    namespace internals {
        CONSTEXPR_VALUE_14 char ERROR_CANNOT_OPEN_FILE[] = "Cannot open file ";
        CONSTEXPR_VALUE_14 char ERROR_FAILED_OPEN_WRITE[] = "Failed to open file for writing: ";
        CONSTEXPR_VALUE_14 char ERROR_STREAM_READ_FAILURE[] = "StreamParser read failure";
        CONSTEXPR_VALUE_14 char ERROR_UNSUPPORTED_ENCODING_SUFFIX[] =
            " encoded CSV input is not supported directly. Please transcode to UTF-8 before parsing.";
        CONSTEXPR_VALUE_14 char ERROR_ROW_TOO_SHORT[] = "Line too short ";
        CONSTEXPR_VALUE_14 char ERROR_ROW_TOO_LONG[] = "Line too long ";
        CONSTEXPR_VALUE_14 char ERROR_ROW_LARGER_THAN_CHUNK_PREFIX[] =
            "End of file not reached and no more records parsed. "
            "This likely indicates a CSV row larger than the chunk size of ";
        CONSTEXPR_VALUE_14 char ERROR_ROW_LARGER_THAN_CHUNK_SUFFIX[] =
            " bytes. Use CSVFormat::chunk_size() to increase the chunk size.";
        CONSTEXPR_VALUE_14 char ERROR_COLUMN_NOT_FOUND[] = "Column not found: ";
        CONSTEXPR_VALUE_14 char ERROR_COLUMN_INDEX_OUT_OF_BOUNDS[] = "Column index out of bounds.";
        CONSTEXPR_VALUE_14 char ERROR_COLUMN_INDEX_OUT_OF_RANGE[] = "Column index out of range.";
        CONSTEXPR_VALUE_14 char CSV_ERROR_INDEX_OUT_OF_BOUNDS[] = "Index out of bounds.";
        CONSTEXPR_VALUE_14 char ERROR_CANNOT_EDIT_CONST_DF_CELL[] = "Cannot edit a const DataFrame cell.";
        CONSTEXPR_VALUE_14 char ERROR_CANNOT_ERASE_CONST_DF_ROW[] = "Cannot erase a const DataFrame row.";
        CONSTEXPR_VALUE_14 char ERROR_COLUMN_APPLY_STATE_COUNT[] =
            "column_parallel_apply() requires one state object per column.";
        CONSTEXPR_VALUE_14 char ERROR_COLUMN_APPLY_SUBSET_STATE_COUNT[] =
            "column_parallel_apply() subset overload requires one state object per selected column.";
        CONSTEXPR_VALUE_14 char ERROR_COLUMN_APPLY_INVALID_INDEX[] =
            "column_parallel_apply() subset overload received an invalid column index.";
        CONSTEXPR_VALUE_14 char ERROR_KEY_COLUMN_EMPTY[] = "Key column cannot be empty.";
        CONSTEXPR_VALUE_14 char ERROR_KEY_COLUMN_NOT_FOUND[] = "Key column not found: ";
        CONSTEXPR_VALUE_14 char ERROR_KEY_COLUMN_VALUE[] = "Error retrieving key column value: ";
        CONSTEXPR_VALUE_14 char ERROR_DUPLICATE_KEY[] = "Duplicate key encountered.";
        CONSTEXPR_VALUE_14 char ERROR_UNKEYED_DATA_FRAME[] =
            "This DataFrame was created without a key column.";
        CONSTEXPR_VALUE_14 char ERROR_KEY_NOT_FOUND[] = "Key not found.";
        CONSTEXPR_VALUE_14 char ERROR_CHUNK_PARALLEL_APPLY_ZERO[] =
            "chunk_parallel_apply() requires a non-zero chunk size.";
        CONSTEXPR_VALUE_14 char ERROR_READER_NULL_STREAM[] = "CSVReader requires a non-null stream";
        CONSTEXPR_VALUE_14 char ERROR_MULTIPLE_DELIMITERS[] =
            "There is more than one possible delimiter.";
        CONSTEXPR_VALUE_14 char ERROR_CHUNK_SIZE_FLOOR_PREFIX[] = "Chunk size must be at least ";
        CONSTEXPR_VALUE_14 char ERROR_CHUNK_SIZE_FLOOR_MIDDLE[] = " bytes (500KB). Provided: ";
        CONSTEXPR_VALUE_14 char ERROR_CHUNK_SIZE_CEILING_PREFIX[] = "Chunk size must fit in uint32_t. Maximum: ";
        CONSTEXPR_VALUE_14 char ERROR_CHUNK_SIZE_CEILING_MIDDLE[] = ". Provided: ";
        CONSTEXPR_VALUE_14 char ERROR_CHAR_OVERLAP_PREFIX[] =
            "There should be no overlap between the quote character, "
            "the set of possible delimiters "
            "and the set of whitespace characters. Offending characters: ";

        inline std::string make_prefixed_message(const char* prefix, csv::string_view value) {
            return std::string(prefix) + std::string(value);
        }

        inline std::string make_unsupported_encoding_message(const char* encoding) {
            return std::string(encoding) + ERROR_UNSUPPORTED_ENCODING_SUFFIX;
        }

        inline std::string make_chunk_size_error(size_t floor, size_t provided) {
            return std::string(ERROR_CHUNK_SIZE_FLOOR_PREFIX)
                + std::to_string(floor)
                + ERROR_CHUNK_SIZE_FLOOR_MIDDLE
                + std::to_string(provided);
        }

        inline std::string make_chunk_size_ceiling_error(size_t ceiling, size_t provided) {
            return std::string(ERROR_CHUNK_SIZE_CEILING_PREFIX)
                + std::to_string(ceiling)
                + ERROR_CHUNK_SIZE_CEILING_MIDDLE
                + std::to_string(provided);
        }

        inline std::string make_row_larger_than_chunk_message(size_t chunk_size) {
            return std::string(ERROR_ROW_LARGER_THAN_CHUNK_PREFIX)
                + std::to_string(chunk_size)
                + ERROR_ROW_LARGER_THAN_CHUNK_SUFFIX;
        }

        inline std::string make_mmap_failure_message(
            const std::string& filename,
            size_t offset,
            size_t length
        ) {
            return "Memory mapping failed during CSV parsing: file='" + filename
                + "' offset=" + std::to_string(offset)
                + " length=" + std::to_string(length);
        }

        inline std::string make_char_overlap_error(const std::set<char>& offenders) {
            std::string err_msg = ERROR_CHAR_OVERLAP_PREFIX;

            size_t i = 0;
            for (std::set<char>::const_iterator it = offenders.begin(); it != offenders.end(); ++it, ++i) {
                err_msg += "'";
                err_msg += *it;
                err_msg += "'";

                if (i + 1 < offenders.size()) {
                    err_msg += ", ";
                }
            }

            err_msg += '.';
            return err_msg;
        }

        [[noreturn]] inline void throw_cannot_open_file(csv::string_view filename) {
            throw std::runtime_error(make_prefixed_message(ERROR_CANNOT_OPEN_FILE, filename));
        }

        [[noreturn]] inline void throw_failed_open_for_writing(const std::string& filename) {
            throw std::runtime_error(std::string(ERROR_FAILED_OPEN_WRITE) + filename);
        }

        [[noreturn]] inline void throw_unsupported_encoding(const char* encoding) {
            throw std::runtime_error(make_unsupported_encoding_message(encoding));
        }

        [[noreturn]] inline void throw_stream_read_failure() {
            throw std::runtime_error(ERROR_STREAM_READ_FAILURE);
        }

        [[noreturn]] inline void throw_mmap_failure(
            const std::error_code& error,
            const std::string& filename,
            size_t offset,
            size_t length
        ) {
            throw std::system_error(error, make_mmap_failure_message(filename, offset, length));
        }

        [[noreturn]] inline void throw_row_too_large_for_chunk(size_t chunk_size) {
            throw std::runtime_error(make_row_larger_than_chunk_message(chunk_size));
        }

        [[noreturn]] inline void throw_line_too_short(csv::string_view raw_row) {
            throw std::runtime_error(make_prefixed_message(ERROR_ROW_TOO_SHORT, raw_row));
        }

        [[noreturn]] inline void throw_line_too_long(csv::string_view raw_row) {
            throw std::runtime_error(make_prefixed_message(ERROR_ROW_TOO_LONG, raw_row));
        }

        [[noreturn]] inline void throw_column_not_found(csv::string_view column) {
            throw std::runtime_error(make_prefixed_message(ERROR_COLUMN_NOT_FOUND, column));
        }

        [[noreturn]] inline void throw_column_not_found_out_of_range(csv::string_view column) {
            throw std::out_of_range(make_prefixed_message(ERROR_COLUMN_NOT_FOUND, column));
        }

        [[noreturn]] inline void throw_column_index_out_of_bounds() {
            throw std::out_of_range(ERROR_COLUMN_INDEX_OUT_OF_BOUNDS);
        }

        [[noreturn]] inline void throw_column_index_out_of_range() {
            throw std::out_of_range(ERROR_COLUMN_INDEX_OUT_OF_RANGE);
        }
    }
}

/** @file
 *  @brief Defines functionality needed for basic CSV parsing
 */


#include <algorithm>
#include <deque>
#include <exception>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

/** @file
 *  @brief CSV scalar type classification adapter.
 */


#include <cstdint>

/*
classify_scalar, version 1.1.0
https://github.com/vincentlaucsb/classify_scalar

MIT License

Copyright (c) 2026 Vincent La

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#if defined(CLASSIFY_SCALAR_VERSION)
#if CLASSIFY_SCALAR_VERSION >= 10100
#define CLASSIFY_SCALAR_SKIP_HEADER
#else
#error "A newer classify_scalar.hpp was included after an older copy. Include the newest copy first."
#endif
#else
#define CLASSIFY_SCALAR_VERSION_MAJOR 1
#define CLASSIFY_SCALAR_VERSION_MINOR 1
#define CLASSIFY_SCALAR_VERSION_PATCH 0
#define CLASSIFY_SCALAR_VERSION 10100
#endif

#ifndef CLASSIFY_SCALAR_SKIP_HEADER

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4127)
#endif

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <tuple>
#include <type_traits>

#if defined(_MSVC_LANG)
#define CLASSIFY_SCALAR_CPLUSPLUS _MSVC_LANG
#else
#define CLASSIFY_SCALAR_CPLUSPLUS __cplusplus
#endif

#if CLASSIFY_SCALAR_CPLUSPLUS >= 202002L
#define CLASSIFY_SCALAR_HAS_CXX20
#endif

#if CLASSIFY_SCALAR_CPLUSPLUS >= 201703L
#define CLASSIFY_SCALAR_HAS_CXX17
#endif

#if CLASSIFY_SCALAR_CPLUSPLUS >= 201402L
#define CLASSIFY_SCALAR_HAS_CXX14
#endif

#if defined(_WIN32) || defined(__LITTLE_ENDIAN__) \
    || (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
#define CLASSIFY_SCALAR_TRUE_U32 0x65757274U
#define CLASSIFY_SCALAR_FALSE_PREFIX_U32 0x736c6166U
#else
#define CLASSIFY_SCALAR_TRUE_U32 0x74727565U
#define CLASSIFY_SCALAR_FALSE_PREFIX_U32 0x66616c73U
#endif

#if defined(__clang__) || defined(__GNUC__)
#define CLASSIFY_SCALAR_CONST __attribute__((__const__))
#else
#define CLASSIFY_SCALAR_CONST
#endif

#if defined(CLASSIFY_SCALAR_DISABLE_FORCE_INLINE)
#define CLASSIFY_SCALAR_FORCE_INLINE inline
#elif defined(_MSC_VER)
#define CLASSIFY_SCALAR_FORCE_INLINE __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define CLASSIFY_SCALAR_FORCE_INLINE inline __attribute__((__always_inline__))
#else
#define CLASSIFY_SCALAR_FORCE_INLINE inline
#endif

#ifdef CLASSIFY_SCALAR_HAS_CXX14
#define CLASSIFY_SCALAR_CONSTEXPR_14 constexpr
#define CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 constexpr
#define CLASSIFY_SCALAR_LOCAL_TABLE_VALUE_14 constexpr
#else
#define CLASSIFY_SCALAR_CONSTEXPR_14 inline
#define CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 const
// C++11 cannot constexpr-build the lookup tables below, so local statics must
// stay mutable while their runtime initializer writes table entries.
#define CLASSIFY_SCALAR_LOCAL_TABLE_VALUE_14
#endif

#ifdef CLASSIFY_SCALAR_HAS_CXX17
#define IF_CONSTEXPR if constexpr
#define CLASSIFY_SCALAR_CONSTEXPR_17 constexpr
#define CLASSIFY_SCALAR_CONSTEXPR_VALUE_17 constexpr
#else
#define IF_CONSTEXPR if
#define CLASSIFY_SCALAR_CONSTEXPR_17 inline
#define CLASSIFY_SCALAR_CONSTEXPR_VALUE_17 const
#endif

#ifdef CLASSIFY_SCALAR_HAS_CXX20
#include <concepts>
#endif

#if CLASSIFY_SCALAR_CPLUSPLUS >= 201703L
#include <charconv>
#include <string_view>
#include <system_error>
#endif

#if defined(CLASSIFY_SCALAR_HAS_CXX17) && !defined(_LIBCPP_VERSION) && !defined(CLASSIFY_SCALAR_DISABLE_STD_FLOAT_FROM_CHARS)
#define CLASSIFY_SCALAR_HAS_STD_FLOAT_FROM_CHARS
#endif

namespace classify_scalar {

/// Built-in scalar classification ids returned by classify_scalar().
enum ScalarKind : int {
    /// Empty input after optional ASCII trimming.
    scalar_null = 0,
    /// Input did not match any enabled scalar policy.
    scalar_string = 1,
    /// Case-insensitive "true" or "false".
    scalar_bool = 2,
    /// Signed 8-bit integer, including 0x-prefixed hexadecimal.
    scalar_int8 = 3,
    /// Reserved for future unsigned 8-bit integer classification.
    scalar_uint8 = 4,
    /// Signed 16-bit integer, including 0x-prefixed hexadecimal.
    scalar_int16 = 5,
    /// Reserved for future unsigned 16-bit integer classification.
    scalar_uint16 = 6,
    /// Signed 32-bit integer, including 0x-prefixed hexadecimal.
    scalar_int32 = 7,
    /// Reserved for future unsigned 32-bit integer classification.
    scalar_uint32 = 8,
    /// Signed 64-bit integer, including 0x-prefixed hexadecimal.
    scalar_int64 = 9,
    /// Reserved for future unsigned 64-bit integer classification.
    scalar_uint64 = 10,
    /// Well-formed decimal integer outside the int64 range.
    scalar_bigint = 11,
    /// Floating-point literal parsed as finite double.
    scalar_float = 12,
    /// Well-formed floating-point literal outside the built-in finite double conversion envelope.
    scalar_bigfloat = 13,
    /// Conservative ISO date/date-time value, stored as UTC unix milliseconds when parsed.
    scalar_timestamp = 14,
    /// Reserved sentinel for invalid integration results.
    scalar_invalid = -2,
    /// First id available for user-defined scalar kinds.
    scalar_custom_begin = 1024
};

/// Include these entries at the top of a custom scalar enum.
#define CLASSIFY_SCALAR_BUILTINS \
    scalar_null = ::classify_scalar::scalar_null, \
    scalar_string = ::classify_scalar::scalar_string, \
    scalar_bool = ::classify_scalar::scalar_bool, \
    scalar_int8 = ::classify_scalar::scalar_int8, \
    scalar_uint8 = ::classify_scalar::scalar_uint8, \
    scalar_int16 = ::classify_scalar::scalar_int16, \
    scalar_uint16 = ::classify_scalar::scalar_uint16, \
    scalar_int32 = ::classify_scalar::scalar_int32, \
    scalar_uint32 = ::classify_scalar::scalar_uint32, \
    scalar_int64 = ::classify_scalar::scalar_int64, \
    scalar_uint64 = ::classify_scalar::scalar_uint64, \
    scalar_bigint = ::classify_scalar::scalar_bigint, \
    scalar_float = ::classify_scalar::scalar_float, \
    scalar_bigfloat = ::classify_scalar::scalar_bigfloat, \
    scalar_timestamp = ::classify_scalar::scalar_timestamp, \
    scalar_invalid = ::classify_scalar::scalar_invalid, \
    scalar_custom_begin = ::classify_scalar::scalar_custom_begin - 1

/// Non-owning half-open character span [first, last).
struct scalar_span {
    scalar_span() noexcept : first(nullptr), last(nullptr) {}
    scalar_span(const char* first_, const char* last_) noexcept : first(first_), last(last_) {}

    const char* first;
    const char* last;
};

/// Output object used when only the scalar kind is needed.
struct classify_only_output {
    template<ScalarKind, typename T>
    void set(T) const noexcept {}
};

namespace detail {
namespace integer {

/// Mapping of integer kinds to their rank for easy comparison.
CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 std::array<unsigned, 9> kind_rank_table = {{
    1U, // scalar_int8
    1U, // scalar_uint8
    2U, // scalar_int16
    2U, // scalar_uint16
    3U, // scalar_int32
    3U, // scalar_uint32
    4U, // scalar_int64
    4U, // scalar_uint64
    5U  // scalar_bigint
}};

/// True when kind is one of the built-in signed integer widths.
CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 bool is_signed_integer_kind(ScalarKind kind) noexcept {
    return kind == scalar_int8 || kind == scalar_int16 || kind == scalar_int32 || kind == scalar_int64;
}

/// True when kind is one of the reserved unsigned integer widths.
CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 bool is_unsigned_integer_kind(ScalarKind kind) noexcept {
    return kind == scalar_uint8 || kind == scalar_uint16 || kind == scalar_uint32 || kind == scalar_uint64;
}

/// True when kind is any built-in integer or bigint classification.
CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 bool is_integer_kind(ScalarKind kind) noexcept {
    return is_signed_integer_kind(kind) || is_unsigned_integer_kind(kind) || kind == scalar_bigint;
}

/// Rank integer widths from smallest to largest; non-integers return 0.
CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 unsigned integer_kind_rank(ScalarKind kind) noexcept {
    constexpr auto offset = static_cast<unsigned>(scalar_int8);
    return kind >= scalar_int8 && kind <= scalar_bigint ?
         kind_rank_table[static_cast<unsigned>(kind) - offset] : 0U;
}

/// True when parsed_integer_kind can be stored by target_integer_kind.
CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 bool integer_kind_fits_in(
    ScalarKind parsed_integer_kind,
    ScalarKind target_integer_kind) noexcept {
    return is_signed_integer_kind(parsed_integer_kind)
        && is_signed_integer_kind(target_integer_kind)
        && integer_kind_rank(parsed_integer_kind) <= integer_kind_rank(target_integer_kind);
}

/// Return the narrowest signed integer scalar kind that can store value.
CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 ScalarKind classify_integer_kind(std::int64_t value) noexcept {
    if (value >= 0) {
        return value <= static_cast<std::int64_t>(std::numeric_limits<std::int8_t>::max())
            ? scalar_int8
            : value <= static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::max())
            ? scalar_int16
            : value <= static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())
            ? scalar_int32
            : scalar_int64;
    }

    return value >= static_cast<std::int64_t>(std::numeric_limits<std::int8_t>::min())
        ? scalar_int8
        : value >= static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::min())
        ? scalar_int16
        : value >= static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min())
        ? scalar_int32
        : scalar_int64;
}

} // namespace integer
} // namespace detail

/// Built-in output adapter for numeric, bool, and timestamp storage.
struct builtin_output_refs {
    builtin_output_refs(long double& number_, std::int64_t& integer_, bool& boolean_) noexcept
        : number(number_), integer(integer_), boolean(boolean_), timestamp(nullptr) {}

    builtin_output_refs(
        long double& number_,
        std::int64_t& integer_,
        bool& boolean_,
        std::uint64_t& timestamp_) noexcept
        : number(number_),
          integer(integer_),
          boolean(boolean_),
          timestamp(&timestamp_) {}

    template<ScalarKind Kind>
    typename std::enable_if<Kind >= scalar_int8 && Kind <= scalar_int64 && ((Kind - scalar_int8) % 2 == 0), void>::type
    set(std::int64_t value) const noexcept {
        integer = value;
    }

    template<ScalarKind Kind>
    typename std::enable_if<Kind == scalar_float, void>::type set(long double value) const noexcept {
        number = value;
    }

    template<ScalarKind Kind>
    typename std::enable_if<Kind == scalar_bool, void>::type set(bool value) const noexcept {
        boolean = value;
    }

    template<ScalarKind Kind>
    typename std::enable_if<Kind == scalar_timestamp, void>::type set(std::uint64_t value) const noexcept {
        if (timestamp)
            *timestamp = value;
    }

    long double& number;
    std::int64_t& integer;
    bool& boolean;
    std::uint64_t* timestamp;
};

/// Create the standard built-in output adapter.
CLASSIFY_SCALAR_FORCE_INLINE builtin_output_refs output_refs(long double& number, std::int64_t& integer, bool& boolean) noexcept {
    return builtin_output_refs(number, integer, boolean);
}

enum class ParseFlag : unsigned char {
    /// Any byte with no scalar-classification meaning in the active parse table.
    other,
    /// ASCII digit bytes '0' through '9'.
    digit,
    /// Active decimal separator byte, '.' by default.
    decimal
};

namespace detail {

template<typename Enum>
struct has_custom_scalar_begin {
    template<typename T>
    static char test(decltype(T::scalar_custom_begin)*);

    template<typename>
    static long test(...);

    enum { value = sizeof(test<Enum>(nullptr)) == sizeof(char) };
};

template<typename Enum, bool HasCustomBegin>
struct custom_scalar_enum_guard {
    static_assert(HasCustomBegin, "custom scalar enums should include CLASSIFY_SCALAR_BUILTINS");

    static constexpr int cast_to_int(Enum value) noexcept {
        return static_cast<int>(value);
    }

    static constexpr int cast_scalar_kind(ScalarKind value) noexcept {
        return static_cast<int>(value);
    }
};

template<typename Enum>
struct custom_scalar_enum_guard<Enum, true> {
    static_assert(
        static_cast<int>(Enum::scalar_custom_begin) == scalar_custom_begin - 1,
        "custom scalar enum has an invalid scalar_custom_begin sentinel");

    static constexpr int cast_to_int(Enum value) noexcept {
        return static_cast<int>(value);
    }

    static constexpr int cast_scalar_kind(ScalarKind value) noexcept {
        return static_cast<int>(value);
    }
};

template<typename Kind, bool IsScalarKind>
struct scalar_kind_cast_impl;

template<typename Kind>
struct scalar_kind_cast_impl<Kind, false> {
    static_assert(std::is_enum<Kind>::value, "custom scalar kind type must be an enum");

    static constexpr Kind from_scalar_kind(ScalarKind value) noexcept {
        return static_cast<Kind>(custom_scalar_enum_guard<
            Kind,
            has_custom_scalar_begin<Kind>::value>::cast_scalar_kind(value));
    }

    template<typename Enum>
    static constexpr Kind from_enum(Enum value) noexcept {
        return static_cast<Kind>(custom_scalar_enum_guard<
            Enum,
            has_custom_scalar_begin<Enum>::value>::cast_to_int(value));
    }
};

template<typename Kind>
struct scalar_kind_cast_impl<Kind, true> {
    static constexpr ScalarKind from_scalar_kind(ScalarKind value) noexcept {
        return value;
    }

    template<typename Enum>
    static constexpr ScalarKind from_enum(Enum value) noexcept {
        return static_cast<ScalarKind>(custom_scalar_enum_guard<
            Enum,
            has_custom_scalar_begin<Enum>::value>::cast_to_int(value));
    }
};

template<typename Kind>
constexpr Kind scalar_kind_cast(ScalarKind value) noexcept {
    return scalar_kind_cast_impl<
        Kind,
        std::is_same<Kind, ScalarKind>::value>::from_scalar_kind(value);
}

template<typename Kind, typename Enum>
constexpr typename std::enable_if<std::is_enum<Enum>::value && !std::is_same<Enum, ScalarKind>::value, Kind>::type
scalar_kind_cast(Enum value) noexcept {
    return scalar_kind_cast_impl<
        Kind,
        std::is_same<Kind, ScalarKind>::value>::from_enum(value);
}

CLASSIFY_SCALAR_FORCE_INLINE bool is_ascii_space(const char c) noexcept {
    static CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 bool table[256] = {
        false, false, false, false, false, false, false, false,
        false, true, true, true, true, true, false, false,
        false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false,
        true
    };
    return table[static_cast<unsigned char>(c)];
}

template<char DecimalSymbol>
CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 ParseFlag classify_ascii_char(const unsigned char c) noexcept {
    return c >= '0' && c <= '9' ? ParseFlag::digit
        : c == static_cast<unsigned char>(DecimalSymbol) ? ParseFlag::decimal
        : ParseFlag::other;
}

template<std::size_t... Indexes>
struct index_sequence {};

template<std::size_t Count, std::size_t... Indexes>
struct make_index_sequence_impl : make_index_sequence_impl<Count - 1, Count - 1, Indexes...> {};

template<std::size_t... Indexes>
struct make_index_sequence_impl<0, Indexes...> {
    typedef index_sequence<Indexes...> type;
};

template<std::size_t Count>
struct make_index_sequence {
    typedef typename make_index_sequence_impl<Count>::type type;
};

struct parse_table_type {
    ParseFlag values[256];

    CLASSIFY_SCALAR_CONSTEXPR_14 ParseFlag operator[](unsigned char value) const noexcept {
        return values[value];
    }
};

struct dispatch_table_type {
    unsigned char values[256];

    CLASSIFY_SCALAR_CONSTEXPR_14 unsigned char operator[](unsigned char value) const noexcept {
        return values[value];
    }
};

struct parse_state {
    enum Sign : unsigned char {
        no_sign,
        positive_sign,
        negative_sign
    };

    parse_state(const char* first_, const char* last_) noexcept
        : first(first_),
          last(last_),
          current(first_),
          numeric_first(first_),
          sign(no_sign) {}

    const char* first;
    const char* last;
    const char* current;
    const char* numeric_first;
    Sign sign;
};

#ifdef CLASSIFY_SCALAR_HAS_CXX20
template<typename Policy>
concept scalar_policy = requires(
    unsigned char c,
    const Policy& policy,
    parse_state& state,
    classify_only_output& output) {
    { Policy::matches_leading(c) } -> std::convertible_to<bool>;
    policy.on_dispatch(state, output);
};
#endif

CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 unsigned char no_dispatch_policy = 255U;

template<unsigned char Index, typename... Policies>
struct dispatch_index_impl;

template<unsigned char Index>
struct dispatch_index_impl<Index> {
    CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 static unsigned char value(unsigned char) noexcept {
        return no_dispatch_policy;
    }
};

template<unsigned char Index, typename First, typename... Rest>
struct dispatch_index_impl<Index, First, Rest...> {
    CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 static unsigned char value(unsigned char c) noexcept {
        return First::matches_leading(c)
            ? Index
            : dispatch_index_impl<static_cast<unsigned char>(Index + 1U), Rest...>::value(c);
    }
};

template<std::size_t Index, std::size_t Count>
struct policy_dispatch_impl {
    template<typename Kind, typename Tuple, typename Output>
    CLASSIFY_SCALAR_FORCE_INLINE static Kind call(
        const unsigned char policy_index,
        const Tuple& policies,
        parse_state& state,
        Output& output) noexcept {
        typedef typename std::remove_reference<Tuple>::type tuple_type;
        typedef typename std::tuple_element<Index, tuple_type>::type policy_type;

        if (policy_index > Index)
            return policy_dispatch_impl<Index + 1U, Count>::template call<Kind>(policy_index, policies, state, output);

        if (policy_index == Index || policy_type::matches_leading(static_cast<unsigned char>(*state.first))) {
            const Kind kind = scalar_kind_cast<Kind>(std::get<Index>(policies).on_dispatch(state, output));
            if (kind != scalar_kind_cast<Kind>(scalar_string))
                return kind;
        }

        return policy_dispatch_impl<Index + 1U, Count>::template call<Kind>(policy_index, policies, state, output);
    }
};

template<std::size_t Count>
struct policy_dispatch_impl<Count, Count> {
    template<typename Kind, typename Tuple, typename Output>
    CLASSIFY_SCALAR_FORCE_INLINE static Kind call(
        unsigned char,
        const Tuple&,
        parse_state&,
        Output&) noexcept {
        return scalar_kind_cast<Kind>(scalar_string);
    }
};

template<typename... Policies>
#ifdef CLASSIFY_SCALAR_HAS_CXX20
requires (scalar_policy<Policies> && ...)
#endif
struct policy_pack {
    typedef std::tuple<Policies...> tuple_type;

    policy_pack() noexcept : policies() {}

    explicit policy_pack(Policies... policies_) noexcept
        : policies(policies_...) {}

    CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 static unsigned char dispatch_index(unsigned char c) noexcept {
        return dispatch_index_impl<0U, Policies...>::value(c);
    }

    template<typename Kind = ScalarKind, typename Output>
    CLASSIFY_SCALAR_FORCE_INLINE Kind dispatch(
        const unsigned char policy_index,
        parse_state& state,
        Output& output) const noexcept {
        return policy_dispatch_impl<0U, sizeof...(Policies)>::template call<Kind>(policy_index, policies, state, output);
    }

    tuple_type policies;
};

template<char DecimalSymbol, std::size_t... Indexes>
CLASSIFY_SCALAR_CONSTEXPR_14 parse_table_type build_parse_table(index_sequence<Indexes...>) noexcept {
    return parse_table_type{{classify_ascii_char<DecimalSymbol>(static_cast<unsigned char>(Indexes))...}};
}

template<char DecimalSymbol>
CLASSIFY_SCALAR_FORCE_INLINE const parse_table_type& parse_table() noexcept {
    static CLASSIFY_SCALAR_LOCAL_TABLE_VALUE_14 parse_table_type table =
        build_parse_table<DecimalSymbol>(typename make_index_sequence<256>::type());
    return table;
}

template<typename PolicyPack, std::size_t... Indexes>
CLASSIFY_SCALAR_CONSTEXPR_14 dispatch_table_type build_dispatch_table(index_sequence<Indexes...>) noexcept {
    return dispatch_table_type{{PolicyPack::dispatch_index(static_cast<unsigned char>(Indexes))...}};
}

template<typename PolicyPack>
CLASSIFY_SCALAR_FORCE_INLINE const dispatch_table_type& dispatch_table() noexcept {
    static CLASSIFY_SCALAR_LOCAL_TABLE_VALUE_14 dispatch_table_type table =
        build_dispatch_table<PolicyPack>(typename make_index_sequence<256>::type());
    return table;
}

CLASSIFY_SCALAR_CONSTEXPR_17 std::array<char, 256> create_ascii_lower_table() noexcept {
    std::array<char, 256> table = {};
    for (std::size_t i = 0; i < table.size(); ++i) {
        table[i] = static_cast<char>(i);
    }
    for (unsigned char i = 0; i < 26; ++i) {
        table[static_cast<unsigned char>('A' + i)] = static_cast<char>('a' + i);
    }
    return table;
}

CLASSIFY_SCALAR_CONSTEXPR_VALUE_17 std::array<char, 256> ascii_lower_chars = create_ascii_lower_table();

CLASSIFY_SCALAR_CONSTEXPR_17 std::array<bool, 256> create_ascii_digits_table() noexcept {
    std::array<bool, 256> table = {};
    for (unsigned char i = 0; i < 10; ++i) {
        table[static_cast<unsigned char>('0' + i)] = true;
    }
    return table;
}

CLASSIFY_SCALAR_CONSTEXPR_VALUE_17 std::array<bool, 256> ascii_digits = create_ascii_digits_table();

CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 unsigned char invalid_digit_value = 255U;

CLASSIFY_SCALAR_CONSTEXPR_17 std::array<unsigned char, 256> create_digit_values_table() noexcept {
    std::array<unsigned char, 256> table = {};
    for (std::size_t i = 0; i < table.size(); ++i) {
        table[i] = invalid_digit_value;
    }
    for (unsigned char i = 0; i < 10; ++i) {
        table[static_cast<unsigned char>('0' + i)] = i;
    }
    for (unsigned char i = 0; i < 26; ++i) {
        table[static_cast<unsigned char>('A' + i)] = static_cast<unsigned char>(10U + i);
        table[static_cast<unsigned char>('a' + i)] = static_cast<unsigned char>(10U + i);
    }
    return table;
}

CLASSIFY_SCALAR_CONSTEXPR_VALUE_17 std::array<unsigned char, 256> digit_values = create_digit_values_table();

CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 std::int64_t int64_min_value = std::numeric_limits<std::int64_t>::min();
CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 std::int64_t int64_max_value = std::numeric_limits<std::int64_t>::max();
CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 std::uint64_t int64_positive_limit = static_cast<std::uint64_t>(int64_max_value);
CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 std::uint64_t int64_negative_limit = int64_positive_limit + 1U;
CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 std::uint64_t signed_integer_limits[3] = {
    int64_positive_limit,
    int64_positive_limit,
    int64_negative_limit
};
CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 long double int64_min_long_double = static_cast<long double>(int64_min_value);
CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 long double int64_max_long_double = static_cast<long double>(int64_max_value);

namespace parsing {

CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 parse_state::Sign parse_sign(unsigned char c) noexcept {
    return c == static_cast<unsigned char>('+') ? parse_state::positive_sign :
        c == static_cast<unsigned char>('-') ? parse_state::negative_sign :
        parse_state::no_sign;
}

CLASSIFY_SCALAR_FORCE_INLINE const char* apply_leading_sign(parse_state& state) noexcept {
    const unsigned char first_char = static_cast<unsigned char>(*state.first);
    const parse_state::Sign sign = parse_sign(first_char);
    if (sign != parse_state::no_sign) {
        state.sign = sign;
        // Keep '-' visible to floating parsers, but skip '+' because from_chars
        // implementations commonly reject a leading plus for floating input.
        state.numeric_first = state.first + (sign == parse_state::positive_sign ? 1 : 0);
        return state.first + 1;
    }

    return state.first;
}

CLASSIFY_SCALAR_FORCE_INLINE std::uint32_t load_u32(const char* value) noexcept {
    std::uint32_t word = 0;
    std::memcpy(&word, value, sizeof(word));
    return word;
}

CLASSIFY_SCALAR_FORCE_INLINE unsigned char decimal_digit_value(const unsigned char c) noexcept {
    // The digit_values[] lookup is useful for generic-base parsing, but benchmarked slower in the hot decimal scanner.
    return static_cast<unsigned char>(c - static_cast<unsigned char>('0'));
}

CLASSIFY_SCALAR_FORCE_INLINE scalar_span trim_ascii(const char* first, const char* last) noexcept {
    while (first != last && is_ascii_space(*first))
        ++first;

    while (first != last && is_ascii_space(*(last - 1)))
        --last;

    return scalar_span{first, last};
}

template<bool TrimAsciiWhitespace>
CLASSIFY_SCALAR_FORCE_INLINE scalar_span trim_span(const char* first, const char* last) noexcept {
    if (!first || !last || last < first)
        return scalar_span();

    IF_CONSTEXPR(TrimAsciiWhitespace)
        return trim_ascii(first, last);
    else
        return scalar_span(first, last);
}

CLASSIFY_SCALAR_FORCE_INLINE bool parse_true(const char* first, const char* last, bool* out) noexcept {
    if (last - first != 4)
        return false;

    const std::uint32_t lowered = load_u32(first) | 0x20202020U;
    if (lowered != CLASSIFY_SCALAR_TRUE_U32)
        return false;

    if (out)
        *out = true;

    return true;
}

CLASSIFY_SCALAR_FORCE_INLINE bool parse_false(const char* first, const char* last, bool* out) noexcept {
    if (last - first != 5)
        return false;

    const std::uint32_t lowered = load_u32(first) | 0x20202020U;
    if (lowered != CLASSIFY_SCALAR_FALSE_PREFIX_U32 || (static_cast<unsigned char>(first[4]) | 0x20U) != 'e')
        return false;

    if (out)
        *out = false;

    return true;
}

} // namespace parsing

namespace parsing_timestamp {

CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 bool is_leap_year(const int year) noexcept {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 int common_days_in_month[13] = {
    0,
    31, 28, 31, 30, 31, 30,
    31, 31, 30, 31, 30, 31
};

CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 int days_in_month(const int year, const int month) noexcept {
    return month == 2 && is_leap_year(year) ? 29 : common_days_in_month[month];
}

template<std::size_t Count>
CLASSIFY_SCALAR_FORCE_INLINE bool parse_digits(const char* value, int& out) noexcept {
    int parsed = 0;
    for (std::size_t i = 0; i < Count; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (!ascii_digits[c])
            return false;

        parsed = (parsed * 10) + (value[i] - '0');
    }
    out = parsed;
    return true;
}

CLASSIFY_SCALAR_FORCE_INLINE bool valid_iso_date(const int year, const int month, const int day) noexcept {
    return (month < 1 || month > 12) ? false :
        day >= 1 && day <= days_in_month(year, month);
}

CLASSIFY_SCALAR_FORCE_INLINE std::int64_t days_from_civil(int year, const int month, const int day) noexcept {
    year -= month <= 2;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153U * static_cast<unsigned>(month + (month > 2 ? -3 : 9)) + 2U) / 5U
        + static_cast<unsigned>(day) - 1U;
    const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

CLASSIFY_SCALAR_FORCE_INLINE bool parse_iso_timestamp(
    const char* first,
    const char* last,
    std::uint64_t* out = nullptr) noexcept {
    if (last - first < 10)
        return false;

    if (first[4] != '-' || first[7] != '-')
        return false;

    int year = 0;
    int month = 0;
    int day = 0;
    if (!parse_digits<4>(first, year) || !parse_digits<2>(first + 5, month) || !parse_digits<2>(first + 8, day))
        return false;

    if (!valid_iso_date(year, month, day))
        return false;

    int hour = 0;
    int minute = 0;
    int second = 0;
    int millisecond = 0;
    int timezone_sign = 0;
    int timezone_hour = 0;
    int timezone_minute = 0;
    const char* current = first + 10;
    if (current != last) {
        if (ascii_lower_chars[static_cast<unsigned char>(*current)] != 't')
            return false;

        ++current;
        if (current + 5 > last || current[2] != ':')
            return false;

        if (!parse_digits<2>(current, hour) || !parse_digits<2>(current + 3, minute))
            return false;

        if (hour > 23 || minute > 59)
            return false;

        current += 5;
        if (current != last && *current == ':') {
            ++current;
            if (current + 2 > last)
                return false;

            if (!parse_digits<2>(current, second) || second > 59)
                return false;

            current += 2;
            if (current != last && *current == '.') {
                ++current;
                const char* fraction_first = current;
                while (current != last && ascii_digits[static_cast<unsigned char>(*current)]) {
                    if (current - fraction_first < 3)
                        millisecond = millisecond * 10 + (*current - '0');
                    ++current;
                }

                if (current == fraction_first)
                    return false;

                for (std::ptrdiff_t digits = current - fraction_first; digits < 3; ++digits)
                    millisecond *= 10;
            }
        }

        if (current != last) {
            if (ascii_lower_chars[static_cast<unsigned char>(*current)] == 'z') {
                ++current;
            } else {
                if (*current != '+' && *current != '-')
                    return false;

                timezone_sign = *current == '+' ? 1 : -1;
                if (current + 6 == last && current[3] == ':') {
                    if (!parse_digits<2>(current + 1, timezone_hour) || !parse_digits<2>(current + 4, timezone_minute))
                        return false;
                    current = last;
                } else if (current + 5 == last) {
                    if (!parse_digits<2>(current + 1, timezone_hour) || !parse_digits<2>(current + 3, timezone_minute))
                        return false;
                    current = last;
                } else {
                    return false;
                }

                if (timezone_hour > 23 || timezone_minute > 59)
                    return false;
            }
        }

        if (current != last)
            return false;
    }

    if (!out)
        return true;

    const std::int64_t timezone_offset_milliseconds =
        static_cast<std::int64_t>(timezone_sign)
        * (static_cast<std::int64_t>(timezone_hour) * 60 + timezone_minute)
        * 60 * 1000;
    const std::int64_t timestamp =
        days_from_civil(year, month, day) * 86400000
        + static_cast<std::int64_t>(hour) * 3600000
        + static_cast<std::int64_t>(minute) * 60000
        + static_cast<std::int64_t>(second) * 1000
        + millisecond
        - timezone_offset_milliseconds;

    if (timestamp < 0)
        return false;

    *out = static_cast<std::uint64_t>(timestamp);
    return true;
}

} // namespace parsing_timestamp

namespace parsing {

enum integer_parse_result {
    integer_parse_invalid,
    integer_parse_valid,
    integer_parse_overflow
};

CLASSIFY_SCALAR_FORCE_INLINE integer_parse_result finish_signed_integer(
    const parse_state& state,
    const std::uint64_t magnitude,
    const std::uint64_t limit,
    std::int64_t* out) noexcept {
    if (magnitude > limit)
        return integer_parse_overflow;

    if (out) {
        if (state.sign == parse_state::negative_sign) {
            *out = magnitude == limit
                ? int64_min_value
                : -static_cast<std::int64_t>(magnitude);
        } else {
            *out = static_cast<std::int64_t>(magnitude);
        }
    }

    return integer_parse_valid;
}

CLASSIFY_SCALAR_FORCE_INLINE integer_parse_result parse_integer_digits(
    const parse_state& state,
    const char* first,
    const char* last,
    const unsigned base,
    std::int64_t* out) noexcept {
    assert(first != last);

    const std::uint64_t limit = signed_integer_limits[state.sign];
    std::uint64_t acc = 0;
    for (const char* current = first; current != last; ++current) {
        const unsigned char digit = digit_values[static_cast<unsigned char>(*current)];
        if (digit >= base)
            return integer_parse_invalid;

        // Precomputing cutoff/cutlim looked cleaner but was measurably slower on MSVC.
        if (acc > (limit - digit) / base)
            return integer_parse_overflow;

        acc = (acc * base) + digit;
    }

    return finish_signed_integer(state, acc, limit, out);
}

CLASSIFY_SCALAR_FORCE_INLINE bool parse_hex_integer(
    const parse_state& state,
    std::int64_t* out) noexcept {
    const char* current = state.sign == parse_state::no_sign ? state.first : state.first + 1;
    const char* last = state.last;
    assert(current != last);

    if (current + 2 > last || current[0] != '0' || state.current != current + 1)
        return false;

    assert(current[1] == 'x' || current[1] == 'X');
    current += 2;

    assert(current != last);

    return parse_integer_digits(state, current, last, 16U, out) == integer_parse_valid;
}

CLASSIFY_SCALAR_FORCE_INLINE bool parse_bare_hex_integer(
    const parse_state& state,
    std::int64_t* out) noexcept {
    const char* current = state.sign == parse_state::no_sign ? state.first : state.first + 1;
    const char* last = state.last;
    assert(current != last);
    return parse_integer_digits(state, current, last, 16U, out) == integer_parse_valid;
}

template<typename Output>
CLASSIFY_SCALAR_FORCE_INLINE ScalarKind finish_integer(
    const std::int64_t parsed_integer,
    Output& output) noexcept {
    output.template set<scalar_int64>(parsed_integer);
    return integer::classify_integer_kind(parsed_integer);
}

} // namespace parsing

namespace floating {

enum { powers_of_10_count = 19 };

// 10^0..10^18 are exact on common x86 extended long double and cover the
// precision envelope where the C++11 fallback parser tries to match legacy
// csv-parser data_type() / from_chars-style double results.
CLASSIFY_SCALAR_CONSTEXPR_VALUE_14 std::array<long double, powers_of_10_count> POWERS_OF_10 = {{
    1.0L,
    10.0L,
    100.0L,
    1000.0L,
    10000.0L,
    100000.0L,
    1000000.0L,
    10000000.0L,
    100000000.0L,
    1000000000.0L,
    10000000000.0L,
    100000000000.0L,
    1000000000000.0L,
    10000000000000.0L,
    100000000000000.0L,
    1000000000000000.0L,
    10000000000000000.0L,
    100000000000000000.0L,
    1000000000000000000.0L,
}};

CLASSIFY_SCALAR_FORCE_INLINE long double pow10_positive_integer(const int exponent) noexcept {
    if (exponent < powers_of_10_count)
        return POWERS_OF_10[static_cast<std::size_t>(exponent)];

    long double value = POWERS_OF_10[powers_of_10_count - 1];
    for (int i = powers_of_10_count - 1; i < exponent; ++i)
        value *= 10.0L;

    return value;
}

CLASSIFY_SCALAR_FORCE_INLINE long double pow10_integer(const int exponent) noexcept {
    const int abs_exponent = exponent >= 0 ? exponent : -exponent;
    const long double value = pow10_positive_integer(abs_exponent);
    return exponent >= 0 ? value : 1.0L / value;
}

enum class floating_parse_status {
    invalid,
    parsed,
    bigfloat
};

CLASSIFY_SCALAR_FORCE_INLINE floating_parse_status parse_floating_exponent(
    const char*& current,
    const char* last,
    int* out) noexcept {
    assert(current != last);
    assert(*current == 'e' || *current == 'E');

    ++current;
    if (current == last)
        return floating_parse_status::invalid;

    bool exponent_negative = false;
    const parse_state::Sign exponent_sign = parsing::parse_sign(static_cast<unsigned char>(*current));
    if (exponent_sign != parse_state::no_sign) {
        exponent_negative = exponent_sign == parse_state::negative_sign;
        ++current;
        if (current == last)
            return floating_parse_status::invalid;
    }

    int exponent = 0;
    while (current != last && ascii_digits[static_cast<unsigned char>(*current)]) {
        if (exponent <= 500)
            exponent = (exponent * 10) + (*current - '0');

        ++current;
    }

    if (current != last)
        return floating_parse_status::invalid;

    if (exponent > 500)
        return floating_parse_status::bigfloat;

    *out = exponent_negative ? -exponent : exponent;
    return floating_parse_status::parsed;
}

template<char DecimalSymbol>
CLASSIFY_SCALAR_FORCE_INLINE floating_parse_status parse_floating_ascii(
    const parse_state& state,
    double* out) noexcept {
    const char* current = state.numeric_first;
    const char* last = state.last;
    assert(current != last);

    if (state.sign == parse_state::negative_sign)
        ++current;

    long double integral_part = 0;
    long double decimal_part = 0;
    unsigned places_after_decimal = 0;
    int exponent = 0;
    bool has_digit = false;

    // Keep this split integral/fractional accumulation shape. A simpler
    // mantissa * pow10(exponent) implementation is mathematically equivalent,
    // but it can round to the neighboring double for user-visible CHECK_EQ
    // cases that legacy csv-parser data_type() handled exactly.

    while (current != last && ascii_digits[static_cast<unsigned char>(*current)]) {
        const unsigned char digit = static_cast<unsigned char>(*current - '0');
        integral_part = (integral_part * 10.0L) + digit;
        has_digit = true;
        ++current;
    }

    if (current != last && static_cast<unsigned char>(*current) == static_cast<unsigned char>(DecimalSymbol)) {
        ++current;
        while (current != last && ascii_digits[static_cast<unsigned char>(*current)]) {
            const unsigned char digit = static_cast<unsigned char>(*current - '0');
            decimal_part += digit / pow10_integer(static_cast<int>(++places_after_decimal));
            has_digit = true;
            ++current;
        }
    }

    if (!has_digit)
        return floating_parse_status::invalid;

    if (current != last && (*current == 'e' || *current == 'E')) {
        const floating_parse_status exponent_status = parse_floating_exponent(current, last, &exponent);
        if (exponent_status != floating_parse_status::parsed)
            return exponent_status;
    }

    if (current != last)
        return floating_parse_status::invalid;

    if (exponent > 308 || exponent < -308)
        return floating_parse_status::bigfloat;

    long double parsed = (integral_part + decimal_part) * pow10_integer(exponent);
    if (state.sign == parse_state::negative_sign)
        parsed = -parsed;

    const double as_double = static_cast<double>(parsed);
    if (!std::isfinite(as_double))
        return floating_parse_status::bigfloat;

    if (out)
        *out = as_double;

    return floating_parse_status::parsed;
}

CLASSIFY_SCALAR_FORCE_INLINE floating_parse_status parse_floating_dot(
    const parse_state& state,
    double* out) noexcept {
    const char* first = state.numeric_first;
    const char* last = state.last;
    const std::size_t size = static_cast<std::size_t>(last - first);
    assert(first != last);
    if (size > 4096)
        return floating_parse_status::bigfloat;

#ifdef CLASSIFY_SCALAR_HAS_STD_FLOAT_FROM_CHARS
    double parsed = 0;
    const std::from_chars_result result = std::from_chars(first, last, parsed);
    if (result.ptr != last)
        return floating_parse_status::invalid;
    if (result.ec == std::errc::result_out_of_range || !std::isfinite(parsed))
        return floating_parse_status::bigfloat;
    if (result.ec != std::errc())
        return floating_parse_status::invalid;

    if (out)
        *out = parsed;

    return floating_parse_status::parsed;
#else
    return parse_floating_ascii<'.'>(state, out);
#endif
}

#ifdef CLASSIFY_SCALAR_HAS_STD_FLOAT_FROM_CHARS
template<char DecimalSymbol>
CLASSIFY_SCALAR_FORCE_INLINE floating_parse_status normalize_floating_point_separator(
    const char* first,
    const char* last,
    char* buffer) noexcept {
    std::size_t i = 0;
    for (const char* current = first; current != last; ++current, ++i) {
        const unsigned char c = static_cast<unsigned char>(*current);
        if (is_ascii_space(static_cast<char>(c)))
            return floating_parse_status::invalid;
        if (c == '.')
            return floating_parse_status::invalid;

        buffer[i] = c == static_cast<unsigned char>(DecimalSymbol) ? '.' : static_cast<char>(c);
    }

    buffer[static_cast<std::size_t>(last - first)] = '\0';
    return floating_parse_status::parsed;
}
#endif

template<char DecimalSymbol>
CLASSIFY_SCALAR_FORCE_INLINE floating_parse_status parse_floating_with_decimal(
    const parse_state& state,
    double* out) noexcept {
    const char* first = state.numeric_first;
    const char* last = state.last;
    const std::size_t size = static_cast<std::size_t>(last - first);
    assert(first != last);
    if (size > 4096)
        return floating_parse_status::bigfloat;

#ifdef CLASSIFY_SCALAR_HAS_STD_FLOAT_FROM_CHARS
    char buffer[4097];
    const floating_parse_status normalize_status =
        normalize_floating_point_separator<DecimalSymbol>(first, last, buffer);
    if (normalize_status != floating_parse_status::parsed)
        return normalize_status;

    double parsed = 0;
    const std::from_chars_result result = std::from_chars(buffer, buffer + size, parsed);
    if (result.ptr != buffer + size)
        return floating_parse_status::invalid;
    if (result.ec == std::errc::result_out_of_range || !std::isfinite(parsed))
        return floating_parse_status::bigfloat;
    if (result.ec != std::errc())
        return floating_parse_status::invalid;

    if (out)
        *out = parsed;

    return floating_parse_status::parsed;
#else
    return parse_floating_ascii<DecimalSymbol>(state, out);
#endif
}

template<char DecimalSymbol>
struct floating_parser {
    CLASSIFY_SCALAR_FORCE_INLINE static floating_parse_status parse(
        const parse_state& state,
        double* out) noexcept {
        return parse_floating_with_decimal<DecimalSymbol>(state, out);
    }
};

template<>
struct floating_parser<'.'> {
    CLASSIFY_SCALAR_FORCE_INLINE static floating_parse_status parse(
        const parse_state& state,
        double* out) noexcept {
        return parse_floating_dot(state, out);
    }
};

template<char DecimalSymbol>
CLASSIFY_SCALAR_FORCE_INLINE floating_parse_status parse_floating(
    const parse_state& state,
    double* out) noexcept {
    return floating_parser<DecimalSymbol>::parse(state, out);
}

CLASSIFY_SCALAR_FORCE_INLINE bool floating_is_integral(const double value, std::int64_t* out) noexcept {
    if (value < int64_min_long_double || value > int64_max_long_double)
        return false;

    const std::int64_t integer = static_cast<std::int64_t>(value);
    // Exact round-trip test: we need to know whether the parsed double is
    // precisely representable as int64, not whether two measured floats are close.
    if (static_cast<double>(integer) != value)
        return false;

    if (out)
        *out = integer;

    return true;
}

CLASSIFY_SCALAR_FORCE_INLINE ScalarKind finish_floating(
    std::true_type,
    const double parsed_float,
    classify_only_output&) noexcept {
    std::int64_t parsed_integer = 0;
    if (floating_is_integral(parsed_float, &parsed_integer))
        return integer::classify_integer_kind(parsed_integer);

    return scalar_float;
}

template<typename Output>
CLASSIFY_SCALAR_FORCE_INLINE ScalarKind finish_floating(
    std::true_type,
    const double parsed_float,
    Output& output) noexcept {
    std::int64_t parsed_integer = 0;
    if (floating_is_integral(parsed_float, &parsed_integer))
        return parsing::finish_integer(parsed_integer, output);

    output.template set<scalar_float>(parsed_float);
    return scalar_float;
}

template<bool IntegralFloatingAsInteger, typename Output>
CLASSIFY_SCALAR_FORCE_INLINE ScalarKind finish_floating(
    const double parsed_float,
    Output& output) noexcept {
    IF_CONSTEXPR (IntegralFloatingAsInteger) {
        return finish_floating(std::true_type(), parsed_float, output);
    } else {
        output.template set<scalar_float>(parsed_float);
        return scalar_float;
    }
}

} // namespace floating

template<char DecimalSymbol = '.', bool IntegralFloatingAsInteger = true>
struct builtin_numeric_policy {
    static_assert(DecimalSymbol != 'e' && DecimalSymbol != 'E', "decimal symbol cannot be an exponent marker");
    static_assert(DecimalSymbol != 'x' && DecimalSymbol != 'X', "decimal symbol cannot be a hexadecimal prefix marker");
    static_assert(DecimalSymbol < '0' || DecimalSymbol > '9', "decimal symbol cannot be an ASCII digit");

    CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 static bool matches_leading(unsigned char c) noexcept {
        return (c >= '0' && c <= '9') || c == static_cast<unsigned char>(DecimalSymbol) || c == '+' || c == '-';
    }

    template<typename Output>
    CLASSIFY_SCALAR_FORCE_INLINE ScalarKind on_dispatch(
        parse_state& state,
        Output& output) const noexcept {
        return on_number(state, output);
    }

    template<typename Output>
    CLASSIFY_SCALAR_FORCE_INLINE ScalarKind on_decimal(
        parse_state& state,
        Output& output) const noexcept {
        double parsed_float = 0;
        const floating::floating_parse_status status = floating::parse_floating<DecimalSymbol>(state, &parsed_float);
        if (status == floating::floating_parse_status::parsed)
            return floating::finish_floating<IntegralFloatingAsInteger>(parsed_float, output);

        return status == floating::floating_parse_status::bigfloat
            ? scalar_bigfloat
            : scalar_string;
    }

    template<typename Output>
    CLASSIFY_SCALAR_FORCE_INLINE ScalarKind scan_short_number(
        parse_state& state,
        const char* value_first,
        Output& output) const noexcept {
        std::uint64_t acc = 0;

        for (const char* current = value_first; current != state.last; ++current) {
            state.current = current;
            const unsigned char c = static_cast<unsigned char>(*current);

            const ParseFlag flag = parse_table<DecimalSymbol>()[c];
            switch (flag) {
            case ParseFlag::digit:
                acc = (acc * 10U) + parsing::decimal_digit_value(c);
                continue;
            case ParseFlag::decimal:
                return on_decimal(state, output);
            case ParseFlag::other:
            default:
                if (ascii_lower_chars[c] == 'e')
                    return on_decimal(state, output);
                return scalar_string;
            }
        }

        state.current = state.last;
        std::int64_t parsed_integer = state.sign == parse_state::negative_sign
            ? -static_cast<std::int64_t>(acc)
            : static_cast<std::int64_t>(acc);
        return parsing::finish_integer(parsed_integer, output);
    }

    template<typename Output>
    CLASSIFY_SCALAR_FORCE_INLINE ScalarKind scan_checked_number(
        parse_state& state,
        const char* value_first,
        Output& output) const noexcept {
        std::uint64_t acc = 0;
        const std::uint64_t limit = signed_integer_limits[state.sign];
        bool overflow = false;

        for (const char* current = value_first; current != state.last; ++current) {
            state.current = current;
            const unsigned char c = static_cast<unsigned char>(*current);

            const ParseFlag flag = parse_table<DecimalSymbol>()[c];
            switch (flag) {
            case ParseFlag::digit: {
                const unsigned char digit = parsing::decimal_digit_value(c);
                if (!overflow) {
                    // Precomputing cutoff/cutlim, digit-count gating, and cold overflow helpers all benchmarked slower.
                    if (acc > (limit - digit) / 10U)
                        overflow = true;
                    else
                        acc = (acc * 10U) + digit;
                }
                continue;
            }
            case ParseFlag::decimal:
                return on_decimal(state, output);
            case ParseFlag::other:
            default:
                if (ascii_lower_chars[c] == 'e')
                    return on_decimal(state, output);
                return scalar_string;
            }
        }

        state.current = state.last;
        if (overflow)
            return scalar_bigint;

        std::int64_t parsed_integer = 0;
        parsing::finish_signed_integer(state, acc, limit, &parsed_integer);
        return parsing::finish_integer(parsed_integer, output);
    }

    template<typename Output>
    CLASSIFY_SCALAR_FORCE_INLINE ScalarKind scan_number(
        parse_state& state,
        const char* value_first,
        Output& output) const noexcept {
        // Use overflow checks for numbers with 19 or more digits, which can exceed 64-bit limits.
        // Shorter numbers are common enough that it's worth skipping the checks for them.
        // Testing note: yes this was a significant optimization.
        return state.last - value_first < 19
            ? scan_short_number(state, value_first, output)
            : scan_checked_number(state, value_first, output);
    }

    template<typename Output>
    CLASSIFY_SCALAR_FORCE_INLINE ScalarKind on_number(
        parse_state& state,
        Output& output) const noexcept {
        const char* value_first = parsing::apply_leading_sign(state);
        if (value_first == state.last)
            return scalar_string;

        const unsigned char value_first_char = static_cast<unsigned char>(*value_first);
        if (!ascii_digits[value_first_char] && value_first_char != static_cast<unsigned char>(DecimalSymbol))
            return scalar_string;

        if (value_first_char == '0' && value_first + 1 != state.last) {
            const unsigned char second_char = static_cast<unsigned char>(value_first[1]);
            if (ascii_lower_chars[second_char] == 'x') {
                if (value_first + 2 == state.last)
                    return scalar_string;

                state.current = value_first + 1;
                std::int64_t parsed_integer = 0;
                return parsing::parse_hex_integer(state, &parsed_integer)
                    ? parsing::finish_integer(parsed_integer, output)
                    : scalar_string;
            }
        }

        return scan_number(state, value_first, output);
    }
};

struct builtin_bool_policy {
    CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 static bool matches_leading(unsigned char c) noexcept {
        return c == 't' || c == 'T' || c == 'f' || c == 'F';
    }

    template<typename Output>
    CLASSIFY_SCALAR_FORCE_INLINE ScalarKind on_dispatch(
        parse_state& state,
        Output& output) const noexcept {
        bool parsed = false;
        if (!(parsing::parse_true(state.first, state.last, &parsed) || parsing::parse_false(state.first, state.last, &parsed)))
            return scalar_string;

        output.template set<scalar_bool>(parsed);
        return scalar_bool;
    }
};

struct builtin_timestamp_policy {
    CLASSIFY_SCALAR_CONST CLASSIFY_SCALAR_CONSTEXPR_14 static bool matches_leading(unsigned char c) noexcept {
        return c >= '0' && c <= '9';
    }

    template<typename Output>
    CLASSIFY_SCALAR_FORCE_INLINE ScalarKind on_dispatch(
        parse_state& state,
        Output& output) const noexcept {
        std::uint64_t timestamp = 0;
        if (!parsing_timestamp::parse_iso_timestamp(state.first, state.last, &timestamp))
            return scalar_string;

        output.template set<scalar_timestamp>(timestamp);
        return scalar_timestamp;
    }
};

template<ScalarKind Kind>
struct scalar_home;

template<typename T>
struct signed_integer_scalar_home {
    typedef T type;
    typedef policy_pack<builtin_numeric_policy<> > policy;

    static T get(long double, std::int64_t integer, std::uint64_t, bool) noexcept {
        return static_cast<T>(integer);
    }
};

template<>
struct scalar_home<scalar_bool> {
    typedef bool type;
    typedef policy_pack<builtin_bool_policy> policy;

    static bool get(long double, std::int64_t, std::uint64_t, bool boolean) noexcept {
        return boolean;
    }
};

template<>
struct scalar_home<scalar_int8> : signed_integer_scalar_home<std::int8_t> {};

template<>
struct scalar_home<scalar_int16> : signed_integer_scalar_home<std::int16_t> {};

template<>
struct scalar_home<scalar_int32> : signed_integer_scalar_home<std::int32_t> {};

template<>
struct scalar_home<scalar_int64> : signed_integer_scalar_home<std::int64_t> {};

template<>
struct scalar_home<scalar_float> {
    typedef double type;
    typedef policy_pack<builtin_numeric_policy<> > policy;

    static double get(long double number, std::int64_t, std::uint64_t, bool) noexcept {
        return static_cast<double>(number);
    }
};

template<>
struct scalar_home<scalar_timestamp> {
    typedef std::uint64_t type;
    typedef policy_pack<builtin_timestamp_policy> policy;

    static std::uint64_t get(long double, std::int64_t, std::uint64_t timestamp, bool) noexcept {
        return timestamp;
    }
};

template<char DecimalSymbol, bool TrimAsciiWhitespace>
CLASSIFY_SCALAR_FORCE_INLINE bool parse_float_with_decimal(
    const char* first,
    const char* last,
    double& out) noexcept {
    const scalar_span span = parsing::trim_span<TrimAsciiWhitespace>(first, last);
    if (span.first == span.last)
        return false;

    parse_state state(span.first, span.last);
    const char* value_first = parsing::apply_leading_sign(state);
    if (value_first == state.last)
        return false;

    return floating::parse_floating<DecimalSymbol>(state, &out) == floating::floating_parse_status::parsed;
}

} // namespace detail

typedef detail::parse_state parse_state;

/// Ordered set of scalar policies; earlier policies have higher priority.
template<typename... Policies>
using policy_pack = detail::policy_pack<Policies...>;

/// Built-in numeric recognizer for int, float, bigint, and 0x-prefixed hex.
template<char DecimalSymbol = '.', bool IntegralFloatingAsInteger = true>
using builtin_numeric_policy = detail::builtin_numeric_policy<DecimalSymbol, IntegralFloatingAsInteger>;

/// Built-in case-insensitive true/false recognizer.
typedef detail::builtin_bool_policy builtin_bool_policy;

/// Built-in conservative ISO date/date-time recognizer.
typedef detail::builtin_timestamp_policy builtin_timestamp_policy;

/// Default policy pack: numeric, timestamp, then bool.
typedef detail::policy_pack<
    detail::builtin_numeric_policy<>,
    detail::builtin_timestamp_policy,
    detail::builtin_bool_policy> builtin_policy_pack;

/// Alias for the default built-in policy pack.
typedef builtin_policy_pack default_policy_pack;

/// Numeric and bool policy pack with timestamp recognition disabled.
typedef detail::policy_pack<
    detail::builtin_numeric_policy<>,
    detail::builtin_bool_policy> numeric_bool_policy_pack;

/// Numeric-only policy pack for null/string/int/float/bigint inference.
typedef detail::policy_pack<
    detail::builtin_numeric_policy<> > numeric_policy_pack;

/// Classify a pointer span using the selected policy pack and output adapter.
template<
    typename Kind = ScalarKind,
    bool TrimAsciiWhitespace = true,
    typename Output = classify_only_output,
    typename Policy = default_policy_pack>
CLASSIFY_SCALAR_FORCE_INLINE Kind classify_scalar(
    const char* first,
    const char* last,
    Output output = Output(),
    Policy policy = Policy()) noexcept {
    const scalar_span span = detail::parsing::trim_span<TrimAsciiWhitespace>(first, last);
    if (span.first == span.last)
        return !first || !last || last < first
            ? detail::scalar_kind_cast<Kind>(scalar_string)
            : detail::scalar_kind_cast<Kind>(scalar_null);

    detail::parse_state state(span.first, span.last);
    return policy.template dispatch<Kind>(
        detail::dispatch_table<Policy>()[static_cast<unsigned char>(*span.first)],
        state,
        output);
}

/// Classify a string literal using the selected policy pack and output adapter.
template<
    typename Kind = ScalarKind,
    bool TrimAsciiWhitespace = true,
    typename Output = classify_only_output,
    typename Policy = default_policy_pack,
    std::size_t N>
CLASSIFY_SCALAR_FORCE_INLINE Kind classify_scalar(
    const char (&value)[N],
    Output output = Output(),
    Policy policy = Policy()) noexcept {
    return classify_scalar<Kind, TrimAsciiWhitespace>(
        value,
        value + (N ? N - 1 : 0),
        output,
        policy);
}

#ifdef CLASSIFY_SCALAR_HAS_CXX17
/// Classify a string_view using the selected policy pack and output adapter.
template<
    typename Kind = ScalarKind,
    bool TrimAsciiWhitespace = true,
    typename Output = classify_only_output,
    typename Policy = default_policy_pack>
CLASSIFY_SCALAR_FORCE_INLINE Kind classify_scalar(
    std::string_view value,
    Output output = Output(),
    Policy policy = Policy()) noexcept {
    if (value.empty())
        return detail::scalar_kind_cast<Kind>(scalar_null);

    return classify_scalar<Kind, TrimAsciiWhitespace>(
        value.data(),
        value.data() + value.size(),
        output,
        policy);
}

#endif

/// Parse an explicit hexadecimal integer, accepting either bare hex or 0x-prefixed hex.
template<bool TrimAsciiWhitespace = true, typename IntegerType>
CLASSIFY_SCALAR_FORCE_INLINE typename std::enable_if<
    std::is_integral<IntegerType>::value
        && !std::is_same<IntegerType, bool>::value,
    bool>::type parse_hex(
    const char* first,
    const char* last,
    IntegerType& out) noexcept {
    const scalar_span span = detail::parsing::trim_span<TrimAsciiWhitespace>(first, last);
    if (span.first == span.last)
        return false;

    detail::parse_state state(span.first, span.last);
    const char* current = detail::parsing::apply_leading_sign(state);
    if (current == state.last)
        return false;

    std::int64_t parsed = 0;
    if (current + 2 <= state.last && current[0] == '0' && (current[1] == 'x' || current[1] == 'X')) {
        if (current + 2 == state.last)
            return false;

        state.current = current + 1;
        if (!detail::parsing::parse_hex_integer(state, &parsed))
            return false;
    } else if (!detail::parsing::parse_bare_hex_integer(state, &parsed))
        return false;

    if (parsed < static_cast<std::int64_t>(std::numeric_limits<IntegerType>::min()))
        return false;

    if (parsed >= 0
            && static_cast<std::uint64_t>(parsed) > static_cast<std::uint64_t>(std::numeric_limits<IntegerType>::max()))
        return false;

    out = static_cast<IntegerType>(parsed);
    return true;
}

/// String-literal overload for parse_hex().
template<bool TrimAsciiWhitespace = true, std::size_t Size, typename IntegerType>
CLASSIFY_SCALAR_FORCE_INLINE typename std::enable_if<
    std::is_integral<IntegerType>::value
        && !std::is_same<IntegerType, bool>::value,
    bool>::type parse_hex(
    const char (&value)[Size],
    IntegerType& out) noexcept {
    return parse_hex<TrimAsciiWhitespace>(value, value + Size - 1, out);
}

/// Parse an explicit floating-point value with runtime '.' or ',' decimal selection.
template<bool TrimAsciiWhitespace = true>
CLASSIFY_SCALAR_FORCE_INLINE bool parse_float(
    const char* first,
    const char* last,
    double& out,
    const char decimal_symbol = '.') noexcept {
    switch (decimal_symbol) {
    case '.':
        return detail::parse_float_with_decimal<'.', TrimAsciiWhitespace>(first, last, out);
    case ',':
        return detail::parse_float_with_decimal<',', TrimAsciiWhitespace>(first, last, out);
    default:
        return false;
    }
}

/// Parse one built-in scalar kind directly and store its natural C++ value.
template<ScalarKind Kind, bool TrimAsciiWhitespace = true>
CLASSIFY_SCALAR_FORCE_INLINE bool parse_scalar(
    const char* first,
    const char* last,
    typename detail::scalar_home<Kind>::type& out) noexcept {
    long double number = 0;
    std::int64_t integer = 0;
    std::uint64_t timestamp = 0;
    bool boolean = false;

    const ScalarKind kind = classify_scalar<ScalarKind, TrimAsciiWhitespace>(
        first,
        last,
        builtin_output_refs(number, integer, boolean, timestamp),
        typename detail::scalar_home<Kind>::policy());
    if (kind != Kind
            && !(Kind == scalar_float && detail::integer::is_signed_integer_kind(kind))
            && !detail::integer::integer_kind_fits_in(kind, Kind))
        return false;

    out = detail::scalar_home<Kind>::get(number, integer, timestamp, boolean);
    return true;
}

/// String-literal overload for built-in scalar parse_scalar().
template<ScalarKind Kind, bool TrimAsciiWhitespace = true, std::size_t Size>
CLASSIFY_SCALAR_FORCE_INLINE bool parse_scalar(
    const char (&value)[Size],
    typename detail::scalar_home<Kind>::type& out) noexcept {
    return parse_scalar<Kind, TrimAsciiWhitespace>(value, value + Size - 1, out);
}

/// Parse a signed integer directly into the requested C++ integer type.
template<typename IntegerType, bool TrimAsciiWhitespace = true>
CLASSIFY_SCALAR_FORCE_INLINE typename std::enable_if<
    std::is_integral<IntegerType>::value
        && std::is_signed<IntegerType>::value
        && !std::is_same<IntegerType, bool>::value,
    bool>::type parse_scalar(
    const char* first,
    const char* last,
    IntegerType& out) noexcept {
    long double number = 0;
    std::int64_t integer = 0;
    bool boolean = false;

    const ScalarKind kind = classify_scalar<ScalarKind, TrimAsciiWhitespace>(
        first,
        last,
        output_refs(number, integer, boolean),
        numeric_policy_pack());
    if (!detail::integer::is_signed_integer_kind(kind))
        return false;

    if (integer < static_cast<std::int64_t>(std::numeric_limits<IntegerType>::min())
            || integer > static_cast<std::int64_t>(std::numeric_limits<IntegerType>::max()))
        return false;

    out = static_cast<IntegerType>(integer);
    return true;
}

/// String-literal overload for signed integer parse_scalar<T>().
template<typename IntegerType, bool TrimAsciiWhitespace = true, std::size_t Size>
CLASSIFY_SCALAR_FORCE_INLINE typename std::enable_if<
    std::is_integral<IntegerType>::value
        && std::is_signed<IntegerType>::value
        && !std::is_same<IntegerType, bool>::value,
    bool>::type parse_scalar(
    const char (&value)[Size],
    IntegerType& out) noexcept {
    return parse_scalar<IntegerType, TrimAsciiWhitespace>(value, value + Size - 1, out);
}

#ifdef CLASSIFY_SCALAR_HAS_CXX17
/// string_view overload for parse_hex().
template<bool TrimAsciiWhitespace = true, typename IntegerType>
CLASSIFY_SCALAR_FORCE_INLINE typename std::enable_if<
    std::is_integral<IntegerType>::value
        && !std::is_same<IntegerType, bool>::value,
    bool>::type parse_hex(
    std::string_view value,
    IntegerType& out) noexcept {
    return parse_hex<TrimAsciiWhitespace>(value.data(), value.data() + value.size(), out);
}

/// string_view overload for parse_float().
template<bool TrimAsciiWhitespace = true>
CLASSIFY_SCALAR_FORCE_INLINE bool parse_float(
    std::string_view value,
    double& out,
    const char decimal_symbol = '.') noexcept {
    return parse_float<TrimAsciiWhitespace>(value.data(), value.data() + value.size(), out, decimal_symbol);
}

/// string_view overload for parse_scalar().
template<ScalarKind Kind, bool TrimAsciiWhitespace = true>
CLASSIFY_SCALAR_FORCE_INLINE bool parse_scalar(
    std::string_view value,
    typename detail::scalar_home<Kind>::type& out) noexcept {
    return parse_scalar<Kind, TrimAsciiWhitespace>(value.data(), value.data() + value.size(), out);
}

/// string_view overload for signed integer parse_scalar<T>().
template<typename IntegerType, bool TrimAsciiWhitespace = true>
CLASSIFY_SCALAR_FORCE_INLINE typename std::enable_if<
    std::is_integral<IntegerType>::value
        && std::is_signed<IntegerType>::value
        && !std::is_same<IntegerType, bool>::value,
    bool>::type parse_scalar(
    std::string_view value,
    IntegerType& out) noexcept {
    return parse_scalar<IntegerType, TrimAsciiWhitespace>(value.data(), value.data() + value.size(), out);
}
#endif

} // namespace classify_scalar

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // CLASSIFY_SCALAR_SKIP_HEADER


namespace csv {
    /** Enumerates the different CSV field types recognized by this library. */
    enum class DataType {
        UNKNOWN = classify_scalar::scalar_invalid,
        CSV_NULL = classify_scalar::scalar_null,           /**< Empty string */
        CSV_STRING = classify_scalar::scalar_string,       /**< Non-scalar string */
        CSV_BOOL = classify_scalar::scalar_bool,           /**< Boolean value */
        CSV_INT8 = classify_scalar::scalar_int8,           /**< 8-bit integer */
        CSV_INT16 = classify_scalar::scalar_int16,         /**< 16-bit integer */
        CSV_INT32 = classify_scalar::scalar_int32,         /**< 32-bit integer */
        CSV_INT64 = classify_scalar::scalar_int64,         /**< 64-bit integer */
        CSV_BIGINT = classify_scalar::scalar_bigint,       /**< Integer too large to fit in 64 bits */
        CSV_DOUBLE = classify_scalar::scalar_float,        /**< Floating point value */
        CSV_TIMESTAMP = classify_scalar::scalar_timestamp, /**< Timestamp value */
        scalar_custom_begin = classify_scalar::scalar_custom_begin - 1
    };

    static_assert(DataType::CSV_STRING < DataType::CSV_INT8, "String type should come before numeric types.");
    static_assert(DataType::CSV_INT8 < DataType::CSV_INT64, "Smaller integer types should come before larger integer types.");
    static_assert(DataType::CSV_INT64 < DataType::CSV_DOUBLE, "Integer types should come before floating point value types.");

    namespace internals {
        /** Cached scalar classification and parsed value for one CSV field. */
        struct CSVFieldScalar {
            DataType type = DataType::UNKNOWN;
            std::int64_t integer = 0;
            long double floating = 0;
            std::uint64_t timestamp = 0;
            bool boolean = false;
        };

        struct CSVFieldScalarOutput {
            CSVFieldScalar& value;

            template<classify_scalar::ScalarKind Kind>
            typename std::enable_if<
                Kind == classify_scalar::scalar_int8
                || Kind == classify_scalar::scalar_int16
                || Kind == classify_scalar::scalar_int32
                || Kind == classify_scalar::scalar_int64,
                void>::type set(std::int64_t parsed) const noexcept {
                value.integer = parsed;
            }

            template<classify_scalar::ScalarKind Kind>
            typename std::enable_if<Kind == classify_scalar::scalar_float, void>::type set(long double parsed) const noexcept {
                value.floating = parsed;
            }

            template<classify_scalar::ScalarKind Kind>
            typename std::enable_if<Kind == classify_scalar::scalar_bool, void>::type set(bool parsed) const noexcept {
                value.boolean = parsed;
            }

            template<classify_scalar::ScalarKind Kind>
            typename std::enable_if<Kind == classify_scalar::scalar_timestamp, void>::type set(std::uint64_t parsed) const noexcept {
                value.timestamp = parsed;
            }
        };

        inline CSVFieldScalar classify_field_scalar(csv::string_view in) {
            CSVFieldScalar scalar;
            if (in.empty()) {
                scalar.type = DataType::CSV_NULL;
                return scalar;
            }

            const char* first = in.data();
            const char* last = first + in.size();
            typedef classify_scalar::policy_pack<
                classify_scalar::builtin_numeric_policy<'.', false>,
                classify_scalar::builtin_timestamp_policy,
                classify_scalar::builtin_bool_policy
            > csv_field_policy_pack;

            scalar.type = classify_scalar::classify_scalar<
                DataType,
                true>(first, last, CSVFieldScalarOutput{ scalar }, csv_field_policy_pack());
            return scalar;
        }

#ifndef DOXYGEN_SHOULD_SKIP_THIS
        inline DataType data_type(csv::string_view in);
#endif

        /** Classify values using the CSVField scalar policy without materializing parsed output. */
        inline DataType data_type(csv::string_view in) {
            if (in.empty())
                return DataType::CSV_NULL;

            const char* first = in.data();
            const char* last = first + in.size();
            typedef classify_scalar::policy_pack<
                classify_scalar::builtin_numeric_policy<'.', false>,
                classify_scalar::builtin_timestamp_policy,
                classify_scalar::builtin_bool_policy
            > csv_field_policy_pack;

            return classify_scalar::classify_scalar<
                DataType,
                true>(first, last, classify_scalar::classify_only_output(), csv_field_policy_pack());
        }
    }
}

/** @file
 *  Defines an object used to store CSV format settings
 */

#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>


namespace csv {
    namespace internals {
        template<typename RowSink, typename ParsePolicy, typename FieldPolicy, typename RowPolicy>
        class CSVParserCore;
        namespace parser {
            class CSVParserDriverBase;
        }
    }

    class CSVReader;

    /** Determines how to handle rows that are shorter or longer than the majority */
    enum class VariableColumnPolicy {
        THROW = -1,
        IGNORE_ROW = 0,
        KEEP   = 1,
        KEEP_NON_EMPTY = 2
    };

    /** Determines how column name lookups are performed */
    enum class ColumnNamePolicy {
        EXACT = 0,            /**< Case-sensitive match (default) */
        CASE_INSENSITIVE = 1  /**< Case-insensitive match */
    };

    /** Stores the inferred format of a CSV file. */
    struct CSVGuessResult {
        char delim;
        int header_row;
        size_t n_cols;
    };

    /** Stores information about how to parse a CSV file.
     *  Can be used to construct a csv::CSVReader. 
     */
    class CSVFormat {
    public:
        /** Settings for parsing a RFC 4180 CSV file */
        CSVFormat() = default;

        /** Sets the delimiter of the CSV file
         *
         *  Passing a single delimiter disables delimiter inference. Header-row
         *  inference still runs unless header_row()/no_header() was set explicitly
         *  or column_names() was provided.
         *
         *  @throws `std::runtime_error` thrown if trim, quote, or possible delimiting characters overlap
         */
        CSVFormat& delimiter(char delim);

        /** Sets a list of potential delimiters
         *
         *  Passing multiple delimiters enables delimiter inference.
         *  
         *  @throws `std::runtime_error` thrown if trim, quote, or possible delimiting characters overlap
         */
        CSVFormat& delimiter(const std::vector<char> & delim);

        /** Sets the whitespace characters to be trimmed
         *
         *  @throws `std::runtime_error` thrown if trim, quote, or possible delimiting characters overlap
         */
        CSVFormat& trim(const std::vector<char> & ws);

        /** Sets the quote character
         *
         *  @throws `std::runtime_error` thrown if trim, quote, or possible delimiting characters overlap
         */
        CSVFormat& quote(char quote);

        /** Sets the column names.
         *
         *  @note Unsets any values set by header_row()
         */
        CSVFormat& column_names(const std::vector<std::string>& names);

        /** Sets the header row
         *
         *  @param[in] row Row index containing column names; negative means no header row.
         *  @note Unsets any values set by column_names()
         */
        CSVFormat& header_row(int row);

        /** Tells the parser that this CSV has no header row
         *
         *  @note Equivalent to `header_row(-1)`
         *
         */
        CSVFormat& no_header() {
            this->header_row(-1);
            return *this;
        }

        /** Turn quoting on or off */
        CSVFormat& quote(bool use_quote) {
            this->no_quote = !use_quote;
            return *this;
        }

        /** Tells the parser how to handle columns of a different length than the others */
        CONSTEXPR_14 CSVFormat& variable_columns(VariableColumnPolicy policy = VariableColumnPolicy::IGNORE_ROW) {
            this->variable_column_policy = policy;
            return *this;
        }

        /** Tells the parser how to handle columns of a different length than the others */
        CONSTEXPR_14 CSVFormat& variable_columns(bool policy) {
            this->variable_column_policy = (VariableColumnPolicy)policy;
            return *this;
        }

        /** Sets the column name lookup policy.
         *
         *  @param[in] policy  Use ColumnNamePolicy::CASE_INSENSITIVE to allow
         *                     case-insensitive column lookups via CSVRow::operator[]
         *                     and CSVReader::index_of().
         */
        CONSTEXPR_14 CSVFormat& column_names_policy(ColumnNamePolicy policy) {
            this->_column_name_policy = policy;
            return *this;
        }

        /** Sets the chunk size used when reading the CSV
         *
         *  @param[in] size Chunk size in bytes (minimum: CSV_CHUNK_SIZE_FLOOR)
         *  @throws std::invalid_argument if size < CSV_CHUNK_SIZE_FLOOR or size > CSV_CHUNK_SIZE_MAX
         *
         *  Use this when constructing a CSVReader from a filename and individual rows
         *  may exceed the default 10MB chunk size. The value is passed to CSVReader at
         *  construction time, before any data is read.
         */
        CSVFormat& chunk_size(size_t size);

        /** Enable or disable parser threading at runtime.
         *
         *  Threading is enabled by default when the library is compiled with
         *  `CSV_ENABLE_THREADS=1`. Disable it for workloads with many small CSVs
         *  where a background parser thread costs more than it helps.
         *
         *  When disabled, CSVReader parses synchronously on the caller thread and
         *  speculative parallel parsing is also disabled.
         */
        CONSTEXPR_14 CSVFormat& threading(bool enabled = true) {
            this->_threading = enabled;
            return *this;
        }

        /** Set the worker count used by speculative parallel parsing.
         *
         *  A value of 0 means "choose automatically" when the reader is created.
         */
        CONSTEXPR_14 CSVFormat& speculative_parallel_threads(size_t n_threads) {
            this->_speculative_parallel_threads = n_threads;
            return *this;
        }

        /** Set the minimum source size required for speculative parallel parsing. */
        CONSTEXPR_14 CSVFormat& speculative_parallel_min_bytes(size_t bytes) {
            this->_speculative_parallel_min_bytes = bytes;
            return *this;
        }

        /** Enable parser-time scalar classification for typed consumers.
         *
         *  Disabled by default so normal string-only parsing keeps the historical
         *  lazy classification cost model.
         */
        CONSTEXPR_14 CSVFormat& eager_field_classification(bool enabled = true) {
            this->_eager_field_classification = enabled;
            return *this;
        }

#ifndef DOXYGEN_SHOULD_SKIP_THIS
        char get_delim() const {
            // This error should never be received by end users.
            if (this->possible_delimiters.size() > 1) {
                throw std::runtime_error(internals::ERROR_MULTIPLE_DELIMITERS);
            }

            return this->possible_delimiters.at(0);
        }

        CONSTEXPR bool is_quoting_enabled() const { return !this->no_quote; }
        CONSTEXPR char get_quote_char() const { return this->quote_char; }
        CONSTEXPR int get_header() const { return this->header; }
        std::vector<char> get_possible_delims() const { return this->possible_delimiters; }
        std::vector<char> get_trim_chars() const { return this->trim_chars; }
        const std::vector<std::string>& get_col_names() const { return this->col_names; }
        CONSTEXPR VariableColumnPolicy get_variable_column_policy() const { return this->variable_column_policy; }
        CONSTEXPR ColumnNamePolicy get_column_name_policy() const { return this->_column_name_policy; }
        CONSTEXPR size_t get_chunk_size() const { return this->_chunk_size; }
        CONSTEXPR bool is_threading_enabled() const {
#if CSV_ENABLE_THREADS
            return this->_threading;
#else
            return false;
#endif
        }
        CONSTEXPR size_t get_speculative_parallel_threads() const { return this->_speculative_parallel_threads; }
        CONSTEXPR size_t get_speculative_parallel_min_bytes() const { return this->_speculative_parallel_min_bytes; }
        CONSTEXPR bool is_eager_field_classification_enabled() const { return this->_eager_field_classification; }
        CONSTEXPR bool should_use_speculative_parallel(size_t source_size, size_t n_threads) const {
#if CSV_ENABLE_THREADS
            return this->_threading
                && n_threads > 1
                && source_size >= this->_speculative_parallel_min_bytes;
#else
            (void)source_size;
            (void)n_threads;
            return false;
#endif
        }
#endif
        
        /** CSVFormat preset for delimiter inference with header/n_cols inference enabled. */
        CSV_INLINE static CSVFormat guess_csv() {
            CSVFormat format;
            format.delimiter({ ',', '|', '\t', ';', '^' })
                .quote('"');
            // Assign header directly rather than via header_row() so that
            // header_explicitly_set_ remains false — the guesser must be free
            // to detect the real header row at construction time.
            format.header = 0;

            return format;
        }

        bool guess_delim() const {
            return this->possible_delimiters.size() > 1;
        }

        friend CSVReader;
        template<typename RowSink, typename ParsePolicy, typename FieldPolicy, typename RowPolicy>
        friend class internals::CSVParserCore;
        friend internals::parser::CSVParserDriverBase;
        
    private:
        /**< Throws an error if delimiters and trim characters overlap */
        void assert_no_char_overlap();

        /**< Set of possible delimiters */
        std::vector<char> possible_delimiters = { ',' };

        /**< Set of whitespace characters to trim */
        std::vector<char> trim_chars = {};

        /**< Row number with columns; negative means no header row (ignored if col_names is non-empty) */
        int header = 0;

        /**< True if the user explicitly called header_row() or no_header() */
        bool header_explicitly_set_ = false;

        /**< Whether or not to use quoting */
        bool no_quote = false;

        /**< Quote character */
        char quote_char = '"';

        /**< Should be left empty unless file doesn't include header */
        std::vector<std::string> col_names = {};

        /**< True if the user explicitly called column_names() */
        bool col_names_explicitly_set_ = false;

        /**< Allow variable length columns? */
        VariableColumnPolicy variable_column_policy = VariableColumnPolicy::IGNORE_ROW;

        /**< Column name lookup policy */
        ColumnNamePolicy _column_name_policy = ColumnNamePolicy::EXACT;

        /**< Chunk size for reading; passed to CSVReader at construction time */
        size_t _chunk_size = internals::CSV_CHUNK_SIZE_DEFAULT;

        /**< Whether CSVReader may use runtime parser threads */
        bool _threading = true;

        /**< 0 means the reader may choose automatically */
        size_t _speculative_parallel_threads = 0;

        /**< Minimum source size before speculative parallel parsing is considered */
        size_t _speculative_parallel_min_bytes = internals::CSV_SPECULATIVE_PARALLEL_MIN_BYTES;

        /**< Whether to precompute field scalar classifications during parsing */
        bool _eager_field_classification = false;
    };
}


/** @file
 *  @brief Contains CSV parser source-adapter declarations and format helpers.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <fstream>
#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if !defined(__EMSCRIPTEN__)
/* Copyright 2017 https://github.com/mandreyel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

 /* csv-parser local note:
  *
  * This vendored mio.hpp includes a minimal Windows-specific narrowing fix in
  * int64_high/int64_low to avoid -Wconversion failures under strict MinGW builds.
  * Keep this patch small and easy to rebase if/when upstream is updated.
  * 
  * - Vincent La 3/31/2026
  */

#ifndef MIO_MMAP_HEADER
#define MIO_MMAP_HEADER

// #include "mio/page.hpp"
/* Copyright 2017 https://github.com/mandreyel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MIO_PAGE_HEADER
#define MIO_PAGE_HEADER

#ifdef _WIN32
# include <windows.h>
#else
# include <unistd.h>
#endif

namespace mio {

/**
 * This is used by `basic_mmap` to determine whether to create a read-only or
 * a read-write memory mapping.
 */
enum class access_mode
{
    read,
    write
};

/**
 * Determines the operating system's page allocation granularity.
 *
 * On the first call to this function, it invokes the operating system specific syscall
 * to determine the page size, caches the value, and returns it. Any subsequent call to
 * this function serves the cached value, so no further syscalls are made.
 */
inline size_t page_size()
{
    static const size_t page_size = []
    {
#ifdef _WIN32
        SYSTEM_INFO SystemInfo;
        GetSystemInfo(&SystemInfo);
        return SystemInfo.dwAllocationGranularity;
#else
        return sysconf(_SC_PAGE_SIZE);
#endif
    }();
    return page_size;
}

/**
 * Alligns `offset` to the operating's system page size such that it subtracts the
 * difference until the nearest page boundary before `offset`, or does nothing if
 * `offset` is already page aligned.
 */
inline size_t make_offset_page_aligned(size_t offset) noexcept
{
    const size_t page_size_ = page_size();
    // Use integer division to round down to the nearest page alignment.
    return offset / page_size_ * page_size_;
}

} // namespace mio

#endif // MIO_PAGE_HEADER


#include <iterator>
#include <string>
#include <system_error>
#include <cstdint>

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif // WIN32_LEAN_AND_MEAN
# include <windows.h>
#else // ifdef _WIN32
# define INVALID_HANDLE_VALUE -1
#endif // ifdef _WIN32

namespace mio {

// This value may be provided as the `length` parameter to the constructor or
// `map`, in which case a memory mapping of the entire file is created.
enum { map_entire_file = 0 };

#ifdef _WIN32
using file_handle_type = HANDLE;
#else
using file_handle_type = int;
#endif

// This value represents an invalid file handle type. This can be used to
// determine whether `basic_mmap::file_handle` is valid, for example.
const static file_handle_type invalid_handle = INVALID_HANDLE_VALUE;

template<access_mode AccessMode, typename ByteT>
struct basic_mmap
{
    using value_type = ByteT;
    using size_type = size_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using difference_type = std::ptrdiff_t;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using iterator_category = std::random_access_iterator_tag;
    using handle_type = file_handle_type;

    static_assert(sizeof(ByteT) == sizeof(char), "ByteT must be the same size as char.");

private:
    // Points to the first requested byte, and not to the actual start of the mapping.
    pointer data_ = nullptr;

    // Length--in bytes--requested by user (which may not be the length of the
    // full mapping) and the length of the full mapping.
    size_type length_ = 0;
    size_type mapped_length_ = 0;

    // Letting user map a file using both an existing file handle and a path
    // introcudes some complexity (see `is_handle_internal_`).
    // On POSIX, we only need a file handle to create a mapping, while on
    // Windows systems the file handle is necessary to retrieve a file mapping
    // handle, but any subsequent operations on the mapped region must be done
    // through the latter.
    handle_type file_handle_ = INVALID_HANDLE_VALUE;
#ifdef _WIN32
    handle_type file_mapping_handle_ = INVALID_HANDLE_VALUE;
#endif

    // Letting user map a file using both an existing file handle and a path
    // introcudes some complexity in that we must not close the file handle if
    // user provided it, but we must close it if we obtained it using the
    // provided path. For this reason, this flag is used to determine when to
    // close `file_handle_`.
    bool is_handle_internal_;

public:
    /**
     * The default constructed mmap object is in a non-mapped state, that is,
     * any operation that attempts to access nonexistent underlying data will
     * result in undefined behaviour/segmentation faults.
     */
    basic_mmap() = default;

#ifdef __cpp_exceptions
    /**
     * The same as invoking the `map` function, except any error that may occur
     * while establishing the mapping is wrapped in a `std::system_error` and is
     * thrown.
     */
    template<typename String>
    basic_mmap(const String& path, const size_type offset = 0, const size_type length = map_entire_file)
    {
        std::error_code error;
        map(path, offset, length, error);
        if(error) { throw std::system_error(error); }
    }

    /**
     * The same as invoking the `map` function, except any error that may occur
     * while establishing the mapping is wrapped in a `std::system_error` and is
     * thrown.
     */
    basic_mmap(const handle_type handle, const size_type offset = 0, const size_type length = map_entire_file)
    {
        std::error_code error;
        map(handle, offset, length, error);
        if(error) { throw std::system_error(error); }
    }
#endif // __cpp_exceptions

    /**
     * `basic_mmap` has single-ownership semantics, so transferring ownership
     * may only be accomplished by moving the object.
     */
    basic_mmap(const basic_mmap&) = delete;
    basic_mmap(basic_mmap&&);
    basic_mmap& operator=(const basic_mmap&) = delete;
    basic_mmap& operator=(basic_mmap&&);

    /**
     * If this is a read-write mapping, the destructor invokes sync. Regardless
     * of the access mode, unmap is invoked as a final step.
     */
    ~basic_mmap();

    /**
     * On UNIX systems 'file_handle' and 'mapping_handle' are the same. On Windows,
     * however, a mapped region of a file gets its own handle, which is returned by
     * 'mapping_handle'.
     */
    handle_type file_handle() const noexcept { return file_handle_; }
    handle_type mapping_handle() const noexcept;

    /** Returns whether a valid memory mapping has been created. */
    bool is_open() const noexcept { return file_handle_ != invalid_handle; }

    /**
     * Returns true if no mapping was established, that is, conceptually the
     * same as though the length that was mapped was 0. This function is
     * provided so that this class has Container semantics.
     */
    bool empty() const noexcept { return length() == 0; }

    /** Returns true if a mapping was established. */
    bool is_mapped() const noexcept;

    /**
     * `size` and `length` both return the logical length, i.e. the number of bytes
     * user requested to be mapped, while `mapped_length` returns the actual number of
     * bytes that were mapped which is a multiple of the underlying operating system's
     * page allocation granularity.
     */
    size_type size() const noexcept { return length(); }
    size_type length() const noexcept { return length_; }
    size_type mapped_length() const noexcept { return mapped_length_; }

    /** Returns the offset relative to the start of the mapping. */
    size_type mapping_offset() const noexcept
    {
        return mapped_length_ - length_;
    }

    /**
     * Returns a pointer to the first requested byte, or `nullptr` if no memory mapping
     * exists.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > pointer data() noexcept { return data_; }
    const_pointer data() const noexcept { return data_; }

    /**
     * Returns an iterator to the first requested byte, if a valid memory mapping
     * exists, otherwise this function call is undefined behaviour.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > iterator begin() noexcept { return data(); }
    const_iterator begin() const noexcept { return data(); }
    const_iterator cbegin() const noexcept { return data(); }

    /**
     * Returns an iterator one past the last requested byte, if a valid memory mapping
     * exists, otherwise this function call is undefined behaviour.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > iterator end() noexcept { return data() + length(); }
    const_iterator end() const noexcept { return data() + length(); }
    const_iterator cend() const noexcept { return data() + length(); }

    /**
     * Returns a reverse iterator to the last memory mapped byte, if a valid
     * memory mapping exists, otherwise this function call is undefined
     * behaviour.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept
    { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept
    { return const_reverse_iterator(end()); }

    /**
     * Returns a reverse iterator past the first mapped byte, if a valid memory
     * mapping exists, otherwise this function call is undefined behaviour.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept
    { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept
    { return const_reverse_iterator(begin()); }

    /**
     * Returns a reference to the `i`th byte from the first requested byte (as returned
     * by `data`). If this is invoked when no valid memory mapping has been created
     * prior to this call, undefined behaviour ensues.
     */
    reference operator[](const size_type i) noexcept { return data_[i]; }
    const_reference operator[](const size_type i) const noexcept { return data_[i]; }

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
     * reason is reported via `error` and the object remains in a state as if this
     * function hadn't been called.
     *
     * `path`, which must be a path to an existing file, is used to retrieve a file
     * handle (which is closed when the object destructs or `unmap` is called), which is
     * then used to memory map the requested region. Upon failure, `error` is set to
     * indicate the reason and the object remains in an unmapped state.
     *
     * `offset` is the number of bytes, relative to the start of the file, where the
     * mapping should begin. When specifying it, there is no need to worry about
     * providing a value that is aligned with the operating system's page allocation
     * granularity. This is adjusted by the implementation such that the first requested
     * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
     * `offset` from the start of the file.
     *
     * `length` is the number of bytes to map. It may be `map_entire_file`, in which
     * case a mapping of the entire file is created.
     */
    template<typename String>
    void map(const String& path, const size_type offset,
            const size_type length, std::error_code& error);

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
     * reason is reported via `error` and the object remains in a state as if this
     * function hadn't been called.
     *
     * `path`, which must be a path to an existing file, is used to retrieve a file
     * handle (which is closed when the object destructs or `unmap` is called), which is
     * then used to memory map the requested region. Upon failure, `error` is set to
     * indicate the reason and the object remains in an unmapped state.
     * 
     * The entire file is mapped.
     */
    template<typename String>
    void map(const String& path, std::error_code& error)
    {
        map(path, 0, map_entire_file, error);
    }

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is
     * unsuccesful, the reason is reported via `error` and the object remains in
     * a state as if this function hadn't been called.
     *
     * `handle`, which must be a valid file handle, which is used to memory map the
     * requested region. Upon failure, `error` is set to indicate the reason and the
     * object remains in an unmapped state.
     *
     * `offset` is the number of bytes, relative to the start of the file, where the
     * mapping should begin. When specifying it, there is no need to worry about
     * providing a value that is aligned with the operating system's page allocation
     * granularity. This is adjusted by the implementation such that the first requested
     * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
     * `offset` from the start of the file.
     *
     * `length` is the number of bytes to map. It may be `map_entire_file`, in which
     * case a mapping of the entire file is created.
     */
    void map(const handle_type handle, const size_type offset,
            const size_type length, std::error_code& error);

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is
     * unsuccesful, the reason is reported via `error` and the object remains in
     * a state as if this function hadn't been called.
     *
     * `handle`, which must be a valid file handle, which is used to memory map the
     * requested region. Upon failure, `error` is set to indicate the reason and the
     * object remains in an unmapped state.
     * 
     * The entire file is mapped.
     */
    void map(const handle_type handle, std::error_code& error)
    {
        map(handle, 0, map_entire_file, error);
    }

    /**
     * If a valid memory mapping has been created prior to this call, this call
     * instructs the kernel to unmap the memory region and disassociate this object
     * from the file.
     *
     * The file handle associated with the file that is mapped is only closed if the
     * mapping was created using a file path. If, on the other hand, an existing
     * file handle was used to create the mapping, the file handle is not closed.
     */
    void unmap();

    void swap(basic_mmap& other);

    /** Flushes the memory mapped page to disk. Errors are reported via `error`. */
    template<access_mode A = AccessMode>
    typename std::enable_if<A == access_mode::write, void>::type
    sync(std::error_code& error);

    /**
     * All operators compare the address of the first byte and size of the two mapped
     * regions.
     */

private:
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > pointer get_mapping_start() noexcept
    {
        return !data() ? nullptr : data() - mapping_offset();
    }

    const_pointer get_mapping_start() const noexcept
    {
        return !data() ? nullptr : data() - mapping_offset();
    }

    /**
     * The destructor syncs changes to disk if `AccessMode` is `write`, but not
     * if it's `read`, but since the destructor cannot be templated, we need to
     * do SFINAE in a dedicated function, where one syncs and the other is a noop.
     */
    template<access_mode A = AccessMode>
    typename std::enable_if<A == access_mode::write, void>::type
    conditional_sync();
    template<access_mode A = AccessMode>
    typename std::enable_if<A == access_mode::read, void>::type conditional_sync();
};

template<access_mode AccessMode, typename ByteT>
bool operator==(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b);

template<access_mode AccessMode, typename ByteT>
bool operator!=(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b);

template<access_mode AccessMode, typename ByteT>
bool operator<(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b);

template<access_mode AccessMode, typename ByteT>
bool operator<=(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b);

template<access_mode AccessMode, typename ByteT>
bool operator>(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b);

template<access_mode AccessMode, typename ByteT>
bool operator>=(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b);

/**
 * This is the basis for all read-only mmap objects and should be preferred over
 * directly using `basic_mmap`.
 */
template<typename ByteT>
using basic_mmap_source = basic_mmap<access_mode::read, ByteT>;

/**
 * This is the basis for all read-write mmap objects and should be preferred over
 * directly using `basic_mmap`.
 */
template<typename ByteT>
using basic_mmap_sink = basic_mmap<access_mode::write, ByteT>;

/**
 * These aliases cover the most common use cases, both representing a raw byte stream
 * (either with a char or an unsigned char/uint8_t).
 */
using mmap_source = basic_mmap_source<char>;
using ummap_source = basic_mmap_source<unsigned char>;

using mmap_sink = basic_mmap_sink<char>;
using ummap_sink = basic_mmap_sink<unsigned char>;

/**
 * Convenience factory method that constructs a mapping for any `basic_mmap` or
 * `basic_mmap` type.
 */
template<
    typename MMap,
    typename MappingToken
> MMap make_mmap(const MappingToken& token,
        int64_t offset, int64_t length, std::error_code& error)
{
    MMap mmap;
    mmap.map(token, offset, length, error);
    return mmap;
}

/**
 * Convenience factory method.
 *
 * MappingToken may be a String (`std::string`, `std::string_view`, `const char*`,
 * `std::filesystem::path`, `std::vector<char>`, or similar), or a
 * `mmap_source::handle_type`.
 */
template<typename MappingToken>
mmap_source make_mmap_source(const MappingToken& token, mmap_source::size_type offset,
        mmap_source::size_type length, std::error_code& error)
{
    return make_mmap<mmap_source>(token, offset, length, error);
}

template<typename MappingToken>
mmap_source make_mmap_source(const MappingToken& token, std::error_code& error)
{
    return make_mmap_source(token, 0, map_entire_file, error);
}

/**
 * Convenience factory method.
 *
 * MappingToken may be a String (`std::string`, `std::string_view`, `const char*`,
 * `std::filesystem::path`, `std::vector<char>`, or similar), or a
 * `mmap_sink::handle_type`.
 */
template<typename MappingToken>
mmap_sink make_mmap_sink(const MappingToken& token, mmap_sink::size_type offset,
        mmap_sink::size_type length, std::error_code& error)
{
    return make_mmap<mmap_sink>(token, offset, length, error);
}

template<typename MappingToken>
mmap_sink make_mmap_sink(const MappingToken& token, std::error_code& error)
{
    return make_mmap_sink(token, 0, map_entire_file, error);
}

} // namespace mio

// #include "detail/mmap.ipp"
/* Copyright 2017 https://github.com/mandreyel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MIO_BASIC_MMAP_IMPL
#define MIO_BASIC_MMAP_IMPL

// #include "mio/mmap.hpp"

// #include "mio/page.hpp"

// #include "mio/detail/string_util.hpp"
/* Copyright 2017 https://github.com/mandreyel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MIO_STRING_UTIL_HEADER
#define MIO_STRING_UTIL_HEADER

#include <type_traits>

namespace mio {
namespace detail {

template<
    typename S,
    typename C = typename std::decay<S>::type,
    typename = decltype(std::declval<C>().data()),
    typename = typename std::enable_if<
        std::is_same<typename C::value_type, char>::value
#ifdef _WIN32
        || std::is_same<typename C::value_type, wchar_t>::value
#endif
    >::type
> struct char_type_helper {
    using type = typename C::value_type;
};

template<class T>
struct char_type {
    using type = typename char_type_helper<T>::type;
};

// TODO: can we avoid this brute force approach?
template<>
struct char_type<char*> {
    using type = char;
};

template<>
struct char_type<const char*> {
    using type = char;
};

template<size_t N>
struct char_type<char[N]> {
    using type = char;
};

template<size_t N>
struct char_type<const char[N]> {
    using type = char;
};

#ifdef _WIN32
template<>
struct char_type<wchar_t*> {
    using type = wchar_t;
};

template<>
struct char_type<const wchar_t*> {
    using type = wchar_t;
};

template<size_t N>
struct char_type<wchar_t[N]> {
    using type = wchar_t;
};

template<size_t N>
struct char_type<const wchar_t[N]> {
    using type = wchar_t;
};
#endif // _WIN32

template<typename CharT, typename S>
struct is_c_str_helper
{
    static constexpr bool value = std::is_same<
        CharT*,
        // TODO: I'm so sorry for this... Can this be made cleaner?
        typename std::add_pointer<
            typename std::remove_cv<
                typename std::remove_pointer<
                    typename std::decay<
                        S
                    >::type
                >::type
            >::type
        >::type
    >::value;
};

template<typename S>
struct is_c_str
{
    static constexpr bool value = is_c_str_helper<char, S>::value;
};

#ifdef _WIN32
template<typename S>
struct is_c_wstr
{
    static constexpr bool value = is_c_str_helper<wchar_t, S>::value;
};
#endif // _WIN32

template<typename S>
struct is_c_str_or_c_wstr
{
    static constexpr bool value = is_c_str<S>::value
#ifdef _WIN32
        || is_c_wstr<S>::value
#endif
        ;
};

template<
    typename String,
    typename = decltype(std::declval<String>().data()),
    typename = typename std::enable_if<!is_c_str_or_c_wstr<String>::value>::type
> const typename char_type<String>::type* c_str(const String& path)
{
    return path.data();
}

template<
    typename String,
    typename = decltype(std::declval<String>().empty()),
    typename = typename std::enable_if<!is_c_str_or_c_wstr<String>::value>::type
> bool empty(const String& path)
{
    return path.empty();
}

template<
    typename String,
    typename = typename std::enable_if<is_c_str_or_c_wstr<String>::value>::type
> const typename char_type<String>::type* c_str(String path)
{
    return path;
}

template<
    typename String,
    typename = typename std::enable_if<is_c_str_or_c_wstr<String>::value>::type
> bool empty(String path)
{
    return !path || (*path == 0);
}

} // namespace detail
} // namespace mio

#endif // MIO_STRING_UTIL_HEADER


#include <algorithm>

#ifndef _WIN32
# include <unistd.h>
# include <fcntl.h>
# include <sys/mman.h>
# include <sys/stat.h>
#endif

namespace mio {
namespace detail {

#ifdef _WIN32
namespace win {

/** Returns the 4 upper bytes of an 8-byte integer. */
inline DWORD int64_high(int64_t n) noexcept
{
    return static_cast<DWORD>(static_cast<uint64_t>(n) >> 32);
}

/** Returns the 4 lower bytes of an 8-byte integer. */
inline DWORD int64_low(int64_t n) noexcept
{
    return static_cast<DWORD>(static_cast<uint64_t>(n) & 0xffffffffULL);
}

template<
    typename String,
    typename = typename std::enable_if<
        std::is_same<typename char_type<String>::type, char>::value
    >::type
> file_handle_type open_file_helper(const String& path, const access_mode mode)
{
    return ::CreateFileA(c_str(path),
            mode == access_mode::read ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            0);
}

template<typename String>
typename std::enable_if<
    std::is_same<typename char_type<String>::type, wchar_t>::value,
    file_handle_type
>::type open_file_helper(const String& path, const access_mode mode)
{
    return ::CreateFileW(c_str(path),
            mode == access_mode::read ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            0);
}

} // win
#endif // _WIN32

/**
 * Returns the last platform specific system error (errno on POSIX and
 * GetLastError on Win) as a `std::error_code`.
 */
inline std::error_code last_error() noexcept
{
    std::error_code error;
#ifdef _WIN32
    error.assign(GetLastError(), std::system_category());
#else
    error.assign(errno, std::system_category());
#endif
    return error;
}

template<typename String>
file_handle_type open_file(const String& path, const access_mode mode,
        std::error_code& error)
{
    error.clear();
    if(detail::empty(path))
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return invalid_handle;
    }
#ifdef _WIN32
    const auto handle = win::open_file_helper(path, mode);
#else // POSIX
    const auto handle = ::open(c_str(path),
            mode == access_mode::read ? O_RDONLY : O_RDWR);
#endif
    if(handle == invalid_handle)
    {
        error = detail::last_error();
    }
    return handle;
}

inline size_t query_file_size(file_handle_type handle, std::error_code& error)
{
    error.clear();
#ifdef _WIN32
    LARGE_INTEGER file_size;
    if(::GetFileSizeEx(handle, &file_size) == 0)
    {
        error = detail::last_error();
        return 0;
    }
	return static_cast<int64_t>(file_size.QuadPart);
#else // POSIX
    struct stat sbuf;
    if(::fstat(handle, &sbuf) == -1)
    {
        error = detail::last_error();
        return 0;
    }
    return sbuf.st_size;
#endif
}

struct mmap_context
{
    char* data;
    int64_t length;
    int64_t mapped_length;
#ifdef _WIN32
    file_handle_type file_mapping_handle;
#endif
};

inline mmap_context memory_map(const file_handle_type file_handle, const int64_t offset,
    const int64_t length, const access_mode mode, std::error_code& error)
{
    const int64_t aligned_offset = make_offset_page_aligned(offset);
    const int64_t length_to_map = offset - aligned_offset + length;
#ifdef _WIN32
    const int64_t max_file_size = offset + length;
    const auto file_mapping_handle = ::CreateFileMapping(
            file_handle,
            0,
            mode == access_mode::read ? PAGE_READONLY : PAGE_READWRITE,
            win::int64_high(max_file_size),
            win::int64_low(max_file_size),
            0);
    if(file_mapping_handle == invalid_handle)
    {
        error = detail::last_error();
        return {};
    }
    char* mapping_start = static_cast<char*>(::MapViewOfFile(
            file_mapping_handle,
            mode == access_mode::read ? FILE_MAP_READ : FILE_MAP_WRITE,
            win::int64_high(aligned_offset),
            win::int64_low(aligned_offset),
            length_to_map));
    if(mapping_start == nullptr)
    {
        // Close file handle if mapping it failed.
        ::CloseHandle(file_mapping_handle);
        error = detail::last_error();
        return {};
    }
#else // POSIX
    char* mapping_start = static_cast<char*>(::mmap(
            0, // Don't give hint as to where to map.
            length_to_map,
            mode == access_mode::read ? PROT_READ : PROT_WRITE,
            MAP_SHARED,
            file_handle,
            aligned_offset));
    if(mapping_start == MAP_FAILED)
    {
        error = detail::last_error();
        return {};
    }
#endif
    mmap_context ctx;
    ctx.data = mapping_start + offset - aligned_offset;
    ctx.length = length;
    ctx.mapped_length = length_to_map;
#ifdef _WIN32
    ctx.file_mapping_handle = file_mapping_handle;
#endif
    return ctx;
}

} // namespace detail

// -- basic_mmap --

template<access_mode AccessMode, typename ByteT>
basic_mmap<AccessMode, ByteT>::~basic_mmap()
{
    conditional_sync();
    unmap();
}

template<access_mode AccessMode, typename ByteT>
basic_mmap<AccessMode, ByteT>::basic_mmap(basic_mmap&& other)
    : data_(std::move(other.data_))
    , length_(std::move(other.length_))
    , mapped_length_(std::move(other.mapped_length_))
    , file_handle_(std::move(other.file_handle_))
#ifdef _WIN32
    , file_mapping_handle_(std::move(other.file_mapping_handle_))
#endif
    , is_handle_internal_(std::move(other.is_handle_internal_))
{
    other.data_ = nullptr;
    other.length_ = other.mapped_length_ = 0;
    other.file_handle_ = invalid_handle;
#ifdef _WIN32
    other.file_mapping_handle_ = invalid_handle;
#endif
}

template<access_mode AccessMode, typename ByteT>
basic_mmap<AccessMode, ByteT>&
basic_mmap<AccessMode, ByteT>::operator=(basic_mmap&& other)
{
    if(this != &other)
    {
        // First the existing mapping needs to be removed.
        unmap();
        data_ = std::move(other.data_);
        length_ = std::move(other.length_);
        mapped_length_ = std::move(other.mapped_length_);
        file_handle_ = std::move(other.file_handle_);
#ifdef _WIN32
        file_mapping_handle_ = std::move(other.file_mapping_handle_);
#endif
        is_handle_internal_ = std::move(other.is_handle_internal_);

        // The moved from basic_mmap's fields need to be reset, because
        // otherwise other's destructor will unmap the same mapping that was
        // just moved into this.
        other.data_ = nullptr;
        other.length_ = other.mapped_length_ = 0;
        other.file_handle_ = invalid_handle;
#ifdef _WIN32
        other.file_mapping_handle_ = invalid_handle;
#endif
        other.is_handle_internal_ = false;
    }
    return *this;
}

template<access_mode AccessMode, typename ByteT>
typename basic_mmap<AccessMode, ByteT>::handle_type
basic_mmap<AccessMode, ByteT>::mapping_handle() const noexcept
{
#ifdef _WIN32
    return file_mapping_handle_;
#else
    return file_handle_;
#endif
}

template<access_mode AccessMode, typename ByteT>
template<typename String>
void basic_mmap<AccessMode, ByteT>::map(const String& path, const size_type offset,
        const size_type length, std::error_code& error)
{
    error.clear();
    if(detail::empty(path))
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    const auto handle = detail::open_file(path, AccessMode, error);
    if(error)
    {
        return;
    }

    map(handle, offset, length, error);
    // This MUST be after the call to map, as that sets this to true.
    if(!error)
    {
        is_handle_internal_ = true;
    }
}

template<access_mode AccessMode, typename ByteT>
void basic_mmap<AccessMode, ByteT>::map(const handle_type handle,
        const size_type offset, const size_type length, std::error_code& error)
{
    error.clear();
    if(handle == invalid_handle)
    {
        error = std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }

    const auto file_size = detail::query_file_size(handle, error);
    if(error)
    {
        return;
    }

    if(offset + length > file_size)
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return;
    }

    const auto ctx = detail::memory_map(handle, offset,
            length == map_entire_file ? (file_size - offset) : length,
            AccessMode, error);
    if(!error)
    {
        // We must unmap the previous mapping that may have existed prior to this call.
        // Note that this must only be invoked after a new mapping has been created in
        // order to provide the strong guarantee that, should the new mapping fail, the
        // `map` function leaves this instance in a state as though the function had
        // never been invoked.
        unmap();
        file_handle_ = handle;
        is_handle_internal_ = false;
        data_ = reinterpret_cast<pointer>(ctx.data);
        length_ = ctx.length;
        mapped_length_ = ctx.mapped_length;
#ifdef _WIN32
        file_mapping_handle_ = ctx.file_mapping_handle;
#endif
    }
}

template<access_mode AccessMode, typename ByteT>
template<access_mode A>
typename std::enable_if<A == access_mode::write, void>::type
basic_mmap<AccessMode, ByteT>::sync(std::error_code& error)
{
    error.clear();
    if(!is_open())
    {
        error = std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }

    if(data())
    {
#ifdef _WIN32
        if(::FlushViewOfFile(get_mapping_start(), mapped_length_) == 0
           || ::FlushFileBuffers(file_handle_) == 0)
#else // POSIX
        if(::msync(get_mapping_start(), mapped_length_, MS_SYNC) != 0)
#endif
        {
            error = detail::last_error();
            return;
        }
    }
#ifdef _WIN32
    if(::FlushFileBuffers(file_handle_) == 0)
    {
        error = detail::last_error();
    }
#endif
}

template<access_mode AccessMode, typename ByteT>
void basic_mmap<AccessMode, ByteT>::unmap()
{
    if(!is_open()) { return; }
    // TODO do we care about errors here?
#ifdef _WIN32
    if(is_mapped())
    {
        ::UnmapViewOfFile(get_mapping_start());
        ::CloseHandle(file_mapping_handle_);
    }
#else // POSIX
    if(data_) { ::munmap(const_cast<pointer>(get_mapping_start()), mapped_length_); }
#endif

    // If `file_handle_` was obtained by our opening it (when map is called with
    // a path, rather than an existing file handle), we need to close it,
    // otherwise it must not be closed as it may still be used outside this
    // instance.
    if(is_handle_internal_)
    {
#ifdef _WIN32
        ::CloseHandle(file_handle_);
#else // POSIX
        ::close(file_handle_);
#endif
    }

    // Reset fields to their default values.
    data_ = nullptr;
    length_ = mapped_length_ = 0;
    file_handle_ = invalid_handle;
#ifdef _WIN32
    file_mapping_handle_ = invalid_handle;
#endif
}

template<access_mode AccessMode, typename ByteT>
bool basic_mmap<AccessMode, ByteT>::is_mapped() const noexcept
{
#ifdef _WIN32
    return file_mapping_handle_ != invalid_handle;
#else // POSIX
    return is_open();
#endif
}

template<access_mode AccessMode, typename ByteT>
void basic_mmap<AccessMode, ByteT>::swap(basic_mmap& other)
{
    if(this != &other)
    {
        using std::swap;
        swap(data_, other.data_);
        swap(file_handle_, other.file_handle_);
#ifdef _WIN32
        swap(file_mapping_handle_, other.file_mapping_handle_);
#endif
        swap(length_, other.length_);
        swap(mapped_length_, other.mapped_length_);
        swap(is_handle_internal_, other.is_handle_internal_);
    }
}

template<access_mode AccessMode, typename ByteT>
template<access_mode A>
typename std::enable_if<A == access_mode::write, void>::type
basic_mmap<AccessMode, ByteT>::conditional_sync()
{
    // This is invoked from the destructor, so not much we can do about
    // failures here.
    std::error_code ec;
    sync(ec);
}

template<access_mode AccessMode, typename ByteT>
template<access_mode A>
typename std::enable_if<A == access_mode::read, void>::type
basic_mmap<AccessMode, ByteT>::conditional_sync()
{
    // noop
}

template<access_mode AccessMode, typename ByteT>
bool operator==(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b)
{
    return a.data() == b.data()
        && a.size() == b.size();
}

template<access_mode AccessMode, typename ByteT>
bool operator!=(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b)
{
    return !(a == b);
}

template<access_mode AccessMode, typename ByteT>
bool operator<(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b)
{
    if(a.data() == b.data()) { return a.size() < b.size(); }
    return a.data() < b.data();
}

template<access_mode AccessMode, typename ByteT>
bool operator<=(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b)
{
    return !(a > b);
}

template<access_mode AccessMode, typename ByteT>
bool operator>(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b)
{
    if(a.data() == b.data()) { return a.size() > b.size(); }
    return a.data() > b.data();
}

template<access_mode AccessMode, typename ByteT>
bool operator>=(const basic_mmap<AccessMode, ByteT>& a,
        const basic_mmap<AccessMode, ByteT>& b)
{
    return !(a < b);
}

} // namespace mio

#endif // MIO_BASIC_MMAP_IMPL


#endif // MIO_MMAP_HEADER
/* Copyright 2017 https://github.com/mandreyel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MIO_PAGE_HEADER
#define MIO_PAGE_HEADER

#ifdef _WIN32
# include <windows.h>
#else
# include <unistd.h>
#endif

namespace mio {

/**
 * This is used by `basic_mmap` to determine whether to create a read-only or
 * a read-write memory mapping.
 */
enum class access_mode
{
    read,
    write
};

/**
 * Determines the operating system's page allocation granularity.
 *
 * On the first call to this function, it invokes the operating system specific syscall
 * to determine the page size, caches the value, and returns it. Any subsequent call to
 * this function serves the cached value, so no further syscalls are made.
 */
inline size_t page_size()
{
    static const size_t page_size = []
    {
#ifdef _WIN32
        SYSTEM_INFO SystemInfo;
        GetSystemInfo(&SystemInfo);
        return SystemInfo.dwAllocationGranularity;
#else
        return sysconf(_SC_PAGE_SIZE);
#endif
    }();
    return page_size;
}

/**
 * Alligns `offset` to the operating's system page size such that it subtracts the
 * difference until the nearest page boundary before `offset`, or does nothing if
 * `offset` is already page aligned.
 */
inline size_t make_offset_page_aligned(size_t offset) noexcept
{
    const size_t page_size_ = page_size();
    // Use integer division to round down to the nearest page alignment.
    return offset / page_size_ * page_size_;
}

} // namespace mio

#endif // MIO_PAGE_HEADER
/* Copyright 2017 https://github.com/mandreyel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MIO_SHARED_MMAP_HEADER
#define MIO_SHARED_MMAP_HEADER

// #include "mio/mmap.hpp"


#include <system_error> // std::error_code
#include <memory> // std::shared_ptr

namespace mio {

/**
 * Exposes (nearly) the same interface as `basic_mmap`, but endowes it with
 * `std::shared_ptr` semantics.
 *
 * This is not the default behaviour of `basic_mmap` to avoid allocating on the heap if
 * shared semantics are not required.
 */
template<
    access_mode AccessMode,
    typename ByteT
> class basic_shared_mmap
{
    using impl_type = basic_mmap<AccessMode, ByteT>;
    std::shared_ptr<impl_type> pimpl_;

public:
    using value_type = typename impl_type::value_type;
    using size_type = typename impl_type::size_type;
    using reference = typename impl_type::reference;
    using const_reference = typename impl_type::const_reference;
    using pointer = typename impl_type::pointer;
    using const_pointer = typename impl_type::const_pointer;
    using difference_type = typename impl_type::difference_type;
    using iterator = typename impl_type::iterator;
    using const_iterator = typename impl_type::const_iterator;
    using reverse_iterator = typename impl_type::reverse_iterator;
    using const_reverse_iterator = typename impl_type::const_reverse_iterator;
    using iterator_category = typename impl_type::iterator_category;
    using handle_type = typename impl_type::handle_type;
    using mmap_type = impl_type;

    basic_shared_mmap() = default;
    basic_shared_mmap(const basic_shared_mmap&) = default;
    basic_shared_mmap& operator=(const basic_shared_mmap&) = default;
    basic_shared_mmap(basic_shared_mmap&&) = default;
    basic_shared_mmap& operator=(basic_shared_mmap&&) = default;

    /** Takes ownership of an existing mmap object. */
    basic_shared_mmap(mmap_type&& mmap)
        : pimpl_(std::make_shared<mmap_type>(std::move(mmap)))
    {}

    /** Takes ownership of an existing mmap object. */
    basic_shared_mmap& operator=(mmap_type&& mmap)
    {
        pimpl_ = std::make_shared<mmap_type>(std::move(mmap));
        return *this;
    }

    /** Initializes this object with an already established shared mmap. */
    basic_shared_mmap(std::shared_ptr<mmap_type> mmap) : pimpl_(std::move(mmap)) {}

    /** Initializes this object with an already established shared mmap. */
    basic_shared_mmap& operator=(std::shared_ptr<mmap_type> mmap)
    {
        pimpl_ = std::move(mmap);
        return *this;
    }

#ifdef __cpp_exceptions
    /**
     * The same as invoking the `map` function, except any error that may occur
     * while establishing the mapping is wrapped in a `std::system_error` and is
     * thrown.
     */
    template<typename String>
    basic_shared_mmap(const String& path, const size_type offset = 0, const size_type length = map_entire_file)
    {
        std::error_code error;
        map(path, offset, length, error);
        if(error) { throw std::system_error(error); }
    }

    /**
     * The same as invoking the `map` function, except any error that may occur
     * while establishing the mapping is wrapped in a `std::system_error` and is
     * thrown.
     */
    basic_shared_mmap(const handle_type handle, const size_type offset = 0, const size_type length = map_entire_file)
    {
        std::error_code error;
        map(handle, offset, length, error);
        if(error) { throw std::system_error(error); }
    }
#endif // __cpp_exceptions

    /**
     * If this is a read-write mapping and the last reference to the mapping,
     * the destructor invokes sync. Regardless of the access mode, unmap is
     * invoked as a final step.
     */
    ~basic_shared_mmap() = default;

    /** Returns the underlying `std::shared_ptr` instance that holds the mmap. */
    std::shared_ptr<mmap_type> get_shared_ptr() { return pimpl_; }

    /**
     * On UNIX systems 'file_handle' and 'mapping_handle' are the same. On Windows,
     * however, a mapped region of a file gets its own handle, which is returned by
     * 'mapping_handle'.
     */
    handle_type file_handle() const noexcept
    {
        return pimpl_ ? pimpl_->file_handle() : invalid_handle;
    }

    handle_type mapping_handle() const noexcept
    {
        return pimpl_ ? pimpl_->mapping_handle() : invalid_handle;
    }

    /** Returns whether a valid memory mapping has been created. */
    bool is_open() const noexcept { return pimpl_ && pimpl_->is_open(); }

    /**
     * Returns true if no mapping was established, that is, conceptually the
     * same as though the length that was mapped was 0. This function is
     * provided so that this class has Container semantics.
     */
    bool empty() const noexcept { return !pimpl_ || pimpl_->empty(); }

    /**
     * `size` and `length` both return the logical length, i.e. the number of bytes
     * user requested to be mapped, while `mapped_length` returns the actual number of
     * bytes that were mapped which is a multiple of the underlying operating system's
     * page allocation granularity.
     */
    size_type size() const noexcept { return pimpl_ ? pimpl_->length() : 0; }
    size_type length() const noexcept { return pimpl_ ? pimpl_->length() : 0; }
    size_type mapped_length() const noexcept
    {
        return pimpl_ ? pimpl_->mapped_length() : 0;
    }

    /**
     * Returns a pointer to the first requested byte, or `nullptr` if no memory mapping
     * exists.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > pointer data() noexcept { return pimpl_->data(); }
    const_pointer data() const noexcept { return pimpl_ ? pimpl_->data() : nullptr; }

    /**
     * Returns an iterator to the first requested byte, if a valid memory mapping
     * exists, otherwise this function call is undefined behaviour.
     */
    iterator begin() noexcept { return pimpl_->begin(); }
    const_iterator begin() const noexcept { return pimpl_->begin(); }
    const_iterator cbegin() const noexcept { return pimpl_->cbegin(); }

    /**
     * Returns an iterator one past the last requested byte, if a valid memory mapping
     * exists, otherwise this function call is undefined behaviour.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > iterator end() noexcept { return pimpl_->end(); }
    const_iterator end() const noexcept { return pimpl_->end(); }
    const_iterator cend() const noexcept { return pimpl_->cend(); }

    /**
     * Returns a reverse iterator to the last memory mapped byte, if a valid
     * memory mapping exists, otherwise this function call is undefined
     * behaviour.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > reverse_iterator rbegin() noexcept { return pimpl_->rbegin(); }
    const_reverse_iterator rbegin() const noexcept { return pimpl_->rbegin(); }
    const_reverse_iterator crbegin() const noexcept { return pimpl_->crbegin(); }

    /**
     * Returns a reverse iterator past the first mapped byte, if a valid memory
     * mapping exists, otherwise this function call is undefined behaviour.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > reverse_iterator rend() noexcept { return pimpl_->rend(); }
    const_reverse_iterator rend() const noexcept { return pimpl_->rend(); }
    const_reverse_iterator crend() const noexcept { return pimpl_->crend(); }

    /**
     * Returns a reference to the `i`th byte from the first requested byte (as returned
     * by `data`). If this is invoked when no valid memory mapping has been created
     * prior to this call, undefined behaviour ensues.
     */
    reference operator[](const size_type i) noexcept { return (*pimpl_)[i]; }
    const_reference operator[](const size_type i) const noexcept { return (*pimpl_)[i]; }

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
     * reason is reported via `error` and the object remains in a state as if this
     * function hadn't been called.
     *
     * `path`, which must be a path to an existing file, is used to retrieve a file
     * handle (which is closed when the object destructs or `unmap` is called), which is
     * then used to memory map the requested region. Upon failure, `error` is set to
     * indicate the reason and the object remains in an unmapped state.
     *
     * `offset` is the number of bytes, relative to the start of the file, where the
     * mapping should begin. When specifying it, there is no need to worry about
     * providing a value that is aligned with the operating system's page allocation
     * granularity. This is adjusted by the implementation such that the first requested
     * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
     * `offset` from the start of the file.
     *
     * `length` is the number of bytes to map. It may be `map_entire_file`, in which
     * case a mapping of the entire file is created.
     */
    template<typename String>
    void map(const String& path, const size_type offset,
        const size_type length, std::error_code& error)
    {
        map_impl(path, offset, length, error);
    }

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
     * reason is reported via `error` and the object remains in a state as if this
     * function hadn't been called.
     *
     * `path`, which must be a path to an existing file, is used to retrieve a file
     * handle (which is closed when the object destructs or `unmap` is called), which is
     * then used to memory map the requested region. Upon failure, `error` is set to
     * indicate the reason and the object remains in an unmapped state.
     *
     * The entire file is mapped.
     */
    template<typename String>
    void map(const String& path, std::error_code& error)
    {
        map_impl(path, 0, map_entire_file, error);
    }

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
     * reason is reported via `error` and the object remains in a state as if this
     * function hadn't been called.
     *
     * `handle`, which must be a valid file handle, which is used to memory map the
     * requested region. Upon failure, `error` is set to indicate the reason and the
     * object remains in an unmapped state.
     *
     * `offset` is the number of bytes, relative to the start of the file, where the
     * mapping should begin. When specifying it, there is no need to worry about
     * providing a value that is aligned with the operating system's page allocation
     * granularity. This is adjusted by the implementation such that the first requested
     * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
     * `offset` from the start of the file.
     *
     * `length` is the number of bytes to map. It may be `map_entire_file`, in which
     * case a mapping of the entire file is created.
     */
    void map(const handle_type handle, const size_type offset,
        const size_type length, std::error_code& error)
    {
        map_impl(handle, offset, length, error);
    }

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
     * reason is reported via `error` and the object remains in a state as if this
     * function hadn't been called.
     *
     * `handle`, which must be a valid file handle, which is used to memory map the
     * requested region. Upon failure, `error` is set to indicate the reason and the
     * object remains in an unmapped state.
     *
     * The entire file is mapped.
     */
    void map(const handle_type handle, std::error_code& error)
    {
        map_impl(handle, 0, map_entire_file, error);
    }

    /**
     * If a valid memory mapping has been created prior to this call, this call
     * instructs the kernel to unmap the memory region and disassociate this object
     * from the file.
     *
     * The file handle associated with the file that is mapped is only closed if the
     * mapping was created using a file path. If, on the other hand, an existing
     * file handle was used to create the mapping, the file handle is not closed.
     */
    void unmap() { if(pimpl_) pimpl_->unmap(); }

    void swap(basic_shared_mmap& other) { pimpl_.swap(other.pimpl_); }

    /** Flushes the memory mapped page to disk. Errors are reported via `error`. */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > void sync(std::error_code& error) { if(pimpl_) pimpl_->sync(error); }

    /** All operators compare the underlying `basic_mmap`'s addresses. */

    friend bool operator==(const basic_shared_mmap& a, const basic_shared_mmap& b)
    {
        return a.pimpl_ == b.pimpl_;
    }

    friend bool operator!=(const basic_shared_mmap& a, const basic_shared_mmap& b)
    {
        return !(a == b);
    }

    friend bool operator<(const basic_shared_mmap& a, const basic_shared_mmap& b)
    {
        return a.pimpl_ < b.pimpl_;
    }

    friend bool operator<=(const basic_shared_mmap& a, const basic_shared_mmap& b)
    {
        return a.pimpl_ <= b.pimpl_;
    }

    friend bool operator>(const basic_shared_mmap& a, const basic_shared_mmap& b)
    {
        return a.pimpl_ > b.pimpl_;
    }

    friend bool operator>=(const basic_shared_mmap& a, const basic_shared_mmap& b)
    {
        return a.pimpl_ >= b.pimpl_;
    }

private:
    template<typename MappingToken>
    void map_impl(const MappingToken& token, const size_type offset,
        const size_type length, std::error_code& error)
    {
        if(!pimpl_)
        {
            mmap_type mmap = make_mmap<mmap_type>(token, offset, length, error);
            if(error) { return; }
            pimpl_ = std::make_shared<mmap_type>(std::move(mmap));
        }
        else
        {
            pimpl_->map(token, offset, length, error);
        }
    }
};

/**
 * This is the basis for all read-only mmap objects and should be preferred over
 * directly using basic_shared_mmap.
 */
template<typename ByteT>
using basic_shared_mmap_source = basic_shared_mmap<access_mode::read, ByteT>;

/**
 * This is the basis for all read-write mmap objects and should be preferred over
 * directly using basic_shared_mmap.
 */
template<typename ByteT>
using basic_shared_mmap_sink = basic_shared_mmap<access_mode::write, ByteT>;

/**
 * These aliases cover the most common use cases, both representing a raw byte stream
 * (either with a char or an unsigned char/uint8_t).
 */
using shared_mmap_source = basic_shared_mmap_source<char>;
using shared_ummap_source = basic_shared_mmap_source<unsigned char>;

using shared_mmap_sink = basic_shared_mmap_sink<char>;
using shared_ummap_sink = basic_shared_mmap_sink<unsigned char>;

} // namespace mio

#endif // MIO_SHARED_MMAP_HEADER

#endif
/** @file
 *  @brief SIMD-accelerated skip for runs of non-special CSV bytes.
 *
 *  Conservative design: any byte that could be the delimiter, quote character,
 *  \n, or \r causes an early return.
 *
 *  Uses 4x cmpeq rather than a lookup-table shuffle because CSV has only four
 *  sentinel characters. vpshufb (shuffle) truncates index bytes to their low
 *  nibble, causing aliasing across 16-byte boundaries and silently skipping
 *  real delimiters. The cmpeq approach is alias-free and equally fast for
 *  small sentinel sets.
 *
 *  UTF-8 safe: all CSV structural bytes are single-byte ASCII; multi-byte
 *  sequences (values > 0x7F) are never misidentified as special.
 */
#include <array>
#include <cstdint>


#if !defined(CSV_NO_SIMD) && (defined(__AVX2__) || (defined(_MSC_VER) && defined(_M_AVX) && _M_AVX >= 2))
#define CSV_SIMD_AVX2 1
#elif !defined(CSV_NO_SIMD) && (defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))))
#define CSV_SIMD_SSE2 1
#elif !defined(CSV_NO_SIMD) && (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__) || defined(_M_ARM64))
#define CSV_SIMD_NEON 1
#endif

#if defined(CSV_SIMD_AVX2) || defined(CSV_SIMD_SSE2)
#include <immintrin.h>
// _tzcnt_u32 in GCC/Clang headers is __attribute__(__target__("bmi")), which
// requires -mbmi at the call site. __builtin_ctz has no such restriction and
// emits BSF/TZCNT as the optimizer sees fit. MSVC's _tzcnt_u32 has no
// equivalent restriction, so keep it there.
#  ifdef _MSC_VER
#    define CSV_TZCNT32(x) _tzcnt_u32(x)
#  else
#    define CSV_TZCNT32(x) static_cast<unsigned>(__builtin_ctz(x))
#  endif
#endif

#if defined(CSV_SIMD_NEON)
#  if defined(_MSC_VER)
#    include <arm64_neon.h>
#  else
#    include <arm_neon.h>
#  endif
#endif

namespace csv {
    namespace internals {
        // Precomputed byte vectors for the four CSV sentinel bytes.
        // Constructed once per parser instance and passed by const-ref into
        // find_next_non_special, amortizing fill cost across every field scan.
        //
        // Keep the layout independent of the consuming target's ISA macros.
        // CSVReader constructors are header-defined while parser methods live
        // in csv.lib, so a consumer TU and the library TU must agree on this
        // member layout even if only the library target was compiled with AVX2.
        //
        // Store byte arrays instead of __m256i/__m128i members so parser objects
        // do not carry over-aligned SIMD members on MSVC. The scan function uses
        // unaligned SIMD loads from these arrays.
        //
        // When no_quote mode is active, set quote_char = delimiter so that
        // quote bytes are not mistakenly treated as sentinels (they are
        // NOT_SPECIAL in that mode and must not cause SIMD to stop early).
        struct SentinelVecs {
            SentinelVecs() noexcept : SentinelVecs(',', '"') {}

            SentinelVecs(char delimiter, char quote_char) noexcept {
                v_delim.fill(delimiter);
                v_quote.fill(quote_char);
                v_lf.fill('\n');
                v_cr.fill('\r');
            }

            std::array<char, 32> v_delim, v_quote, v_lf, v_cr;
        };

        static_assert(sizeof(SentinelVecs) == 128, "SentinelVecs layout must stay ISA-independent.");
        static_assert(alignof(SentinelVecs) <= alignof(void*), "SentinelVecs must not require over-aligned allocation.");

        // Free function — easy to unit test independently of CSVParserCore.
        //
        // SIMD-only fast-forward: skips pos forward past any bytes that are
        // definitely not one of the four CSV sentinel characters. Stops as
        // soon as a sentinel byte is found OR fewer bytes remain than one
        // SIMD lane. The caller is responsible for the scalar tail loop.
        //
        // State-agnostic by design: stops conservatively at any sentinel byte
        // regardless of quote_escape. Inside a quoted field, delimiter and
        // newline bytes are NOT_SPECIAL under compound_parse_flag, so the
        // outer DFA loop re-enters parse_field immediately at zero cost.
        inline size_t find_next_non_special(
            csv::string_view data,
            size_t pos,
            const SentinelVecs& sentinels
        ) noexcept
        {
#if defined(CSV_SIMD_AVX2)
            const __m256i v_delim = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(sentinels.v_delim.data()));
            const __m256i v_quote = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(sentinels.v_quote.data()));
            const __m256i v_lf    = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(sentinels.v_lf.data()));
            const __m256i v_cr    = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(sentinels.v_cr.data()));

            while (pos + 32 <= data.size()) {
                __m256i bytes   = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data.data() + pos));
                __m256i special = _mm256_cmpeq_epi8(bytes, v_delim);
                special         = _mm256_or_si256(special, _mm256_cmpeq_epi8(bytes, v_quote));
                special         = _mm256_or_si256(special, _mm256_cmpeq_epi8(bytes, v_lf));
                special         = _mm256_or_si256(special, _mm256_cmpeq_epi8(bytes, v_cr));
                int mask        = _mm256_movemask_epi8(special);

                if (mask != 0)
                    return pos + CSV_TZCNT32(static_cast<unsigned>(mask));
                pos += 32;
            }
#elif defined(CSV_SIMD_SSE2)
            const __m128i v_delim = _mm_loadu_si128(reinterpret_cast<const __m128i*>(sentinels.v_delim.data()));
            const __m128i v_quote = _mm_loadu_si128(reinterpret_cast<const __m128i*>(sentinels.v_quote.data()));
            const __m128i v_lf    = _mm_loadu_si128(reinterpret_cast<const __m128i*>(sentinels.v_lf.data()));
            const __m128i v_cr    = _mm_loadu_si128(reinterpret_cast<const __m128i*>(sentinels.v_cr.data()));

            while (pos + 16 <= data.size()) {
                __m128i bytes   = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data.data() + pos));
                __m128i special = _mm_cmpeq_epi8(bytes, v_delim);
                special         = _mm_or_si128(special, _mm_cmpeq_epi8(bytes, v_quote));
                special         = _mm_or_si128(special, _mm_cmpeq_epi8(bytes, v_lf));
                special         = _mm_or_si128(special, _mm_cmpeq_epi8(bytes, v_cr));
                int mask        = _mm_movemask_epi8(special);

                if (mask != 0)
                    return pos + CSV_TZCNT32(static_cast<unsigned>(mask));
                pos += 16;
            }
#elif defined(CSV_SIMD_NEON)
            const uint8x16_t v_delim = vld1q_u8(reinterpret_cast<const uint8_t*>(sentinels.v_delim.data()));
            const uint8x16_t v_quote = vld1q_u8(reinterpret_cast<const uint8_t*>(sentinels.v_quote.data()));
            const uint8x16_t v_lf    = vld1q_u8(reinterpret_cast<const uint8_t*>(sentinels.v_lf.data()));
            const uint8x16_t v_cr    = vld1q_u8(reinterpret_cast<const uint8_t*>(sentinels.v_cr.data()));

            while (pos + 16 <= data.size()) {
                const uint8x16_t bytes = vld1q_u8(reinterpret_cast<const uint8_t*>(data.data() + pos));
                uint8x16_t special     = vceqq_u8(bytes, v_delim);
                special                = vorrq_u8(special, vceqq_u8(bytes, v_quote));
                special                = vorrq_u8(special, vceqq_u8(bytes, v_lf));
                special                = vorrq_u8(special, vceqq_u8(bytes, v_cr));

#if defined(__aarch64__) || defined(_M_ARM64)
                if (vmaxvq_u8(special) == 0) {
                    pos += 16;
                    continue;
                }
#endif

                uint8_t lanes[16];
                vst1q_u8(lanes, special);
                for (size_t i = 0; i < 16; ++i) {
                    if (lanes[i] != 0)
                        return pos + i;
                }
                pos += 16;
            }
#else
            (void)data; (void)sentinels;
#endif
            return pos;
        }
    }
}

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <string>
#include <vector>


namespace csv {
    namespace internals {
        struct ColNames;
        using ColNamesPtr = std::shared_ptr<ColNames>;
        using ConstColNamesPtr = std::shared_ptr<const ColNames>;

        /** @struct ColNames
             *  A data structure for handling column name information.
             *
             *  These are created by CSVReader and passed (via smart pointer)
             *  to CSVRow objects it creates, thus
             *  allowing for indexing by column name.
             */
        struct ColNames {
        public:
            ColNames() = default;
            ColNames(const std::vector<std::string>& names) {
                set_col_names(names);
            }

            const std::vector<std::string>& get_col_names() const noexcept;
            void set_col_names(const std::vector<std::string>&);
            int index_of(csv::string_view) const;

            /** Sets the column name lookup policy.
             *  Must be called before set_col_names() for CI policy to take effect.
             */
            void set_policy(csv::ColumnNamePolicy policy);
            csv::ColumnNamePolicy get_policy() const noexcept;

            bool empty() const noexcept { return this->col_names.empty(); }
            size_t size() const noexcept;

            /** Retrieve column name by index. Throws if index is out of bounds. */
            const std::string& operator[](size_t i) const;

        private:
            std::vector<std::string> col_names;
            std::unordered_map<std::string, size_t> col_pos;
            csv::ColumnNamePolicy _policy = csv::ColumnNamePolicy::EXACT;
        };
    }
}

/** @file
 *  @brief Focused CSV byte parser core.
 */


#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

/** @file
 *  Defines the data type used for storing information about a CSV row
 */

#include <chrono>
#include <cstdint>
#include <cmath>
#include <iterator>
#include <memory> // For CSVField
#include <limits> // For CSVField
#include <unordered_set>
#include <string>
#include <sstream>
#include <vector>

#ifdef CSV_HAS_CXX17
#include <optional>
#endif
#ifdef CSV_HAS_CXX23
#if defined(__has_include)
#if __has_include(<expected>)
#include <expected>
#ifdef __cpp_lib_expected
#define CSV_HAS_STD_EXPECTED
#endif
#endif
#endif
#endif
#ifdef CSV_HAS_CXX20
#include <ranges>
#endif
/** @file
 *  @brief Internal JSON serialization helpers for row-like CSV data.
 */


#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>


namespace csv {
    namespace internals {
        /** Escape sequences for ASCII control characters that must be encoded in JSON. */
        static const char* const json_control_escape_sequences[32] = {
            "\\u0000", "\\u0001", "\\u0002", "\\u0003",
            "\\u0004", "\\u0005", "\\u0006", "\\u0007",
            "\\b",     "\\t",     "\\n",     "\\u000b",
            "\\f",     "\\r",     "\\u000e", "\\u000f",
            "\\u0010", "\\u0011", "\\u0012", "\\u0013",
            "\\u0014", "\\u0015", "\\u0016", "\\u0017",
            "\\u0018", "\\u0019", "\\u001a", "\\u001b",
            "\\u001c", "\\u001d", "\\u001e", "\\u001f"
        };

        static const char* const json_quote_escape_sequence = "\\\"";
        static const char* const json_backslash_escape_sequence = "\\\\";

        CSV_CONST inline const char* json_escape_sequence(unsigned char c) noexcept {
            if (c < 0x20) {
                return json_control_escape_sequences[c];
            }

            if (c == '"') {
                return json_quote_escape_sequence;
            }

            if (c == '\\') {
                return json_backslash_escape_sequence;
            }

            return nullptr;
        }

        inline std::size_t json_extra_space(csv::string_view s) noexcept {
            std::size_t result = 0;

            for (csv::string_view::size_type i = 0; i < s.size(); ++i) {
                const unsigned char c = static_cast<unsigned char>(s[i]);
                const char* escape_sequence = json_escape_sequence(c);
                if (escape_sequence) {
                    result += std::strlen(escape_sequence) - 1;
                }
            }

            return result;
        }

        inline void append_json_escaped(std::string& out, csv::string_view s) noexcept {
            const std::size_t extra = json_extra_space(s);
            if (extra == 0) {
                out.append(s.data(), s.size());
                return;
            }

            const std::size_t original_size = out.size();
            out.resize(original_size + s.size() + extra);
            char* dest = &out[original_size];
            std::size_t pos = 0;

            for (csv::string_view::size_type i = 0; i < s.size(); ++i) {
                const unsigned char c = static_cast<unsigned char>(s[i]);
                const char* escape_sequence = json_escape_sequence(c);
                if (escape_sequence) {
                    const std::size_t escape_length = std::strlen(escape_sequence);
                    std::memcpy(dest + pos, escape_sequence, escape_length);
                    pos += escape_length;
                } else {
                    dest[pos++] = static_cast<char>(c);
                }
            }
        }

        inline std::string json_escape_string(csv::string_view s) noexcept {
            std::string out;
            out.reserve(s.size() + json_extra_space(s));
            append_json_escaped(out, s);
            return out;
        }

        class JsonConverter {
        public:
            JsonConverter() = default;
            explicit JsonConverter(const std::vector<std::string>& column_names)
                : column_names_(column_names) {
                escaped_object_keys_.reserve(column_names_.size());
                column_positions_.reserve(column_names_.size());

                for (size_t i = 0; i < column_names_.size(); ++i) {
                    escaped_object_keys_.push_back('"' + json_escape_string(column_names_[i]) + "\":");
                    column_positions_[column_names_[i]] = i;
                }
            }

            template<typename FieldAt>
            std::string row_to_json(size_t field_count, FieldAt field_at, const std::vector<std::string>& subset = {}) const {
                return subset.empty()
                    ? this->row_to_json_all(field_count, field_at)
                    : this->row_to_json_subset(field_count, field_at, subset);
            }

            template<typename FieldAt>
            std::string row_to_json_array(size_t field_count, FieldAt field_at, const std::vector<std::string>& subset = {}) const {
                return subset.empty()
                    ? this->row_to_json_array_all(field_count, field_at)
                    : this->row_to_json_array_subset(field_count, field_at, subset);
            }

        private:
            template<typename FieldAt>
            std::string row_to_json_all(size_t field_count, FieldAt field_at) const {
                const size_t count = (field_count < escaped_object_keys_.size()) ? field_count : escaped_object_keys_.size();
                std::string out = "{";

                for (size_t i = 0; i < count; ++i) {
                    out += escaped_object_keys_[i];
                    this->append_json_value(out, field_at(i));
                    if (i + 1 < count) {
                        out += ',';
                    }
                }

                out += '}';
                return out;
            }

            template<typename FieldAt>
            std::string row_to_json_subset(
                size_t field_count,
                FieldAt field_at,
                const std::vector<std::string>& subset
            ) const {
                std::string out = "{";
                bool first = true;

                for (const auto& column : subset) {
                    const size_t index = this->index_of(column);
                    if (index >= field_count) {
                        continue;
                    }

                    if (!first) {
                        out += ',';
                    }

                    out += escaped_object_keys_[index];
                    this->append_json_value(out, field_at(index));
                    first = false;
                }

                out += '}';
                return out;
            }

            template<typename FieldAt>
            std::string row_to_json_array_all(size_t field_count, FieldAt field_at) const {
                const size_t count = (field_count < column_names_.size()) ? field_count : column_names_.size();
                std::string out = "[";

                for (size_t i = 0; i < count; ++i) {
                    this->append_json_value(out, field_at(i));
                    if (i + 1 < count) {
                        out += ',';
                    }
                }

                out += ']';
                return out;
            }

            template<typename FieldAt>
            std::string row_to_json_array_subset(
                size_t field_count,
                FieldAt field_at,
                const std::vector<std::string>& subset
            ) const {
                std::string out = "[";
                bool first = true;

                for (const auto& column : subset) {
                    const size_t index = this->index_of(column);
                    if (index >= field_count) {
                        continue;
                    }

                    if (!first) {
                        out += ',';
                    }

                    this->append_json_value(out, field_at(index));
                    first = false;
                }

                out += ']';
                return out;
            }

            void append_json_value(std::string& out, csv::string_view value) const {
                const DataType type = internals::data_type(value);
                if (type >= DataType::CSV_INT8 && type <= DataType::CSV_DOUBLE) {
                    out.append(value.data(), value.size());
                } else {
                    out += '"';
                    append_json_escaped(out, value);
                    out += '"';
                }
            }

            size_t index_of(const std::string& column) const {
                const auto it = column_positions_.find(column);
                if (it == column_positions_.end()) {
                    throw_column_not_found(column);
                }

                return it->second;
            }

            std::vector<std::string> column_names_;
            std::vector<std::string> escaped_object_keys_;
            std::unordered_map<std::string, size_t> column_positions_;
        };
    }
}

/** @file
 *  @brief Internal data structures for CSV parsing
 * 
 *  This file contains the low-level structures used by the parser to store
 *  CSV data before it's exposed through the public CSVRow/CSVField API.
 * 
 *  Data flow: Parser -> RawCSVData -> CSVRow -> CSVField
 */


#include <memory>

/** @file
 *  @brief Stable storage for eager CSV field scalar classifications
 */


#include <algorithm>
#include <atomic>
#include <cassert>
#include <memory>
#include <utility>
#include <vector>


namespace csv {
    namespace internals {
        namespace memory {
            /** Stable sidecar storage for parser-time CSVFieldScalar values.
             *
             *  This intentionally mirrors RawCSVFieldList allocation semantics:
             *  scalar values are stored in fixed-size blocks whose addresses stay
             *  stable while the parser appends later fields from the same chunk.
             *  Cross-thread visibility is provided by the row queue mutex plus
             *  atomic size/block publication, matching RawCSVFieldList.
             */
            class CSVFieldScalarList {
            public:
                CSVFieldScalarList(size_t single_buffer_capacity = (size_t)(internals::PAGE_SIZE / sizeof(CSVFieldScalar))) :
                    block_capacity_(single_buffer_capacity == 0 ? 1 : single_buffer_capacity) {
                    const size_t max_fields = internals::CSV_CHUNK_SIZE_DEFAULT + 1;
                    const size_t block_count = (max_fields + this->block_capacity_ - 1) / this->block_capacity_;
                    this->blocks_.reserve(block_count);
                    this->blocks_.resize(block_count);
                }

                CSVFieldScalarList(const CSVFieldScalarList&) = delete;

                CSVFieldScalarList(CSVFieldScalarList&& other) noexcept
                    : block_capacity_(other.block_capacity_),
                      blocks_(std::move(other.blocks_)) {
                    this->size_.store(other.size_.load(std::memory_order_acquire), std::memory_order_release);
                    this->block_count_.store(other.block_count_.load(std::memory_order_acquire), std::memory_order_release);
                    other.size_.store(0, std::memory_order_release);
                    other.block_count_.store(0, std::memory_order_release);
                }

                void emplace_back(const CSVFieldScalar& scalar) {
                    const size_t offset = this->size_.load(std::memory_order_acquire);
                    const size_t page_no = offset / this->block_capacity_;
                    const size_t buffer_idx = offset % this->block_capacity_;

                    if (page_no >= this->block_count_.load(std::memory_order_acquire)) {
                        this->allocate_block(page_no);
                    }

                    this->blocks_[page_no].get()[buffer_idx] = scalar;
                    this->size_.store(offset + 1, std::memory_order_release);
                }

                size_t size() const noexcept {
                    return this->size_.load(std::memory_order_acquire);
                }

                bool empty() const noexcept {
                    return this->size() == 0;
                }

                void reserve_for_source_size(size_t source_size) {
                    const size_t max_fields = source_size + 1;
                    const size_t block_count = (max_fields + this->block_capacity_ - 1) / this->block_capacity_;
                    if (block_count > this->blocks_.size()) {
                        this->blocks_.resize(block_count);
                    }
                }

                inline const CSVFieldScalar& operator[](size_t n) const {
                    assert(n < this->size_.load(std::memory_order_acquire));
                    const size_t page_no = n / this->block_capacity_;
                    const size_t buffer_idx = n % this->block_capacity_;
                    assert(page_no < this->block_count_.load(std::memory_order_acquire));
                    return this->blocks_[page_no].get()[buffer_idx];
                }

            private:
                size_t block_capacity_;
                std::vector<std::unique_ptr<CSVFieldScalar[]>> blocks_;
                std::atomic<size_t> size_{ 0 };
                std::atomic<size_t> block_count_{ 0 };

                void allocate_block(size_t page_no) {
                    if (page_no >= this->blocks_.size()) {
                        this->blocks_.resize((std::max)(page_no + 1, this->blocks_.size() * 2));
                    }

                    if (!this->blocks_[page_no]) {
                        this->blocks_[page_no] = std::unique_ptr<CSVFieldScalar[]>(new CSVFieldScalar[this->block_capacity_]);
                    }

                    this->block_count_.store(page_no + 1, std::memory_order_release);
                }
            };
        }
    }
}

/** @file
 *  @brief Stable sidecar storage for parser-realized quoted field bytes
 */


#include <algorithm>
#include <cassert>

/** @file
 *  @brief Stable append-only arenas used by parser backing storage
 */


#include <algorithm>
#include <atomic>
#include <cassert>
#include <memory>
#include <utility>
#include <vector>


namespace csv {
    namespace internals {
        namespace memory {
            /** Variable-size block arena used for quote realization bytes.
             *
             *  Quote fields can be larger than the default page and blocks may grow,
             *  so this arena carries per-block metadata and resolves logical offsets
             *  by binary search.
             */
            template<typename T>
            class RawCSVBlockArena {
            public:
                struct Allocation {
                    Allocation() : offset(0), data(nullptr) {}

                    Allocation(CSVChunkIndex offset_, T* data_) : offset(offset_), data(data_) {}

                    CSVChunkIndex offset = 0;
                    T* data = nullptr;
                };

                explicit RawCSVBlockArena(size_t default_block_capacity, bool grow_blocks = true)
                    : default_block_capacity_(default_block_capacity == 0 ? 1 : default_block_capacity),
                      grow_blocks_(grow_blocks) {
                    this->blocks_.reserve(1);
                }

                RawCSVBlockArena(const RawCSVBlockArena&) = delete;
                RawCSVBlockArena& operator=(const RawCSVBlockArena&) = delete;

                RawCSVBlockArena(RawCSVBlockArena&& other) noexcept
                    : default_block_capacity_(other.default_block_capacity_),
                      grow_blocks_(other.grow_blocks_),
                      next_block_capacity_(other.next_block_capacity_),
                      blocks_(std::move(other.blocks_)) {
                    this->size_.store(other.size_.load(std::memory_order_acquire), std::memory_order_release);
                    this->block_count_.store(other.block_count_.load(std::memory_order_acquire), std::memory_order_release);
                    other.size_.store(0, std::memory_order_release);
                    other.block_count_.store(0, std::memory_order_release);
                }

                RawCSVBlockArena& operator=(RawCSVBlockArena&& other) noexcept {
                    if (this == &other) {
                        return *this;
                    }

                    this->default_block_capacity_ = other.default_block_capacity_;
                    this->grow_blocks_ = other.grow_blocks_;
                    this->next_block_capacity_ = other.next_block_capacity_;
                    this->blocks_ = std::move(other.blocks_);
                    this->size_.store(other.size_.load(std::memory_order_acquire), std::memory_order_release);
                    this->block_count_.store(other.block_count_.load(std::memory_order_acquire), std::memory_order_release);
                    other.size_.store(0, std::memory_order_release);
                    other.block_count_.store(0, std::memory_order_release);
                    return *this;
                }

                template<class... Args>
                T& emplace_back(Args&&... args) {
                    Allocation allocation = this->allocate_contiguous(1);
                    *allocation.data = T(std::forward<Args>(args)...);
                    return *allocation.data;
                }

                Allocation allocate_contiguous(size_t count) {
                    if (count == 0) {
                        return Allocation{ this->checked_offset(this->size_.load(std::memory_order_acquire)), nullptr };
                    }

                    if (this->block_count_.load(std::memory_order_acquire) == 0
                        || this->current_block().used.load(std::memory_order_acquire) + count > this->current_block().capacity) {
                        this->allocate_block(count);
                    }

                    Block& block = this->current_block();
                    const size_t block_used = block.used.load(std::memory_order_acquire);
                    Allocation allocation{
                        this->checked_offset(block.logical_start + block_used),
                        block.values.get() + block_used
                    };
                    block.used.store(block_used + count, std::memory_order_release);
                    this->size_.fetch_add(count, std::memory_order_release);
                    return allocation;
                }

                T& operator[](size_t n) const {
                    assert(n < this->size_.load(std::memory_order_acquire));
                    const Block& block = this->find_block(n);
                    return block.values.get()[n - block.logical_start];
                }

                size_t size() const noexcept {
                    return this->size_.load(std::memory_order_acquire);
                }

                csv::string_view view(size_t offset, size_t length) const {
                    if (length == 0) {
                        return csv::string_view();
                    }

                    const Block& block = this->find_block(offset);
                    assert(offset + length <= block.logical_start + block.used.load(std::memory_order_acquire));
                    const size_t block_offset = offset - block.logical_start;
                    return csv::string_view(block.values.get() + block_offset, length);
                }

                void reserve_blocks(size_t count) {
                    if (count > this->blocks_.size()) {
                        this->blocks_.resize(count);
                    }
                }

            private:
                struct Block {
                    std::unique_ptr<T[]> values;
                    size_t capacity = 0;
                    std::atomic<size_t> used{ 0 };
                    size_t logical_start = 0;
                };

                size_t default_block_capacity_;
                bool grow_blocks_;
                size_t next_block_capacity_ = 0;
                std::vector<std::unique_ptr<Block>> blocks_;
                std::atomic<size_t> size_{ 0 };
                std::atomic<size_t> block_count_{ 0 };

                CSVChunkIndex checked_offset(size_t value) const noexcept {
                    assert(value <= CSV_CHUNK_INDEX_MAX);
                    return static_cast<CSVChunkIndex>(value);
                }

                void allocate_block(size_t required_capacity) {
                    const size_t block_count = this->block_count_.load(std::memory_order_acquire);
                    if (block_count >= this->blocks_.size()) {
                        this->blocks_.resize((std::max)(size_t(1), this->blocks_.size() * 2));
                    }

                    const size_t baseline = this->next_block_capacity_ == 0
                        ? this->default_block_capacity_
                        : this->next_block_capacity_;
                    const size_t capacity = (std::max)(baseline, required_capacity);

                    if (!this->blocks_[block_count]) {
                        this->blocks_[block_count] = std::unique_ptr<Block>(new Block());
                    }

                    Block& block = *this->blocks_[block_count];
                    block.values = std::unique_ptr<T[]>(new T[capacity]);
                    block.capacity = capacity;
                    block.used.store(0, std::memory_order_release);
                    block.logical_start = this->size_.load(std::memory_order_acquire);

                    if (required_capacity > baseline) {
                        this->next_block_capacity_ = this->default_block_capacity_;
                    }
                    else if (this->grow_blocks_ && baseline < this->default_block_capacity_ * 4) {
                        this->next_block_capacity_ = baseline * 2;
                    }
                    else {
                        this->next_block_capacity_ = baseline;
                    }

                    this->block_count_.store(block_count + 1, std::memory_order_release);
                }

                Block& current_block() {
                    const size_t block_count = this->block_count_.load(std::memory_order_acquire);
                    return *this->blocks_[block_count - 1];
                }

                const Block& find_block(size_t offset) const {
                    size_t low = 0;
                    size_t high = this->block_count_.load(std::memory_order_acquire);
                    while (low < high) {
                        const size_t mid = low + (high - low) / 2;
                        const Block& block = *this->blocks_[mid];
                        if (offset < block.logical_start) {
                            high = mid;
                        }
                        else if (offset >= block.logical_start + block.used.load(std::memory_order_acquire)) {
                            low = mid + 1;
                        }
                        else {
                            return block;
                        }
                    }

                    assert(false && "RawCSVBlockArena offset out of range");
                    return *this->blocks_[this->block_count_.load(std::memory_order_acquire) - 1];
                }
            };
        }
    }
}


namespace csv {
    namespace internals {
        namespace memory {
            class RawCSVQuoteArena {
            public:
                RawCSVQuoteArena() : arena_(internals::PAGE_SIZE) {}

                CSVChunkIndex append(csv::string_view bytes) {
                    if (bytes.empty()) {
                        return this->checked_offset(this->arena_.size());
                    }

                    auto allocation = this->arena_.allocate_contiguous(bytes.size());
                    std::copy(bytes.begin(), bytes.end(), allocation.data);
                    return allocation.offset;
                }

                RawCSVBlockArena<char>::Allocation allocate_contiguous(size_t length) {
                    return this->arena_.allocate_contiguous(length);
                }

                csv::string_view view(size_t start, size_t length) const {
                    return this->arena_.view(start, length);
                }

                void reserve_for_source_size(size_t source_size) {
                    const size_t block_capacity = (source_size + internals::PAGE_SIZE - 1) / internals::PAGE_SIZE;
                    this->arena_.reserve_blocks(block_capacity + 1);
                }

            private:
                RawCSVBlockArena<char> arena_;

                CSVChunkIndex checked_offset(size_t value) const noexcept {
                    assert(value <= CSV_CHUNK_INDEX_MAX);
                    return static_cast<CSVChunkIndex>(value);
                }
            };
        }
    }
}

/** @file
 *  @brief Compact parser metadata for a single CSV field
 */


#include <cassert>


namespace csv {
    namespace internals {
        namespace memory {
            /** A barebones class used for describing CSV fields */
            struct RawCSVField {
                RawCSVField() = default;
                RawCSVField(
                    size_t _start,
                    size_t _length,
                    bool _is_realized = false
                ) noexcept {
                    assert(_start <= CSV_CHUNK_INDEX_MAX);
                    assert(_length <= CSV_CHUNK_INDEX_MAX);
                    start = static_cast<CSVChunkIndex>(_start);
                    length = static_cast<CSVChunkIndex>(_length);
                    is_realized = _is_realized;
                }

                /** Raw row-relative start, or quote-arena logical start when is_realized is true. */
                CSVChunkIndex start = 0;

                /** Field length in the selected backing storage. */
                CSVChunkIndex length = 0;

                /** True when start/length refer to RawCSVData::quote_arena instead of RawCSVData::data. */
                bool is_realized = false;

                CONSTEXPR bool has_realized_storage() const noexcept {
                    return is_realized;
                }
            };
        }
    }
}

/** @file
 *  @brief Stable storage for RawCSVField metadata
 */


#include <algorithm>
#include <atomic>
#include <cassert>
#include <memory>
#include <utility>
#include <vector>


namespace csv {
    namespace internals {
        namespace memory {
            /** A class used for efficiently storing RawCSVField objects and expanding as necessary
             *
             *  @par Implementation
             *  Stores fields in fixed-size page-aligned chunks via a vector of
             *  unique_ptr<RawCSVField[]> handles:
             *   - This design provides better cache locality when accessing sequential fields in a row
             *     as well as much lower memory allocation overhead.
             *   - Fixed-size blocks allow direct division/modulo lookup; this intentionally
             *     avoids the variable-block metadata needed by quote realization storage.
             * 
             *  @par Thread Safety
             *  Cross-thread visibility for field contents is provided by the records
             *  queue mutex in ThreadSafeDeque. The fixed arena publishes size/block
             *  state with atomics so readers can safely resolve already-emitted rows
             *  while the parser appends later fields from the same RawCSVData.
             */
            class RawCSVFieldList {
            public:
                /** Construct a RawCSVFieldList which allocates blocks of a certain size */
                RawCSVFieldList(size_t single_buffer_capacity = (size_t)(internals::PAGE_SIZE / sizeof(RawCSVField))) :
                    block_capacity_(single_buffer_capacity == 0 ? 1 : single_buffer_capacity) {
                    const size_t max_fields = internals::CSV_CHUNK_SIZE_DEFAULT + 1;
                    const size_t block_count = (max_fields + this->block_capacity_ - 1) / this->block_capacity_;
                    this->blocks_.reserve(block_count);
                    this->blocks_.resize(block_count);
                }

                // No copy constructor
                RawCSVFieldList(const RawCSVFieldList& other) = delete;

                // RawCSVFieldList may be moved with its backing blocks intact.
                RawCSVFieldList(RawCSVFieldList&& other) noexcept
                    : block_capacity_(other.block_capacity_),
                      blocks_(std::move(other.blocks_)) {
                    this->size_.store(other.size_.load(std::memory_order_acquire), std::memory_order_release);
                    this->block_count_.store(other.block_count_.load(std::memory_order_acquire), std::memory_order_release);
                    other.size_.store(0, std::memory_order_release);
                    other.block_count_.store(0, std::memory_order_release);
                }

                template <class... Args>
                void emplace_back(Args&&... args) {
                    const size_t offset = this->size_.load(std::memory_order_acquire);
                    const size_t page_no = offset / this->block_capacity_;
                    const size_t buffer_idx = offset % this->block_capacity_;

                    if (page_no >= this->block_count_.load(std::memory_order_acquire)) {
                        this->allocate_block(page_no);
                    }

                    this->blocks_[page_no].get()[buffer_idx] = RawCSVField(std::forward<Args>(args)...);
                    this->size_.store(offset + 1, std::memory_order_release);
                }

                size_t size() const noexcept {
                    return this->size_.load(std::memory_order_acquire);
                }

                void reserve_for_source_size(size_t source_size) {
                    const size_t max_fields = source_size + 1;
                    const size_t block_count = (max_fields + this->block_capacity_ - 1) / this->block_capacity_;
                    if (block_count > this->blocks_.size()) {
                        this->blocks_.resize(block_count);
                    }
                }

                /** Access a field by its index. This allows CSVRow objects to access fields
                 *  without knowing internal implementation details of RawCSVFieldList.
                 */
                inline RawCSVField& operator[](size_t n) const {
                    assert(n < this->size_.load(std::memory_order_acquire));
                    const size_t page_no = n / this->block_capacity_;
                    const size_t buffer_idx = n % this->block_capacity_;
                    assert(page_no < this->block_count_.load(std::memory_order_acquire));
                    return this->blocks_[page_no].get()[buffer_idx];
                }

            private:
                size_t block_capacity_;
                std::vector<std::unique_ptr<RawCSVField[]>> blocks_;
                std::atomic<size_t> size_{ 0 };
                std::atomic<size_t> block_count_{ 0 };

                void allocate_block(size_t page_no) {
                    if (page_no >= this->blocks_.size()) {
                        this->blocks_.resize((std::max)(page_no + 1, this->blocks_.size() * 2));
                    }

                    if (!this->blocks_[page_no]) {
                        this->blocks_[page_no] = std::unique_ptr<RawCSVField[]>(new RawCSVField[this->block_capacity_]);
                    }

                    this->block_count_.store(page_no + 1, std::memory_order_release);
                }
            };
        }
    }
}


namespace csv {
    namespace internals {
        class JsonConverter;

        using memory::CSVFieldScalarList;
        using memory::RawCSVField;
        using memory::RawCSVFieldList;
        using memory::RawCSVQuoteArena;

        /** A class for storing raw CSV data and associated metadata
         * 
         *  This structure is the bridge between the parser thread and the main thread.
         *  Parser populates fields, data, and parse_flags; main thread reads via CSVRow.
         */
        struct RawCSVData {
            std::shared_ptr<void> _data = nullptr;
            csv::string_view data = "";

            /** Absolute byte offset where this parsed chunk starts in the source. */
            size_t source_start = 0;

            internals::RawCSVFieldList fields;

            /** Optional parser-time scalar sidecar; empty unless eager classification is enabled. */
            internals::CSVFieldScalarList field_scalars;

            /** Parser-time sidecar bytes for fields whose quoted contents contained doubled quotes. */
            internals::RawCSVQuoteArena quote_arena;

            /** Cached JSON converter for rows sharing this parsed backing storage. */
            mutable internals::lazy_shared_ptr<JsonConverter> json_converter;

            internals::ColNamesPtr col_names = nullptr;
            internals::ParseFlagMap parse_flags;
            internals::WhitespaceMap ws_flags;

            /** True when at least one whitespace trim character is configured.
             *  Used by get_field_impl() to skip trim work in the common no-trim case.
             */
            bool has_ws_trimming = false;

            bool has_field_scalars() const noexcept {
                return !this->field_scalars.empty();
            }
        };

        using RawCSVDataPtr = std::shared_ptr<RawCSVData>;
    }
}


namespace csv {
    namespace internals {
        template<typename RowSink, typename ParsePolicy, typename FieldPolicy, typename RowPolicy>
        class CSVParserCore;
        struct CSVRowRowPolicy;
        namespace parser {
            class CSVParserDriverBase;
        }
        namespace speculative {
            struct CSVRowFragment;
        }

        static const std::string ERROR_NAN = "Not a number.";
        static const std::string ERROR_OVERFLOW = "Overflow error.";
        static const std::string ERROR_FLOAT_TO_INT =
            "Attempted to convert a floating point value to an integral type.";
        static const std::string ERROR_NEG_TO_UNSIGNED = "Negative numbers cannot be converted to unsigned types.";
    
        // Inside CSVField::get() or wherever you materialize the value
        csv::string_view get_trimmed(csv::string_view sv, const WhitespaceMap& ws_flags) noexcept;
    }

    /**
    * @enum CSVConversionError
    * @brief Non-throwing CSVField conversion result.
    *
    * Returned by CSVField::as() inside std::expected, and used internally by
    * CSVField::get() and CSVField::try_get() to keep throwing and non-throwing
    * conversions on the same rules.
    *
    * @sa csv_conversion_error_message()
    */
    enum class CSVConversionError {
        /** Conversion succeeded. */
        None = 0,

        /** The field is not compatible with the requested target type. */
        NotANumber,

        /** The parsed value does not fit in the requested target type. */
        Overflow,

        /** A floating point field was requested as an integral type. */
        FloatToInt,

        /** A negative value was requested as an unsigned type. */
        NegativeToUnsigned
    };

    namespace internals {
        typedef const char* csv_error_message;

        static CONSTEXPR_VALUE_14 csv_error_message CSV_CONVERSION_ERROR_MESSAGES[] = {
            "",
            "Not a number.",
            "Overflow error.",
            "Attempted to convert a floating point value to an integral type.",
            "Negative numbers cannot be converted to unsigned types."
        };
    }

    /** Return a stable human-readable description for a CSVConversionError. */
    inline const char* csv_conversion_error_message(CSVConversionError error) noexcept {
        const size_t index = static_cast<size_t>(error);
        return index < (sizeof(internals::CSV_CONVERSION_ERROR_MESSAGES) / sizeof(internals::CSV_CONVERSION_ERROR_MESSAGES[0]))
            ? internals::CSV_CONVERSION_ERROR_MESSAGES[index]
            : internals::CSV_CONVERSION_ERROR_MESSAGES[static_cast<size_t>(CSVConversionError::NotANumber)];
    }

    /**
    * @class CSVField
    * @brief Data type representing individual CSV values.
    *        CSVFields can be obtained by using CSVRow::operator[]
    */
    class CSVField {
    public:
        /** Constructs a CSVField from a string_view */
        constexpr explicit CSVField(csv::string_view _sv) noexcept
            : sv(_sv.data() ? _sv : csv::string_view("")) {}

        CSVField(csv::string_view _sv, const internals::CSVFieldScalar& scalar) noexcept
            : sv(_sv.data() ? _sv : csv::string_view("")),
              type_(scalar.type) {
            switch (scalar.type) {
            case DataType::CSV_INT8:
            case DataType::CSV_INT16:
            case DataType::CSV_INT32:
            case DataType::CSV_INT64:
                this->value_.integer = scalar.integer;
                break;
            case DataType::CSV_DOUBLE:
            case DataType::CSV_BIGINT:
                this->value_.floating = scalar.floating;
                break;
            case DataType::CSV_TIMESTAMP:
                this->value_.timestamp = scalar.timestamp;
                break;
            case DataType::CSV_BOOL:
                this->value_.boolean = scalar.boolean;
                break;
            default:
                break;
            }
        }

        operator csv::string_view() const noexcept {
            return this->sv;
        }

        operator std::string() const {
            return std::string(this->sv);
        }

        /** Returns the value casted to the requested type, performing type checking before.
        *
        *  \par Valid options for T
        *   - std::string or csv::string_view
        *   - signed integral types (signed char, short, int, long int, long long int)
        *   - unsigned integral types (unsigned char, unsigned short, unsigned int, unsigned long long)
        *   - floating point types (float, double, long double)
        *
        *  \par Invalid conversions
        *   - Converting non-numeric values to any numeric type
        *   - Converting floating point values to integers
        *   - Converting a large integer to a smaller type that will not hold it
        *
        *  @note    This method is capable of parsing scientific E-notation.
        *           See @ref scalar_conversions for more details.
        *
        *  @throws  std::runtime_error Thrown if an invalid conversion is performed.
        *
        *  @warning Currently, conversions to floating point types are not
        *           checked for loss of precision
        *
        *  @warning Any string_views returned are only guaranteed to be valid
        *           if the parent CSVRow is still alive. If you are concerned
        *           about object lifetimes, then grab a std::string or a
        *           numeric value.
        *
        */
        template<typename T = std::string> T get() {
            T out{};
            const CSVConversionError err = check_convert(out);
            if (err != CSVConversionError::None) throw std::runtime_error(csv_conversion_error_message(err));
            return out;
        }

#ifdef CSV_HAS_STD_EXPECTED
        /**
         * Return this field as T, preserving conversion failure as CSVConversionError.
         *
         * @return std::expected containing T on success, or CSVConversionError
         *         describing why conversion failed.
         *
         * @note Requires C++23 and a standard library that provides std::expected.
         *
         * @sa CSVConversionError
         * @sa csv_conversion_error_message()
         */
        template<typename T = std::string>
        std::expected<T, CSVConversionError> as() {
            T out{};
            const CSVConversionError err = check_convert(out);
            return (err != CSVConversionError::None)
                ? std::expected<T, CSVConversionError>(std::unexpected(err))
                : std::expected<T, CSVConversionError>(out);
        }
#endif

        /** Non-throwing equivalent of get(). Applies the same type checks and conversions;
         *  returns true and writes to @p out on success, or returns false without throwing.
         *
         *  @sa get() for the full description of valid types, conversion rules, and warnings.
         *
         *  Example:
         *  @code
         *  int value;
         *  if (field.try_get(value)) {
         *      // Use value safely
         *  } else {
         *      // Handle conversion failure
         *  }
         *  @endcode
         */
        template<typename T = std::string>
        bool try_get(T& out) noexcept {
            return check_convert(out) == CSVConversionError::None;
        }

#ifdef CSV_HAS_CXX17
        /** @anchor CSVField_optional_conversion
         *  Convert this field to std::optional<T>, returning std::nullopt when conversion fails.
         *
         *  This is a value-returning wrapper around try_get(), useful for C++17
         *  callers that want non-throwing conversion without an output parameter.
         *
         *  @note Requires C++17 or later.
         */
        template<typename T>
        operator std::optional<T>() {
            T out{};
            return try_get(out) ? std::optional<T>(out) : std::nullopt;
        }
#endif

        /** Parse a hexadecimal value, returning false if the value is not hex.
         *  @tparam T An integral type (int, long, long long, etc.)
         */
        template<typename T = long long>
        bool try_parse_hex(T& parsedValue) {
            static_assert(std::is_integral<T>::value,
                "try_parse_hex only works with integral types (int, long, long long, etc.)");

            return classify_scalar::parse_hex(this->sv.data(), this->sv.data() + this->sv.size(), parsedValue);
        }

        /** Attempts to parse a decimal (or integer) value using the given symbol,
         *  returning `true` if the value is numeric.
         *
         *  @note This method also updates this field's type
         *
         */
        bool try_parse_decimal(long double& dVal, const char decimalSymbol = '.');

        /** Parse this field as Unix milliseconds.
         *
         *  Timestamp-classified values return their parsed epoch value. Integral
         *  values are treated as already being Unix milliseconds.
         */
        bool try_parse_timestamp(std::uint64_t& out) noexcept;

        /** Parse this field as Unix milliseconds in a 64-bit unsigned integer. */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
        template<typename T>
        bool try_parse_timestamp(T& out) noexcept;
#else
        template<typename T>
        internals::enable_if_t<
            std::is_integral<T>::value && std::is_unsigned<T>::value && !std::is_same<T, bool>::value
            && (sizeof(T) >= sizeof(std::uint64_t)),
            bool
        >
        try_parse_timestamp(T& out) noexcept {
            std::uint64_t milliseconds = 0;
            if (!this->try_parse_timestamp(milliseconds))
                return false;

            out = static_cast<T>(milliseconds);
            return true;
        }
#endif

        /** Parse this field as a timestamp duration since the Unix epoch. */
        template<typename Rep, typename Period>
        bool try_parse_timestamp(std::chrono::duration<Rep, Period>& out) noexcept {
            std::uint64_t milliseconds = 0;
            if (!this->try_parse_timestamp(milliseconds))
                return false;

            out = std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(
                std::chrono::milliseconds(milliseconds));
            return true;
        }

        /** Parse this field as a std::chrono::system_clock time point. */
        template<typename Duration>
        bool try_parse_timestamp(std::chrono::time_point<std::chrono::system_clock, Duration>& out) noexcept {
            std::uint64_t milliseconds = 0;
            if (!this->try_parse_timestamp(milliseconds))
                return false;

            out = std::chrono::time_point<std::chrono::system_clock, Duration>(
                std::chrono::duration_cast<Duration>(std::chrono::milliseconds(milliseconds)));
            return true;
        }

        /** Compares the contents of this field to a numeric value. If this
         *  field does not contain a numeric value, then all comparisons return
         *  false.
         *
         *  @note    Floating point values are considered equal if they are within
         *           `0.000001` of each other.
         *
         *  @warning Multiple numeric comparisons involving the same field can
         *           be done more efficiently by calling the CSVField::get<>() method.
         *
         *  @sa      csv::CSVField::operator==(const char * other)
         *  @sa      csv::CSVField::operator==(csv::string_view other)
         */
        template<typename T>
        inline bool operator==(T other) const noexcept
        {
            static_assert(std::is_arithmetic<T>::value,
                "T should be a numeric value.");

            const_cast<CSVField*>(this)->get_value();
            if (this->type_ < DataType::CSV_INT8 || this->type_ > DataType::CSV_DOUBLE || this->type_ == DataType::CSV_BIGINT) {
                return false;
            }

            return internals::is_equal(this->numeric_value_as_long_double(), static_cast<long double>(other), 0.000001L);
        }

        /** Return a string view over the field's contents */
        CONSTEXPR csv::string_view get_sv() const noexcept { return this->sv; }

        /** Returns true if field is an empty string or string of whitespace characters */
        inline bool is_null() noexcept { return type() == DataType::CSV_NULL; }

        /** Returns true if field is a non-numeric, non-empty string */
        inline bool is_str() noexcept { return type() == DataType::CSV_STRING; }

        /** Returns true if field is an integer or float */
        inline bool is_num() noexcept {
            return type() >= DataType::CSV_INT8 && type() <= DataType::CSV_DOUBLE;
        }

        /** Returns true if field is an integer */
        inline bool is_int() noexcept {
            return (type() >= DataType::CSV_INT8) && (type() <= DataType::CSV_INT64);
        }

        /** Returns true if field is a floating point value */
        inline bool is_float() noexcept { return type() == DataType::CSV_DOUBLE; }

        /** Returns true if field is a boolean value */
        inline bool is_bool() noexcept { return type() == DataType::CSV_BOOL; }

        /** Returns true if field is a timestamp value */
        inline bool is_timestamp() noexcept { return type() == DataType::CSV_TIMESTAMP; }

        /** Return the type of the underlying CSV data */
        inline DataType type() noexcept {
            this->get_value();
            return type_;
        }

    private:
        // GCC emits a psABI note for by-value APIs involving unions with long double.
        // This is a workaround to keep the logs clean without sacrificing the benefits of a union on other compilers.
        // Give only GCC users the struct tax so normal builds and strict CI logs stay quiet.
#if defined(__GNUC__) && !defined(__clang__)
        struct FieldValue {
            constexpr FieldValue() noexcept
                : integer(0), floating(0), timestamp(0), boolean(false) {}

            std::int64_t integer;
            long double floating;
            std::uint64_t timestamp;
            bool boolean;
        };
#else
        union FieldValue {
            constexpr FieldValue() noexcept : floating(0) {}

            std::int64_t integer;
            long double floating;
            std::uint64_t timestamp;
            bool boolean;
        };
#endif

        struct FieldValueOutput {
            FieldValue& value;

            template<classify_scalar::ScalarKind Kind>
            typename std::enable_if<
                Kind == classify_scalar::scalar_int8
                || Kind == classify_scalar::scalar_int16
                || Kind == classify_scalar::scalar_int32
                || Kind == classify_scalar::scalar_int64,
                void>::type set(std::int64_t parsed) const noexcept {
                value.integer = parsed;
            }

            template<classify_scalar::ScalarKind Kind>
            typename std::enable_if<Kind == classify_scalar::scalar_float, void>::type set(long double parsed) const noexcept {
                value.floating = parsed;
            }

            template<classify_scalar::ScalarKind Kind>
            typename std::enable_if<Kind == classify_scalar::scalar_bool, void>::type set(bool parsed) const noexcept {
                value.boolean = parsed;
            }

            template<classify_scalar::ScalarKind Kind>
            typename std::enable_if<Kind == classify_scalar::scalar_timestamp, void>::type set(std::uint64_t parsed) const noexcept {
                value.timestamp = parsed;
            }
        };

        FieldValue value_;        /**< Cached typed values. */
        csv::string_view sv = ""; /**< A pointer to this field's text */
        DataType type_ = DataType::UNKNOWN; /**< Cached data type value */

        CONSTEXPR_14 bool stores_integral() const noexcept {
            return type_ >= DataType::CSV_INT8 && type_ <= DataType::CSV_INT64;
        }

        CONSTEXPR_14 long double numeric_value_as_long_double() const noexcept {
            return stores_integral()
                ? static_cast<long double>(value_.integer)
                : value_.floating;
        }

        CONSTEXPR_14 void cache_parsed_value(DataType parsed_type, long double parsed_value) noexcept {
            type_ = parsed_type;

            if (parsed_type >= DataType::CSV_INT8 && parsed_type <= DataType::CSV_INT64) {
                value_.integer = static_cast<std::int64_t>(parsed_value);
            }
            else if (parsed_type == DataType::CSV_DOUBLE || parsed_type == DataType::CSV_BIGINT) {
                value_.floating = parsed_value;
            }
        }

        /** Shared validation + conversion kernel used by get(), try_get(), and as().
         *  Assigns to @p out and returns CSVConversionError::None on success.
         */
        CSVConversionError check_convert(bool& out) noexcept {
            if (this->type() != DataType::CSV_BOOL)
                return CSVConversionError::NotANumber;

            out = this->value_.boolean;
            return CSVConversionError::None;
        }

        template<typename Rep, typename Period>
        CSVConversionError check_convert(std::chrono::duration<Rep, Period>& out) noexcept {
            if (this->type() != DataType::CSV_TIMESTAMP)
                return CSVConversionError::NotANumber;

            out = std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(
                std::chrono::milliseconds(this->value_.timestamp));
            return CSVConversionError::None;
        }

        template<typename Duration>
        CSVConversionError check_convert(std::chrono::time_point<std::chrono::system_clock, Duration>& out) noexcept {
            if (this->type() != DataType::CSV_TIMESTAMP)
                return CSVConversionError::NotANumber;

            out = std::chrono::time_point<std::chrono::system_clock, Duration>(
                std::chrono::duration_cast<Duration>(std::chrono::milliseconds(this->value_.timestamp)));
            return CSVConversionError::None;
        }

        template<typename T>
        CSVConversionError check_convert(T& out) noexcept {
            IF_CONSTEXPR(std::is_arithmetic<T>::value) {
                if (!this->is_num())
                    return CSVConversionError::NotANumber;
                if (this->type_ == DataType::CSV_BIGINT)
                    return CSVConversionError::Overflow;
            }

            IF_CONSTEXPR(std::is_integral<T>::value) {
                if (this->is_float())
                    return CSVConversionError::FloatToInt;

                IF_CONSTEXPR(std::is_unsigned<T>::value) {
                    if (this->numeric_value_as_long_double() < 0)
                        return CSVConversionError::NegativeToUnsigned;
                }
            }

            IF_CONSTEXPR(!std::is_floating_point<T>::value) {
                const long double value = this->numeric_value_as_long_double();
                if (value < static_cast<long double>(std::numeric_limits<T>::min())
                    || value > static_cast<long double>(std::numeric_limits<T>::max())) {
                    return CSVConversionError::Overflow;
                }
            }

            out = this->stores_integral()
                ? static_cast<T>(this->value_.integer)
                : static_cast<T>(this->value_.floating);
            return CSVConversionError::None;
        }

        /** Check to see if value has been cached previously before evaluating. */
        inline void get_value() noexcept {
            if ((int)type_ < 0) {
                if (this->sv.empty()) {
                    type_ = DataType::CSV_NULL;
                    return;
                }

                const char* first = this->sv.data();
                const char* last = first + this->sv.size();
                typedef classify_scalar::policy_pack<
                    classify_scalar::builtin_numeric_policy<'.', false>,
                    classify_scalar::builtin_timestamp_policy,
                    classify_scalar::builtin_bool_policy
                > csv_field_policy_pack;

                type_ = classify_scalar::classify_scalar<
                    DataType,
                    true>(first, last, FieldValueOutput{ this->value_ }, csv_field_policy_pack());
            }
        }
    };

    /** Data structure for representing CSV rows */
    class CSVRow {
    public:
        template<typename RowSink, typename ParsePolicy, typename FieldPolicy, typename RowPolicy>
        friend class internals::CSVParserCore;
        friend struct internals::CSVRowRowPolicy;
        friend internals::parser::CSVParserDriverBase;
        friend struct internals::speculative::CSVRowFragment;

        CSVRow() = default;
        
        /** Construct a CSVRow view over parsed row storage. */
        CSVRow(internals::RawCSVDataPtr _data) : data(_data) {}
        CSVRow(internals::RawCSVDataPtr _data, size_t _data_start, size_t _field_bounds)
            : data(_data), data_start(_data_start), fields_start(_field_bounds) {}
        CSVRow(internals::RawCSVDataPtr _data, size_t _data_start, size_t _field_bounds, size_t _row_length)
            : data(_data), data_start(_data_start), fields_start(_field_bounds), row_length(_row_length) {}

        /** Indicates whether row is empty or not */
        CONSTEXPR bool empty() const noexcept { return this->size() == 0; }

        /** Return the number of fields in this row */
        CONSTEXPR size_t size() const noexcept { return row_length; }

        /** Return the absolute byte offset where this row starts in the source. */
        size_t byte_offset() const noexcept {
            return this->data ? this->data->source_start + this->data_start : 0;
        }

        /** @name Value Retrieval */
        ///@{
        CSVField operator[](size_t n) const;
        CSVField operator[](csv::string_view) const;
        inline std::string to_json(const std::vector<std::string>& subset = {}) const {
            const auto* converter = this->get_json_converter();
            return converter == nullptr ? "{}"
                : converter->row_to_json(this->size(), [this](size_t i) { return this->get_field(i); }, subset);
        }
        inline std::string to_json_array(const std::vector<std::string>& subset = {}) const {
            const auto* converter = this->get_json_converter();
            return converter == nullptr ? "[]"
                : converter->row_to_json_array(this->size(), [this](size_t i) { return this->get_field(i); }, subset);
        }

        /** Retrieve this row's associated column names */
        const std::vector<std::string>& get_col_names() const {
            return this->data->col_names->get_col_names();
        }

        /** Internal accessor for preserving resolved column-name lookup policy across helper types. */
        internals::ConstColNamesPtr col_names_ptr() const noexcept {
            return this->data->col_names;
        }

        /** Convert this CSVRow into an unordered map.
         *  The keys are the column names and the values are the corresponding field values.
         */
        std::unordered_map<std::string, std::string> to_unordered_map() const;

        /** Convert a selected subset of columns into an unordered map. */
        std::unordered_map<std::string, std::string> to_unordered_map(
            const std::vector<std::string>& subset
        ) const;

        #ifdef CSV_HAS_CXX20
        /** Convert this CSVRow into a std::ranges::input_range of string_views.
         *
         *  @note Requires C++20 or later.
         */
        auto to_sv_range() const {
            return std::views::iota(size_t{0}, this->size())
                | std::views::transform([this](size_t i) { return this->get_field(i); });
        }
        #endif

        /** Convert this row into a `std::vector<std::string>`.
         *
         * This conversion is primarily intended for write-side workflows, such as
         * reordering or selecting columns before forwarding the row to `CSVWriter`.
         *
         * @note This is less efficient than indexed access via `operator[]` because
         *       it materializes all fields as owning strings.
         */
        operator std::vector<std::string>() const;

        /** Return a string_view of the raw bytes of this row as they appear in
         *  the underlying parse buffer, up to (but not including) the trailing
         *  record terminator.
         *
         *  @warning The view is only valid for as long as the CSVRow (and its
         *           associated data chunk) remains alive.
         */
        csv::string_view raw_str() const noexcept;
        ///@}

        /** A random access iterator over the contents of a CSV row.
         *  Each iterator points to a CSVField.
         */
        class iterator {
        public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
            using value_type = CSVField;
            using difference_type = int;
            using pointer = std::shared_ptr<CSVField>;
            using reference = CSVField & ;
            using iterator_category = std::random_access_iterator_tag;
#endif
            iterator(const CSVRow*, int i);

            reference operator*() const;
            pointer operator->() const;

            iterator operator++(int);
            iterator& operator++();
            iterator operator--(int);
            iterator& operator--();
            iterator operator+(difference_type n) const;
            iterator operator-(difference_type n) const;
            iterator& operator+=(difference_type n);
            iterator& operator-=(difference_type n);
            difference_type operator-(const iterator& other) const noexcept;

            /** Two iterators are equal if they point to the same field */
            CONSTEXPR bool operator==(const iterator& other) const noexcept {
                return this->i == other.i;
            };

            CONSTEXPR bool operator!=(const iterator& other) const noexcept { return !operator==(other); }

#ifndef NDEBUG
            friend CSVRow;
#endif

        private:
            const CSVRow * daddy = nullptr;                      // Pointer to parent
            internals::RawCSVDataPtr data = nullptr;             // Keep data alive for lifetime of iterator
            std::shared_ptr<CSVField> field = nullptr;           // Current field pointed at
            int i = 0;                                           // Index of current field
        };

        /** A reverse iterator over the contents of a CSVRow. */
        using reverse_iterator = std::reverse_iterator<iterator>;

        /** @name Iterators
         *  @brief Each iterator points to a CSVField object.
         */
         ///@{
        iterator begin() const;
        iterator end() const noexcept;
        reverse_iterator rbegin() const noexcept;
        reverse_iterator rend() const;
        ///@}

    private:
        /** Shared implementation for field access (handles quoting and caching). */
        inline csv::string_view get_field_impl(size_t index, const internals::RawCSVDataPtr& _data) const {
            if (index >= this->size())
                throw std::runtime_error(internals::CSV_ERROR_INDEX_OUT_OF_BOUNDS);

            const size_t field_index = this->fields_start + index;
            const auto field = _data->fields[field_index];
            csv::string_view field_str;
            if (field.has_realized_storage()) {
                field_str = _data->quote_arena.view(field.start, field.length);
            }
            else {
                field_str = csv::string_view(_data->data).substr(this->data_start + field.start, field.length);
            }

            if (_data->has_ws_trimming) {
                field_str = internals::get_trimmed(field_str, _data->ws_flags);
            }

            return field_str;
        }

        CSVField make_field(size_t index, const internals::RawCSVDataPtr& _data) const;

        /** Retrieve a string view corresponding to the specified index */
        csv::string_view get_field(size_t index) const;

        /** Iterator-safe field access using explicit data pointer 
         *  (prevents accessing freed data when CSVRow is reassigned)
         */
        csv::string_view get_field_safe(size_t index, internals::RawCSVDataPtr _data) const;

        const internals::JsonConverter* get_json_converter() const {
            if (this->data.get() == nullptr) {
                return nullptr;
            }

            return &this->data->json_converter.get_or_create([this]() {
                const std::vector<std::string> columns = this->data->col_names
                    ? this->data->col_names->get_col_names()
                    : std::vector<std::string>();
                return std::make_shared<internals::JsonConverter>(columns);
            });
        }

        internals::RawCSVDataPtr data;

        /** Byte offset where this row begins within the shared row storage. */
        size_t data_start = 0;

        /** Field-list offset where this row begins. */
        size_t fields_start = 0;

        /** How many columns this row spans */
        size_t row_length = 0;

        /** Byte offset one past the last byte belonging to this row. */
        size_t data_end = (std::numeric_limits<size_t>::max)();
    };

#ifdef _MSC_VER
#pragma region CSVField::get Specializations
#endif
    /** Retrieve this field's original string */
    template<>
    inline std::string CSVField::get<std::string>() {
        return std::string(this->sv);
    }

    /** Retrieve a view over this field's string
     *
     *  @warning This string_view is only guaranteed to be valid as long as this
     *           CSVRow is still alive.
     */
    template<>
    CONSTEXPR_14 csv::string_view CSVField::get<csv::string_view>() {
        return this->sv;
    }

    /** Retrieve this field's value as a long double */
    template<>
    inline long double CSVField::get<long double>() {
        if (!is_num())
            throw std::runtime_error(internals::ERROR_NAN);

        return this->numeric_value_as_long_double();
    }

    /** Non-throwing retrieval of field as std::string */
    template<>
    inline bool CSVField::try_get<std::string>(std::string& out) noexcept {
        out = std::string(this->sv);
        return true;
    }

    /** Non-throwing retrieval of field as csv::string_view */
    template<>
    CONSTEXPR_14 bool CSVField::try_get<csv::string_view>(csv::string_view& out) noexcept {
        out = this->sv;
        return true;
    }

    /** Non-throwing retrieval of field as long double */
    template<>
    inline bool CSVField::try_get<long double>(long double& out) noexcept {
        if (!is_num())
            return false;

        out = this->numeric_value_as_long_double();
        return true;
    }
#ifdef _MSC_VER
#pragma endregion CSVField::get Specializations
#endif

    /** Compares the contents of this field to a string */
    template<>
    CONSTEXPR bool CSVField::operator==(const char * other) const noexcept
    {
        return this->sv == other;
    }

    /** Compares the contents of this field to a string */
    template<>
    CONSTEXPR bool CSVField::operator==(csv::string_view other) const noexcept
    {
        return this->sv == other;
    }
}

/** @file
 *  @brief Shared contracts for row deque implementations
 */



#include <vector>

#if CSV_ENABLE_THREADS
/** @file
 *  @brief Thread-safe deque for producer-consumer patterns
 * 
 *  Generic container used for cross-thread communication in the CSV parser.
 *  Parser thread pushes rows, main thread pops them.
 *
 *  Design notes: see THREADSAFE_DEQUE_DESIGN.md for protocol details,
 *  invariants, and producer/consumer timing diagrams.
 */


#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

/** @file
 *  @brief Shared helpers for batch-backed row queues.
 */


#include <cstddef>
#include <deque>
#include <iterator>
#include <vector>

namespace csv {
    namespace internals {
        template<typename T>
        size_t drain_front_batches(
            std::deque<std::vector<T>>& batches,
            size_t& front_index,
            size_t& size,
            std::vector<T>& out,
            size_t max_items
        ) {
            const size_t drain_count = size < max_items ? size : max_items;
            size_t remaining = drain_count;

            out.reserve(out.size() + drain_count);

            while (remaining > 0) {
                auto& batch = batches.front();
                const size_t available = batch.size() - front_index;
                const size_t take = available < remaining ? available : remaining;
                const auto first_offset = static_cast<typename std::vector<T>::difference_type>(front_index);
                const auto take_offset = static_cast<typename std::vector<T>::difference_type>(take);
                auto first = batch.begin() + first_offset;
                auto last = first + take_offset;

                out.insert(
                    out.end(),
                    std::make_move_iterator(first),
                    std::make_move_iterator(last)
                );

                front_index += take;
                size -= take;
                remaining -= take;

                while (!batches.empty() && front_index >= batches.front().size()) {
                    batches.pop_front();
                    front_index = 0;
                }
            }

            return drain_count;
        }
    }
}


namespace csv {
    namespace internals {
        /** A std::deque wrapper which allows multiple read and write threads to concurrently
         *  access it along with providing read threads the ability to wait for the deque
         *  to become populated.
         *
         *  Concurrency strategy: writer-side mutations (push_back/pop_front) are locked;
         *  hot-path flags (empty/is_waitable) are atomic; inspect() is the synchronized
         *  observation path for tests and diagnostics. Keep inspect() as a local
         *  ThreadSafeDeque affordance instead of promoting a generic queue-view type
         *  into the parser queue concept.
         */
        template<typename T>
        class ThreadSafeDeque {
        public:
            ThreadSafeDeque(size_t notify_size = 100) : _notify_size(notify_size) {}

            ThreadSafeDeque(const ThreadSafeDeque& other) {
                std::lock_guard<std::mutex> lock{ other._lock };
                this->batches_ = other.batches_;
                this->front_index_ = other.front_index_;
                this->size_ = other.size_;
                this->_notify_size = other._notify_size;
                this->_is_empty.store(other._is_empty.load(std::memory_order_acquire), std::memory_order_release);
                this->_is_waitable.store(other._is_waitable.load(std::memory_order_acquire), std::memory_order_release);
            }

            ThreadSafeDeque(const std::deque<T>& source) : ThreadSafeDeque() {
                std::vector<T> rows;
                rows.reserve(source.size());
                for (const auto& row : source) {
                    rows.push_back(row);
                }
                if (!rows.empty()) {
                    this->batches_.push_back(std::move(rows));
                    this->size_ = source.size();
                }
                this->_is_empty.store(source.empty(), std::memory_order_release);
            }

            bool empty() const noexcept {
                return this->_is_empty.load(std::memory_order_acquire);
            }

            void push_back(T&& item) {
                std::lock_guard<std::mutex> lock{ this->_lock };
                std::vector<T> batch;
                batch.push_back(std::move(item));
                this->batches_.push_back(std::move(batch));
                this->size_++;
                this->_is_empty.store(false, std::memory_order_release);

                if (this->size_ >= _notify_size) {
                    this->_cond.notify_all();
                }
            }

            void append_rows(std::vector<T>&& rows) {
                if (rows.empty()) {
                    return;
                }

                std::lock_guard<std::mutex> lock{ this->_lock };
                this->size_ += rows.size();
                this->batches_.push_back(std::move(rows));
                this->_is_empty.store(false, std::memory_order_release);

                if (this->size_ >= _notify_size) {
                    this->_cond.notify_all();
                }
            }

            T pop_front() noexcept {
                std::lock_guard<std::mutex> lock{ this->_lock };
                T item = std::move(this->batches_.front()[this->front_index_]);
                this->front_index_++;
                this->size_--;
                this->discard_exhausted_front_batch();

                if (this->size_ == 0) {
                    this->_is_empty.store(true, std::memory_order_release);
                }

                return item;
            }

            /** Move up to @p max_items rows into a caller-owned batch buffer under one lock.
             *
             *  This is the preferred consumer path for chunked reads: it preserves queue
             *  semantics while amortizing mutex traffic across many rows. Complete queued
             *  batches are moved as contiguous spans; per-row moves only remain when the
             *  requested limit splits a batch.
             */
            size_t drain_front(std::vector<T>& out, size_t max_items) {
                std::lock_guard<std::mutex> lock{ this->_lock };
                const size_t drain_count = drain_front_batches(
                    this->batches_,
                    this->front_index_,
                    this->size_,
                    out,
                    max_items
                );

                if (this->size_ == 0) {
                    this->_is_empty.store(true, std::memory_order_release);
                }

                return drain_count;
            }

            /** Invoke @p callback with a synchronized copy of queued rows.
             *
             *  Intended for tests and diagnostics that need stable indexed
             *  observation without exposing unsynchronized random access. The
             *  plain vector snapshot is deliberate: callers do not need to know
             *  whether the underlying queue is batch-backed, and RowDequeLike
             *  should not grow a custom inspection-view abstraction.
             */
            template<typename Callback>
            void inspect(Callback&& callback) const {
                std::vector<T> snapshot;
                {
                    std::lock_guard<std::mutex> lock{ this->_lock };
                    snapshot.reserve(this->size_);

                    bool first_batch = true;
                    for (const auto& batch : this->batches_) {
                        const size_t start = first_batch ? this->front_index_ : 0;
                        first_batch = false;

                        for (size_t i = start; i < batch.size(); ++i) {
                            snapshot.push_back(batch[i]);
                        }
                    }
                }

                std::forward<Callback>(callback)(snapshot);
            }

            /** Returns true if a thread is actively pushing items to this deque */
            bool is_waitable() const noexcept {
                return this->_is_waitable.load(std::memory_order_acquire);
            }

            void wait() {
                if (!is_waitable()) {
                    return;
                }

                std::unique_lock<std::mutex> lock{ this->_lock };
                this->_cond.wait(lock, [this] { return this->size_ >= _notify_size || !this->is_waitable(); });
                lock.unlock();
            }

            size_t size() const noexcept {
                std::lock_guard<std::mutex> lock{ this->_lock };
                return this->size_;
            }

            /** Tell listeners that this deque is actively being pushed to */
            void notify_all() {
                std::lock_guard<std::mutex> lock{ this->_lock };
                this->_is_waitable.store(true, std::memory_order_release);
                this->_cond.notify_all();
            }

            void kill_all() {
                std::lock_guard<std::mutex> lock{ this->_lock };
                this->_is_waitable.store(false, std::memory_order_release);
                this->_cond.notify_all();
            }

        private:
            std::atomic<bool> _is_empty{ true };      // Lock-free empty() check
            std::atomic<bool> _is_waitable{ false };  // Lock-free is_waitable() check
            size_t _notify_size;
            mutable std::mutex _lock;
            std::condition_variable _cond;
            std::deque<std::vector<T>> batches_;
            size_t front_index_ = 0;
            size_t size_ = 0;

            void discard_exhausted_front_batch() noexcept {
                while (!this->batches_.empty() && this->front_index_ >= this->batches_.front().size()) {
                    this->batches_.pop_front();
                    this->front_index_ = 0;
                }
            }
        };
    }
}

#else
/** @file
 *  @brief Single-threaded row deque implementation
 */


#include <cstddef>
#include <deque>
#include <iterator>
#include <utility>
#include <vector>

namespace csv {
    namespace internals {
        /** Minimal row queue used when parser threading is disabled.
         *
         *  This intentionally stores rows directly. It only needs to satisfy the
         *  parser queue operations in RowDequeLike; it does not mirror
         *  ThreadSafeDeque's batch storage or test-only inspection helper.
         */
        template<typename T>
        class SingleThreadDeque {
        public:
            SingleThreadDeque(size_t notify_size = 100) {
                (void)notify_size;
            }

            SingleThreadDeque(const SingleThreadDeque& other) {
                this->records_ = other.records_;
                this->_is_empty = other._is_empty;
                this->_is_waitable = other._is_waitable;
            }

            SingleThreadDeque(const std::deque<T>& source)
                : _is_empty(source.empty()),
                  records_(source) {}

            bool empty() const noexcept {
                return this->_is_empty;
            }

            void push_back(T&& item) {
                this->records_.push_back(std::move(item));
                this->_is_empty = false;
            }

            void append_rows(std::vector<T>&& rows) {
                if (rows.empty()) {
                    return;
                }

                this->records_.insert(
                    this->records_.end(),
                    std::make_move_iterator(rows.begin()),
                    std::make_move_iterator(rows.end())
                );
                this->_is_empty = false;
            }

            T pop_front() noexcept {
                T item = std::move(this->records_.front());
                this->records_.pop_front();

                if (this->records_.empty()) {
                    this->_is_empty = true;
                }

                return item;
            }

            /** Move up to @p max_items rows into a caller-owned batch buffer. */
            size_t drain_front(std::vector<T>& out, size_t max_items) {
                const size_t drain_count = this->records_.size() < max_items ? this->records_.size() : max_items;
                out.reserve(out.size() + drain_count);

                for (size_t i = 0; i < drain_count; ++i) {
                    out.push_back(std::move(this->records_.front()));
                    this->records_.pop_front();
                }

                if (this->records_.empty()) {
                    this->_is_empty = true;
                }

                return drain_count;
            }

            bool is_waitable() const noexcept {
                return this->_is_waitable;
            }

            void wait() {
                // No-op in single-thread mode.
            }

            size_t size() const noexcept {
                return this->records_.size();
            }

            void notify_all() {
                this->_is_waitable = true;
            }

            void kill_all() {
                this->_is_waitable = false;
            }

        private:
            bool _is_empty = true;
            bool _is_waitable = false;
            std::deque<T> records_;
        };
    }
}

#endif

#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef CSV_HAS_CXX20
#include <concepts>
#endif

namespace csv {
    namespace internals {
#if !CSV_ENABLE_THREADS
    template<typename T>
    using ThreadSafeDeque = SingleThreadDeque<T>;
#endif

#ifdef CSV_HAS_CXX20
        // This concept is intentionally limited to parser queue semantics.
        // Diagnostics such as ThreadSafeDeque::inspect() stay out of the
        // contract so single-threaded and threaded queues can use different
        // storage without inventing a generic inspection-view abstraction.
        template<typename Q, typename T>
        concept RowDequeLike = requires(Q q, const Q cq, T item, size_t n, std::vector<T> batch) {
            { Q(100) };
            { q.push_back(std::move(item)) } -> std::same_as<void>;
            { q.append_rows(std::move(batch)) } -> std::same_as<void>;
            { q.pop_front() } -> std::same_as<T>;
            { q.drain_front(batch, n) } -> std::same_as<size_t>;
            { cq.empty() } -> std::same_as<bool>;
            { cq.is_waitable() } -> std::same_as<bool>;
            { q.wait() } -> std::same_as<void>;
            { q.notify_all() } -> std::same_as<void>;
            { q.kill_all() } -> std::same_as<void>;
            { cq.size() } -> std::same_as<size_t>;
        };

#if CSV_ENABLE_THREADS
            static_assert(RowDequeLike<ThreadSafeDeque<int>, int>, "ThreadSafeDeque must satisfy RowDequeLike contract");
#else
            static_assert(RowDequeLike<SingleThreadDeque<int>, int>, "SingleThreadDeque must satisfy RowDequeLike contract");
            static_assert(RowDequeLike<ThreadSafeDeque<int>, int>, "Selected ThreadSafeDeque alias must satisfy RowDequeLike contract");
#endif
#endif
    }
}


namespace csv {
    namespace internals {
        constexpr const int UNINITIALIZED_FIELD = -1;

        /** Helper constexpr function to initialize an array with all the elements set to value. */
        template<typename OutArray, typename T = typename OutArray::type>
        CSV_CONST CONSTEXPR_17 OutArray arrayToDefault(T&& value)
        {
            OutArray a {};
            for (auto& e : a)
                 e = value;
            return a;
        }

        /** Create a vector v where each index i corresponds to the
         *  ASCII number for a character and, v[i + 128] labels it according to
         *  the CSVReader::ParseFlags enum
         */
        CSV_CONST CONSTEXPR_17 ParseFlagMap make_parse_flags(char delimiter) {
            auto ret = arrayToDefault<ParseFlagMap>(ParseFlags::NOT_SPECIAL);
            ret[delimiter + CHAR_OFFSET] = ParseFlags::DELIMITER;
            ret['\r' + CHAR_OFFSET] = ParseFlags::CARRIAGE_RETURN;
            ret['\n' + CHAR_OFFSET] = ParseFlags::NEWLINE;
            return ret;
        }

        /** Create a vector v where each index i corresponds to the
         *  ASCII number for a character and, v[i + 128] labels it according to
         *  the CSVReader::ParseFlags enum
         */
        CSV_CONST CONSTEXPR_17 ParseFlagMap make_parse_flags(char delimiter, char quote_char) {
            std::array<ParseFlags, 256> ret = make_parse_flags(delimiter);
            ret[quote_char + CHAR_OFFSET] = ParseFlags::QUOTE;
            return ret;
        }

        inline char infer_char_for_flag(
            const ParseFlagMap& parse_flags,
            ParseFlags target,
            char fallback
        ) noexcept {
            for (size_t i = 0; i < parse_flags.size(); ++i) {
                if (parse_flags[i] == target) {
                    return static_cast<char>(static_cast<int>(i) - CHAR_OFFSET);
                }
            }

            return fallback;
        }

        inline char infer_delimiter(const ParseFlagMap& parse_flags) noexcept {
            return infer_char_for_flag(parse_flags, ParseFlags::DELIMITER, ',');
        }

        // fallback is returned when no QUOTE flag exists in parse_flags (e.g. no_quote mode).
        // Pass the delimiter so SIMD stops there instead of on a byte that is NOT_SPECIAL.
        inline char infer_quote_char(const ParseFlagMap& parse_flags, char fallback = '"') noexcept {
            return infer_char_for_flag(parse_flags, ParseFlags::QUOTE, fallback);
        }

        /** Create a vector v where each index i corresponds to the
         *  ASCII number for a character c and, v[i + 128] is true if
         *  c is a whitespace character
         */
        CSV_CONST CONSTEXPR_17 WhitespaceMap make_ws_flags(const char* ws_chars, size_t n_chars) {
            auto ret = arrayToDefault<WhitespaceMap>(false);
            for (size_t j = 0; j < n_chars; j++) {
                ret[ws_chars[j] + CHAR_OFFSET] = true;
            }
            return ret;
        }

        inline WhitespaceMap make_ws_flags(const std::vector<char>& flags) {
            return make_ws_flags(flags.data(), flags.size());
        }

        /** Return the number of leading BOM bytes to skip, or throw for unsupported Unicode encodings. */
        CSV_INLINE size_t get_bom_skip_or_throw(csv::string_view data, bool& utf8_bom);

        /** Explicit DFA state at a parse boundary.
         *
         *  This is intentionally about parser control flow, not field metadata.
         *  `pending_quote` represents the ambiguous case where a chunk ended at
         *  a quote while already inside a quoted field; the next byte determines
         *  whether that quote closes the field, escapes a quote, or is kept as
         *  non-strict literal content.
         */
        struct ParserDFAState {
            ParserDFAState() noexcept = default;
            ParserDFAState(
                bool quote_escape,
                bool pending_quote = false,
                bool pending_linefeed = false
            ) noexcept
                : quote_escape(quote_escape),
                  pending_quote(pending_quote),
                  pending_linefeed(pending_linefeed) {}

            bool quote_escape = false;
            bool pending_quote = false;
            bool pending_linefeed = false;
        };

        inline bool parser_dfa_state_equal(ParserDFAState lhs, ParserDFAState rhs) noexcept {
            return lhs.quote_escape == rhs.quote_escape
                && lhs.pending_quote == rhs.pending_quote
                && lhs.pending_linefeed == rhs.pending_linefeed;
        }

        struct ParserChunkOptions {
            ParserChunkOptions() noexcept = default;
            explicit ParserChunkOptions(
                ParserDFAState initial_state,
                bool scan_bom = true,
                size_t source_start = 0
            ) noexcept
                : initial_state(initial_state),
                  scan_bom(scan_bom),
                  source_start(source_start) {}

            ParserDFAState initial_state;
            bool scan_bom = true;
            size_t source_start = 0;
        };

        struct ParserChunkResult {
            ParserChunkResult() noexcept = default;
            ParserChunkResult(
                ParserDFAState initial_state,
                ParserDFAState ending_state,
                size_t complete_prefix_length
            ) noexcept
                : initial_state(initial_state),
                  ending_state(ending_state),
                  complete_prefix_length(complete_prefix_length) {}

            ParserDFAState initial_state;
            ParserDFAState ending_state;
            size_t complete_prefix_length = 0;
        };

        struct CSVParseWindowResult {
            size_t complete_prefix_length = 0;
        };
    }

    /** Standard type for storing collection of rows. */
    using RowCollection = internals::ThreadSafeDeque<CSVRow>;

    namespace internals {
        /** Default parse policy hook.
         *
         *  The permissive policy intentionally does nothing. Calls are direct
         *  template calls that optimize away in the default parser, leaving a
         *  zero-cost extension point for future validation policies.
         */
        struct PermissiveParsePolicy {
            void begin_chunk(const RawCSVDataPtr&) noexcept {}
            void begin_row(const CSVRow&) noexcept {}
            void push_field(const RawCSVField&) noexcept {}
            void end_row(const CSVRow&) noexcept {}
        };

        /** Default field policy for the CSVRow path.
         *
         *  Owns RawCSVData/RawCSVFieldList setup and appends RawCSVField metadata
         *  exactly as the historical parser core did.
         */
        template<bool EagerClassify = false>
        struct CSVRowFieldPolicy {
            void begin_chunk(
                RawCSVDataPtr& data_ptr,
                RawCSVFieldList*& fields,
                const ParseFlagMap& parse_flags,
                const WhitespaceMap& ws_flags,
                bool has_ws_trimming,
                const ColNamesPtr& col_names
            ) const {
                data_ptr = std::make_shared<RawCSVData>();
                data_ptr->parse_flags = parse_flags;
                data_ptr->ws_flags = ws_flags;
                data_ptr->has_ws_trimming = has_ws_trimming;
                data_ptr->col_names = col_names;
                fields = &(data_ptr->fields);
            }

            const RawCSVField& push_field(
                RawCSVData& data,
                RawCSVFieldList& fields,
                size_t row_start,
                int field_start,
                size_t field_length,
                bool field_has_double_quote
            ) const {
                const size_t raw_start = field_start == UNINITIALIZED_FIELD
                    ? 0
                    : static_cast<size_t>(field_start);
                size_t stored_start = raw_start;
                size_t stored_length = field_length;
                bool is_realized = false;

                if (field_has_double_quote) {
                    stored_start = this->append_realized_quoted_field(
                        data,
                        row_start + raw_start,
                        field_length,
                        stored_length
                    );
                    is_realized = true;
                }

                fields.emplace_back(
                    stored_start,
                    stored_length,
                    is_realized
                );
                this->append_scalar(
                    data,
                    fields[fields.size() - 1],
                    row_start,
                    std::integral_constant<bool, EagerClassify>()
                );
                return fields[fields.size() - 1];
            }

        private:
            void append_scalar(
                RawCSVData&,
                const RawCSVField&,
                size_t,
                std::false_type
            ) const {}

            void append_scalar(
                RawCSVData& data,
                const RawCSVField& field,
                size_t row_start,
                std::true_type
            ) const {
                csv::string_view field_str;
                if (field.has_realized_storage()) {
                    field_str = data.quote_arena.view(field.start, field.length);
                }
                else {
                    field_str = csv::string_view(data.data).substr(row_start + field.start, field.length);
                }

                if (data.has_ws_trimming) {
                    field_str = internals::get_trimmed(field_str, data.ws_flags);
                }

                data.field_scalars.emplace_back(internals::classify_field_scalar(field_str));
            }

            CSVChunkIndex append_realized_quoted_field(
                RawCSVData& data,
                size_t field_start,
                size_t field_length,
                size_t& realized_length
            ) const {
                using internals::ParseFlags;

                const csv::string_view field_str = csv::string_view(data.data).substr(field_start, field_length);
                // Allocate the original length as an upper bound, then compact doubled
                // quotes in one pass. Wasting a byte per escaped quote pair is cheaper
                // than scanning quote-heavy fields twice in the parser hot path.
                auto allocation = data.quote_arena.allocate_contiguous(field_str.size());
                char* out = allocation.data;
                for (size_t i = 0; i < field_str.size(); ++i) {
                    if (data.parse_flags[field_str[i] + CHAR_OFFSET] == ParseFlags::QUOTE
                        && i + 1 < field_str.size()
                        && data.parse_flags[field_str[i + 1] + CHAR_OFFSET] == ParseFlags::QUOTE) {
                        *(out++) = field_str[i++];
                        continue;
                    }

                    *(out++) = field_str[i];
                }

                realized_length = static_cast<size_t>(out - allocation.data);
                return allocation.offset;
            }
        };

        /** Default row policy for the CSVRow path. */
        struct CSVRowRowPolicy {
            CSVRow make_initial_row(const RawCSVDataPtr& data_ptr) const {
                return CSVRow(data_ptr);
            }

            CSVRow make_next_row(
                const RawCSVDataPtr& data_ptr,
                size_t data_pos,
                size_t fields_size
            ) const {
                return CSVRow(data_ptr, data_pos, fields_size);
            }

            void finalize_row(
                CSVRow& row,
                const RawCSVFieldList& fields,
                size_t raw_end
            ) const {
                row.row_length = fields.size() - row.fields_start;
                row.data_end = raw_end;
            }
        };

        inline void csv_push_row(RowCollection& sink, CSVRow&& row) {
            sink.push_back(std::move(row));
        }

        inline void csv_push_row(std::vector<CSVRow>& sink, CSVRow&& row) {
            sink.push_back(std::move(row));
        }

        inline void csv_append_rows(RowCollection& sink, std::vector<CSVRow>&& rows) {
            sink.append_rows(std::move(rows));
        }

        inline void csv_append_rows(std::vector<CSVRow>& sink, std::vector<CSVRow>&& rows) {
            if (rows.empty()) {
                return;
            }

            sink.reserve(sink.size() + rows.size());
            for (auto& row : rows) {
                sink.push_back(std::move(row));
            }
        }

        template<
            typename RowSink = RowCollection,
            typename ParsePolicy = PermissiveParsePolicy,
            typename FieldPolicy = CSVRowFieldPolicy<false>,
            typename RowPolicy = CSVRowRowPolicy>
        class CSVParserCore {
        public:
            CSVParserCore() = default;

            CSVParserCore(
                const CSVFormat& source_format,
                const ColNamesPtr& col_names
            ) : col_names_(col_names) {
                // Only initialize the fields that are stable before format resolution.
                // parse_flags_ and simd_sentinels_ are always set by resolve_format_from_head,
                // so there is no point computing them here with a placeholder delimiter.
                ws_flags_ = internals::make_ws_flags(
                    source_format.trim_chars.data(), source_format.trim_chars.size()
                );
                has_ws_trimming_ = !source_format.trim_chars.empty();
            }

            CSVParserCore(
                const ParseFlagMap& parse_flags,
                const WhitespaceMap& ws_flags
            ) : parse_flags_(parse_flags),
                ws_flags_(ws_flags) {
                const char d = internals::infer_delimiter(parse_flags);
                simd_sentinels_ = SentinelVecs(d, internals::infer_quote_char(parse_flags, d));
                has_ws_trimming_ = std::any_of(ws_flags.begin(), ws_flags.end(), [](bool b) { return b; });
            }

            CSVParserCore(
                const ParseFlagMap& parse_flags,
                const WhitespaceMap& ws_flags,
                const ColNamesPtr& col_names
            ) : CSVParserCore(parse_flags, ws_flags) {
                this->col_names_ = col_names;
            }

            ~CSVParserCore() {}

            /** Indicate the last block of data has been parsed. */
            void end_feed() {
                using internals::ParseFlags;

                bool empty_last_field = this->data_ptr_
                    && this->data_ptr_->_data
                    && !this->data_ptr_->data.empty()
                    && (parse_flag(this->data_ptr_->data.back()) == ParseFlags::DELIMITER
                        || parse_flag(this->data_ptr_->data.back()) == ParseFlags::QUOTE);

                if (this->field_length_ > 0 || empty_last_field) {
                    this->push_field();
                }

                if (this->current_row_.size() > 0) {
                    this->push_row(this->data_ptr_ ? this->data_ptr_->data.size() : this->current_row_start());
                }
            }

            CONSTEXPR_17 ParseFlags parse_flag(const char ch) const noexcept {
                return parse_flags_.data()[ch + CHAR_OFFSET];
            }

            CONSTEXPR_17 ParseFlags compound_parse_flag(const char ch) const noexcept {
                return quote_escape_flag(parse_flag(ch), this->quote_escape_);
            }

            /** Whether or not this CSV has a UTF-8 byte order mark. */
            CONSTEXPR bool utf8_bom() const { return this->utf8_bom_; }

            void set_output(RowSink& output) noexcept {
                this->output_ = &output;
            }

            RowSink& output() noexcept {
                return *this->output_;
            }

            /** Seed the DFA state for the next parse call. */
            void reset_with_initial_state(ParserDFAState state) noexcept {
                this->initial_state_ = state;
            }

            /** Convenience overload for callers that only care about quote state. */
            void reset_with_initial_state(bool starts_in_quoted, bool in_escape = false) noexcept {
                this->reset_with_initial_state(ParserDFAState{ starts_in_quoted, in_escape });
            }

            /** Return the DFA state observed when the most recent parse call stopped. */
            ParserDFAState ending_state() const noexcept {
                return this->ending_state_;
            }

            ParserChunkResult parse_chunk(
                csv::string_view chunk,
                std::shared_ptr<void> owner
            ) {
                return this->parse_prepared_chunk(
                    chunk,
                    std::move(owner),
                    ParserChunkOptions(this->initial_state_)
                );
            }

            ParserChunkResult parse_chunk(
                csv::string_view chunk,
                std::shared_ptr<void> owner,
                const ParserChunkOptions& options
            ) {
                return this->parse_prepared_chunk(chunk, std::move(owner), options);
            }

            ParserChunkResult parse_chunk(
                csv::string_view chunk,
                std::shared_ptr<void> owner,
                RowSink& output
            ) {
                return this->parse_chunk(
                    chunk,
                    std::move(owner),
                    output,
                    ParserChunkOptions(this->initial_state_)
                );
            }

            ParserChunkResult parse_chunk(
                csv::string_view chunk,
                std::shared_ptr<void> owner,
                RowSink& output,
                size_t source_start
            ) {
                return this->parse_chunk(
                    chunk,
                    std::move(owner),
                    output,
                    ParserChunkOptions(this->initial_state_, true, source_start)
                );
            }

            ParserChunkResult parse_chunk(
                csv::string_view chunk,
                std::shared_ptr<void> owner,
                RowSink& output,
                const ParserChunkOptions& options
            ) {
                this->output_ = &output;
                return this->parse_prepared_chunk(
                    chunk,
                    std::move(owner),
                    options
                );
            }

        protected:
            void set_col_names(const ColNamesPtr& col_names) {
                this->col_names_ = col_names;
            }

            void set_whitespace_flags(const WhitespaceMap& ws_flags) {
                this->ws_flags_ = ws_flags;
                this->has_ws_trimming_ = std::any_of(ws_flags.begin(), ws_flags.end(), [](bool b) { return b; });
            }

            void set_parse_flags(const ParseFlagMap& parse_flags) {
                this->parse_flags_ = parse_flags;
                const char d = internals::infer_delimiter(parse_flags);
                this->simd_sentinels_ = SentinelVecs(d, internals::infer_quote_char(parse_flags, d));
            }

            void set_parse_flags(const ParseFlagMap& parse_flags, char delimiter, char quote_char) {
                this->parse_flags_ = parse_flags;
                this->simd_sentinels_ = SentinelVecs(delimiter, quote_char);
            }

            /** @name Current Parser State */
            ///@{
            CSVRow current_row_;
            RawCSVDataPtr data_ptr_ = nullptr;
            ColNamesPtr col_names_ = nullptr;
            RawCSVFieldList* fields_ = nullptr;
            int field_start_ = UNINITIALIZED_FIELD;
            size_t field_length_ = 0;

            /** Precomputed SIMD broadcast vectors for find_next_non_special. */
            SentinelVecs simd_sentinels_;

            /** An array where the (i + 128)th slot gives the ParseFlags for ASCII character i. */
            ParseFlagMap parse_flags_;
            ///@}

            /** Parse the current chunk of data and return the completed-row prefix length. */
            size_t parse() {
                using internals::ParseFlags;

                const ParserDFAState start_state = this->initial_state_;
                this->quote_escape_ = start_state.quote_escape;
                this->pending_quote_ = start_state.pending_quote;
                this->pending_linefeed_ = start_state.pending_linefeed;
                this->data_pos_ = 0;
                this->current_row_start() = 0;
                if (this->scan_bom_for_current_chunk_) {
                    this->strip_unicode_bom();
                }

                auto& in = this->data_ptr_->data;

                // Resolve any pending state from the previous chunk in speculative parsing
                this->resolve_pending_linefeed_at_start(in);
                this->resolve_pending_quote_at_start(in);

                while (this->data_pos_ < in.size()) {
                    const size_t raw_end = this->data_pos_;
                    switch (compound_parse_flag(in[this->data_pos_])) {
                    case ParseFlags::DELIMITER:
                        this->push_field();
                        this->data_pos_++;
                        break;

                    case ParseFlags::CARRIAGE_RETURN:
                        if (this->data_pos_ + 1 == in.size()) {
                            this->pending_linefeed_ = true;
                            this->data_pos_++;
                            this->finish_row(raw_end);
                            break;
                        }
                        else if (parse_flag(in[this->data_pos_ + 1]) == ParseFlags::NEWLINE) {
                            this->data_pos_++;
                        }

                        CSV_FALLTHROUGH;

                    case ParseFlags::NEWLINE:
                        this->data_pos_++;
                        this->finish_row(raw_end);
                        break;

                    case ParseFlags::NOT_SPECIAL:
                        this->parse_field();
                        break;

                    case ParseFlags::QUOTE_ESCAPE_QUOTE:
                        if (data_pos_ + 1 == in.size()) {
                            this->pending_quote_ = true;
                            return this->finish_parse(this->current_row_start());
                        }
                        else if (data_pos_ + 1 < in.size()) {
                            auto next_ch = parse_flag(in[data_pos_ + 1]);
                            if (next_ch >= ParseFlags::DELIMITER) {
                                quote_escape_ = false;
                                data_pos_++;
                                break;
                            }
                            else if (next_ch == ParseFlags::QUOTE) {
                                data_pos_ += 2;
                                this->field_length_ += 2;
                                this->field_has_double_quote_ = true;
                                break;
                            }
                        }

                        this->field_length_++;
                        data_pos_++;
                        break;

                    default:
                        if (this->field_length_ == 0) {
                            quote_escape_ = true;
                            data_pos_++;
                            if (field_start_ == UNINITIALIZED_FIELD && data_pos_ < in.size() && !ws_flag(in[data_pos_]))
                                field_start_ = (int)(data_pos_ - current_row_start());
                            break;
                        }

                        this->field_length_++;
                        data_pos_++;
                        break;
                    }
                }

                return this->finish_parse(this->current_row_start());
            }

            /** Create a new RawCSVDataPtr for a new chunk of data. */
            void reset_data_ptr() {
                this->field_policy_.begin_chunk(
                    this->data_ptr_,
                    this->fields_,
                    this->parse_flags_,
                    this->ws_flags_,
                    this->has_ws_trimming_,
                    this->col_names_
                );
            }

            const WhitespaceMap& whitespace_flags() const noexcept {
                return this->ws_flags_;
            }

            void emit_row(CSVRow&& row) {
                if (this->output_) {
                    csv_push_row(*this->output_, std::move(row));
                }
            }

        private:
            /** An array where the (i + 128)th slot determines whether ASCII character i should
             *  be trimmed.
             */
            WhitespaceMap ws_flags_;

            /** True when at least one whitespace trim character is configured.
             *  Used to skip trim loops entirely in the common no-trim case.
             */
            bool has_ws_trimming_ = false;
            bool quote_escape_ = false;
            bool pending_quote_ = false;
            bool pending_linefeed_ = false;
            bool field_has_double_quote_ = false;
            ParserDFAState initial_state_;
            ParserDFAState ending_state_;
            bool scan_bom_for_current_chunk_ = true;

            /** Where we are in the current data block. */
            size_t data_pos_ = 0;

            /** Whether or not an attempt to find Unicode BOM has been made. */
            bool unicode_bom_scan_ = false;
            bool utf8_bom_ = false;

            RowSink* output_ = nullptr;
            ParsePolicy policy_;
            FieldPolicy field_policy_;
            RowPolicy row_policy_;

            CONSTEXPR_17 bool ws_flag(const char ch) const noexcept {
                return ws_flags_.data()[ch + CHAR_OFFSET];
            }

            size_t& current_row_start() {
                return this->current_row_.data_start;
            }

            void resolve_pending_quote_at_start(csv::string_view in) {
                using internals::ParseFlags;

                if (!this->pending_quote_ || this->data_pos_ >= in.size()) {
                    return;
                }

                const ParseFlags next_ch = this->parse_flag(in[this->data_pos_]);
                this->pending_quote_ = false;

                if (next_ch >= ParseFlags::DELIMITER) {
                    this->quote_escape_ = false;
                    return;
                }

                if (next_ch == ParseFlags::QUOTE) {
                    this->quote_escape_ = true;
                    this->field_has_double_quote_ = true;
                    this->field_length_++;
                    this->data_pos_++;
                    return;
                }

                this->quote_escape_ = true;
                this->field_length_++;
            }

            void resolve_pending_linefeed_at_start(csv::string_view in) {
                using internals::ParseFlags;

                if (!this->pending_linefeed_ || this->data_pos_ >= in.size()) {
                    return;
                }

                this->pending_linefeed_ = false;
                if (this->parse_flag(in[this->data_pos_]) == ParseFlags::NEWLINE) {
                    this->data_pos_++;
                    this->current_row_start() = this->data_pos_;
                }
            }

            void parse_field() noexcept {
                using internals::ParseFlags;
                auto& in = this->data_ptr_->data;

                if (field_start_ == UNINITIALIZED_FIELD)
                    field_start_ = (int)(data_pos_ - current_row_start());

#if !defined(CSV_NO_SIMD)
                data_pos_ = find_next_non_special(in, data_pos_, this->simd_sentinels_);
#endif

                while (data_pos_ < in.size() && compound_parse_flag(in[data_pos_]) == ParseFlags::NOT_SPECIAL)
                    data_pos_++;

                field_length_ = data_pos_ - (field_start_ + current_row_start());
            }

            /** Finish parsing the current field. */
            void push_field() {
                const RawCSVField& field = this->field_policy_.push_field(
                    *this->data_ptr_,
                    *this->fields_,
                    this->current_row_start(),
                    this->field_start_,
                    this->field_length_,
                    this->field_has_double_quote_
                );

                this->policy_.push_field(field);
                current_row_.row_length++;

                field_has_double_quote_ = false;
                field_start_ = UNINITIALIZED_FIELD;
                field_length_ = 0;
            }

            /** Finish parsing the current record and reset row-local state. */
            void finish_row(size_t raw_end) {
                if (this->field_length_ > 0
                    || this->field_start_ != UNINITIALIZED_FIELD
                    || !this->current_row_.empty()) {
                    this->push_field();
                }

                this->push_row(raw_end);
                this->current_row_ = this->row_policy_.make_next_row(
                    this->data_ptr_,
                    this->data_pos_,
                    this->fields_->size()
                );
                this->policy_.begin_row(this->current_row_);
            }

            /** Finish parsing the current row. */
            void push_row(size_t raw_end) {
                this->row_policy_.finalize_row(this->current_row_, *this->fields_, raw_end);
                this->policy_.end_row(this->current_row_);
                this->emit_row(std::move(current_row_));
            }

            /** Handle possible Unicode byte order mark. */
            void strip_unicode_bom() {
                auto& data = this->data_ptr_->data;

                if (!this->unicode_bom_scan_) {
                    this->data_pos_ += get_bom_skip_or_throw(data, this->utf8_bom_);
                    this->unicode_bom_scan_ = true;
                }
            }

            ParserChunkResult parse_prepared_chunk(
                csv::string_view chunk,
                std::shared_ptr<void> owner,
                const ParserChunkOptions& options
            ) {
                this->field_start_ = UNINITIALIZED_FIELD;
                this->field_length_ = 0;
                this->field_has_double_quote_ = false;
                this->reset_data_ptr();
                this->data_ptr_->_data = std::move(owner);
                this->data_ptr_->data = chunk;
                this->data_ptr_->source_start = options.source_start;
                this->data_ptr_->fields.reserve_for_source_size(chunk.size());
                this->data_ptr_->field_scalars.reserve_for_source_size(chunk.size());
                this->data_ptr_->quote_arena.reserve_for_source_size(chunk.size());
                this->initial_state_ = options.initial_state;
                this->scan_bom_for_current_chunk_ = options.scan_bom;
                this->current_row_ = this->row_policy_.make_initial_row(this->data_ptr_);
                this->policy_.begin_chunk(this->data_ptr_);
                this->policy_.begin_row(this->current_row_);

                const size_t complete_prefix_length = this->parse();
                return ParserChunkResult(options.initial_state, this->ending_state_, complete_prefix_length);
            }

            ParserDFAState current_dfa_state() const noexcept {
                return ParserDFAState{ this->quote_escape_, this->pending_quote_, this->pending_linefeed_ };
            }

            size_t finish_parse(size_t remainder) {
                this->ending_state_ = this->current_dfa_state();
                this->initial_state_ = this->ending_state_.pending_linefeed
                    ? this->ending_state_
                    : ParserDFAState{};
                this->scan_bom_for_current_chunk_ = true;
                return remainder;
            }
        };
    }
}


#include <cstddef>

namespace csv {
    namespace internals {
        namespace speculative {
        struct SpeculativeParseDiagnostics {
            size_t chunks = 0;
            size_t ambiguous_chunks = 0;
            size_t probability_model_chunks = 0;
            size_t record_size_heuristic_chunks = 0;
            size_t assumed_quoted_chunks = 0;
            size_t assumed_unquoted_chunks = 0;
            size_t validation_repairs = 0;

            void merge(const SpeculativeParseDiagnostics& other) noexcept {
                this->chunks += other.chunks;
                this->ambiguous_chunks += other.ambiguous_chunks;
                this->probability_model_chunks += other.probability_model_chunks;
                this->record_size_heuristic_chunks += other.record_size_heuristic_chunks;
                this->assumed_quoted_chunks += other.assumed_quoted_chunks;
                this->assumed_unquoted_chunks += other.assumed_unquoted_chunks;
                this->validation_repairs += other.validation_repairs;
            }
        };
        }

        using SpeculativeParseDiagnostics = speculative::SpeculativeParseDiagnostics;
    }
}


namespace csv {
    namespace internals {
        namespace parser {
        struct GuessScore {
            size_t header;
            size_t mode_row_length;
            double score;
        };

        CSV_INLINE GuessScore calculate_score(csv::string_view head, const CSVFormat& format);

        /** Guess the delimiter used by a delimiter-separated values file. */
        CSV_INLINE CSVGuessResult guess_format(
            csv::string_view head,
            const std::vector<char>& delims = { ',', '|', '\t', ';', '^', '~' }
        );

        /** Read the first 500KB from a seekless stream source. */
        template<typename TStream,
            csv::enable_if_t<std::is_base_of<std::istream, TStream>::value, int>  = 0>
        std::string get_csv_head_stream(TStream& source) {
            const size_t limit = 500000;
            std::string buf(limit, '\0');
            source.read(&buf[0], (std::streamsize)limit);
            buf.resize(static_cast<size_t>(source.gcount()));
            return buf;
        }

#if defined(__EMSCRIPTEN__)
        /** Open a file-backed source and read the first 500KB through the stream path. */
        CSV_INLINE std::string get_csv_head_stream(csv::string_view filename);
#endif

#if !defined(__EMSCRIPTEN__)
        /** Read the first 500KB from a filename using mmap.
         *  Also returns the total file size so callers avoid a second mmap open.
         */
        CSV_INLINE std::pair<std::string, size_t> get_csv_head_mmap(csv::string_view filename);
#endif

        /** Compatibility shim selecting stream on Emscripten and mmap otherwise. */
        CSV_INLINE std::string get_csv_head(csv::string_view filename);

        struct ResolvedFormat {
            CSVFormat format;
            size_t n_cols = 0;
        };

        class CSVParserDriverBase;
        }
    }

    /** @brief Guess the delimiter, header row, and mode column count of a CSV file
         *
         *  **Heuristic:** For each candidate delimiter, calculate a score based on
         *  the most common row length (mode). The delimiter with the highest score wins.
         *
         *  **Header Detection:**
         *  - If the first row has >= columns than the mode, it's treated as the header
         *  - Otherwise, the first row with the mode length is treated as the header
         *
         *  This approach handles:
         *  - Headers with trailing delimiters or optional columns (wider than data rows)
         *  - Comment lines before the actual header (first row shorter than mode)
         *  - Standard CSVs where first row is the header
         *
        *  @note Score = (row_length * count_of_rows_with_that_length)
        *  @note Also returns inferred mode-width column count (CSVGuessResult::n_cols)
         */
    inline CSVGuessResult guess_format(csv::string_view filename,
        const std::vector<char>& delims = { ',', '|', '\t', ';', '^', '~' }) {
        auto head = internals::parser::get_csv_head(filename);
        return internals::parser::guess_format(head, delims);
    }

    namespace internals {
        namespace parser {

        class ICSVParseOrchestrator {
        public:
            virtual ~ICSVParseOrchestrator() {}

            virtual size_t worker_count() const noexcept = 0;
            virtual SpeculativeParseDiagnostics diagnostics() const noexcept = 0;
            virtual size_t read_window_size(size_t chunk_size) const noexcept = 0;
            virtual bool utf8_bom() const noexcept = 0;
            virtual void reset_with_initial_state(ParserDFAState state) noexcept = 0;
            virtual ParserDFAState ending_state() const noexcept = 0;

            virtual CSVParseWindowResult parse_window(
                csv::string_view chunk,
                std::shared_ptr<void> owner,
                size_t base_offset,
                size_t serial_chunk_size,
                bool source_exhausted,
                RowCollection& output
            ) = 0;
        };

        inline std::unique_ptr<ICSVParseOrchestrator> make_csv_parse_orchestrator(
            const ParseFlagMap& parse_flags,
            const WhitespaceMap& ws_flags,
            const CSVFormat& format,
            size_t source_size,
            const ColNamesPtr& col_names = nullptr,
            bool enable_speculative_parallel = true,
            bool source_size_known = true
        );

        /** Abstract base class for source adapters.
         *
         *  It preserves the existing parser API while delegating byte-level
         *  parsing to CSVParserCore.
         */
        class CSVParserDriverBase : public CSVParserCore<> {
        public:
            CSVParserDriverBase() = default;
            CSVParserDriverBase(const CSVFormat&, const ColNamesPtr&);
            CSVParserDriverBase(
                const ParseFlagMap& parse_flags,
                const WhitespaceMap& ws_flags
            ) : CSVParserCore<>(parse_flags, ws_flags) {}

            virtual ~CSVParserDriverBase() {}

            /** Whether or not we have reached the end of source */
            bool eof() { return this->eof_; }

            ResolvedFormat get_resolved_format() { return this->format; }

            /** Parse the next block of data */
            virtual void next(size_t bytes) = 0;

            virtual SpeculativeParseDiagnostics speculative_diagnostics() const noexcept {
                return this->parse_orchestrator_
                    ? this->parse_orchestrator_->diagnostics()
                    : SpeculativeParseDiagnostics();
            }

            virtual size_t parse_worker_count() const noexcept {
                return this->parse_orchestrator_
                    ? this->parse_orchestrator_->worker_count()
                    : 1;
            }

            virtual bool utf8_bom() const noexcept {
                return this->parse_orchestrator_
                    ? this->parse_orchestrator_->utf8_bom()
                    : CSVParserCore<>::utf8_bom();
            }

        protected:
            /** @name Current Stream/File State */
            ///@{
            bool eof_ = false;

            ResolvedFormat format;

            /** The size of the incoming CSV */
            size_t source_size_ = 0;
            ///@}

            std::unique_ptr<ICSVParseOrchestrator> parse_orchestrator_;

            virtual std::string& get_csv_head() = 0;

            void resolve_format_from_head(const CSVFormat& format);
        };
        }
    }
}


#if !defined(__EMSCRIPTEN__)
namespace csv {
    namespace internals {
        namespace parser {
        /** Parser for memory-mapped files.
         *
         *  @par Implementation
         *  This class constructs moving windows over a file to avoid
         *  creating massive memory maps which may require more RAM
         *  than the user has available. It contains logic to automatically
         *  re-align each memory map to the beginning of a CSV row.
         *
         *  @par Head buffer
         *  CSVReader may prime a pre-read head buffer used for format guessing
         *  via prime_head_for_reuse(). When provided, the first next() call
         *  parses that buffer directly, then resumes mmap reads from the proper
         *  file offset while preserving chunk-boundary remainder semantics.
         */
        class MmapParser : public CSVParserDriverBase {
        public:
            MmapParser(
                csv::string_view filename,
                const CSVFormat& format,
                const ColNamesPtr& col_names = nullptr
            );

            ~MmapParser();

            std::string& get_csv_head() override {
                // head_ was already populated in the constructor.
                return this->head_;
            }

            void next(size_t bytes) override;

        private:
            void finalize_loaded_chunk(
                csv::string_view chunk,
                std::shared_ptr<void> owner,
                size_t length,
                size_t chunk_size
            );

            size_t read_window_size(size_t chunk_size) const noexcept;

            std::string _filename;
            size_t mmap_pos = 0;
            std::string head_;
        };
        }
    }
}
#endif

/** @file
 *  @brief Runtime read scheduling for CSVReader.
 */


#include <exception>
#include <functional>
#include <utility>


#ifdef CSV_HAS_CXX20
#include <concepts>
#endif

#if CSV_ENABLE_THREADS
#include <mutex>
#include <thread>
#endif

namespace csv {
    namespace internals {
        /** Synchronous read scheduler used by no-thread builds and runtime opt-out.
         *
         *  It mirrors the threaded scheduler's tiny surface so CSVReader can keep
         *  worker-launch and exception-transfer details out of its public facade
         *  logic.
         */
        class SynchronousCSVReadScheduler {
        public:
            SynchronousCSVReadScheduler() noexcept = default;

            explicit SynchronousCSVReadScheduler(bool enabled) noexcept {
                (void)enabled;
            }

            void set_threading_enabled(bool enabled) noexcept {
                (void)enabled;
            }

            void run(std::function<void()> task) {
                task();
            }

            void run(
                std::function<void()> task,
                const std::function<void()>& before_async_run
            ) {
                (void)before_async_run;
                this->run(std::move(task));
            }

            void join() noexcept {}

            bool wait_if_active(
                const std::function<bool()>& is_waitable,
                const std::function<void()>& wait
            ) {
                (void)is_waitable;
                (void)wait;
                return false;
            }

            void clear_exception() noexcept {}

            std::exception_ptr take_exception() noexcept {
                return nullptr;
            }

            void adopt_exception(std::exception_ptr eptr) noexcept {
                (void)eptr;
            }

            void rethrow_exception_if_any() {}
        };

#if CSV_ENABLE_THREADS
        /** Thread-backed scheduler selected when CSV_ENABLE_THREADS is available.
         *
         *  This concrete scheduler always launches a worker thread. Runtime
         *  selection between this and SynchronousCSVReadScheduler is handled by
         *  CSVReadScheduler below so the hot worker path has no "should I thread?"
         *  branch.
         */
        class ThreadedCSVReadScheduler {
        public:
            ThreadedCSVReadScheduler() noexcept = default;

            ThreadedCSVReadScheduler(const ThreadedCSVReadScheduler&) = delete;
            ThreadedCSVReadScheduler& operator=(const ThreadedCSVReadScheduler&) = delete;

            ~ThreadedCSVReadScheduler() {
                this->join();
            }

            void run(
                std::function<void()> task,
                const std::function<void()>& before_async_run = std::function<void()>()
            ) {
                this->join();
                this->clear_exception();

                if (before_async_run) {
                    before_async_run();
                }

                this->worker_ = std::thread([this, task]() mutable {
                    this->run_now(std::move(task));
                });
            }

            void join() noexcept {
                if (this->worker_.joinable()) {
                    this->worker_.join();
                }
            }

            bool wait_if_active(
                const std::function<bool()>& is_waitable,
                const std::function<void()>& wait
            ) {
                if (is_waitable()) {
                    wait();
                    return true;
                }

                return false;
            }

            void clear_exception() noexcept {
                std::lock_guard<std::mutex> lock(this->exception_lock_);
                this->exception_ = nullptr;
            }

            std::exception_ptr take_exception() noexcept {
                std::lock_guard<std::mutex> lock(this->exception_lock_);
                auto eptr = this->exception_;
                this->exception_ = nullptr;
                return eptr;
            }

            void adopt_exception(std::exception_ptr eptr) noexcept {
                std::lock_guard<std::mutex> lock(this->exception_lock_);
                this->exception_ = std::move(eptr);
            }

            void rethrow_exception_if_any() {
                if (auto eptr = this->take_exception()) {
                    std::rethrow_exception(eptr);
                }
            }

        private:
            std::thread worker_;
            std::exception_ptr exception_ = nullptr;
            std::mutex exception_lock_;

            void run_now(std::function<void()> task) noexcept {
                try {
                    task();
                }
                catch (...) {
                    this->adopt_exception(std::current_exception());
                }
            }
        };

        /** Runtime scheduler selector used by CSVReader.
         *
         *  Keeps runtime threading policy at the boundary without virtual
         *  dispatch. The selected concrete scheduler is stored as a void* and
         *  identified by comparing it to the owned threaded scheduler.
         */
        class CSVReadScheduler {
        public:
            explicit CSVReadScheduler(bool threading_enabled = true) noexcept {
                this->select(threading_enabled);
            }

            CSVReadScheduler(const CSVReadScheduler&) = delete;
            CSVReadScheduler& operator=(const CSVReadScheduler&) = delete;

            ~CSVReadScheduler() {
                this->join();
            }

            void set_threading_enabled(bool enabled) noexcept {
                this->join();
                auto eptr = this->take_exception();
                this->select(enabled);
                this->adopt_exception(std::move(eptr));
            }

            void run(
                std::function<void()> task,
                const std::function<void()>& before_async_run = std::function<void()>()
            ) {
                if (this->is_threaded()) {
                    this->threaded_.run(std::move(task), before_async_run);
                }
                else {
                    this->sync_.run(std::move(task));
                }
            }

            bool wait_if_active(
                const std::function<bool()>& is_waitable,
                const std::function<void()>& wait
            ) {
                return this->is_threaded()
                    && this->threaded_.wait_if_active(is_waitable, wait);
            }

            void join() noexcept {
                if (this->is_threaded()) {
                    this->threaded_.join();
                }
            }

            void clear_exception() noexcept {
                if (this->is_threaded()) {
                    this->threaded_.clear_exception();
                }
                else {
                    this->sync_.clear_exception();
                }
            }

            std::exception_ptr take_exception() noexcept {
                return this->is_threaded()
                    ? this->threaded_.take_exception()
                    : this->sync_.take_exception();
            }

            void adopt_exception(std::exception_ptr eptr) noexcept {
                if (this->is_threaded()) {
                    this->threaded_.adopt_exception(std::move(eptr));
                }
                else {
                    this->sync_.adopt_exception(std::move(eptr));
                }
            }

            void rethrow_exception_if_any() {
                if (auto eptr = this->take_exception()) {
                    std::rethrow_exception(eptr);
                }
            }

        private:
            SynchronousCSVReadScheduler sync_;
            ThreadedCSVReadScheduler threaded_;
            void* scheduler_ = &sync_;

            void select(bool threading_enabled) noexcept {
                if (threading_enabled) {
                    this->scheduler_ = &this->threaded_;
                }
                else {
                    this->scheduler_ = &this->sync_;
                }
            }

            bool is_threaded() const noexcept {
                return this->scheduler_ == &this->threaded_;
            }
        };
#else
        using CSVReadScheduler = SynchronousCSVReadScheduler;
#endif

#ifdef CSV_HAS_CXX20
        template<typename Scheduler>
        concept CSVConcreteReadSchedulerLike = requires(
            Scheduler scheduler,
            std::function<void()> task,
            std::function<void()> before_async_run,
            std::function<bool()> is_waitable,
            std::exception_ptr eptr
        ) {
            { scheduler.run(task) } -> std::same_as<void>;
            { scheduler.run(task, before_async_run) } -> std::same_as<void>;
            { scheduler.wait_if_active(is_waitable, before_async_run) } -> std::same_as<bool>;
            { scheduler.join() } -> std::same_as<void>;
            { scheduler.clear_exception() } -> std::same_as<void>;
            { scheduler.take_exception() } -> std::same_as<std::exception_ptr>;
            { scheduler.adopt_exception(eptr) } -> std::same_as<void>;
            { scheduler.rethrow_exception_if_any() } -> std::same_as<void>;
        };

        template<typename Scheduler>
        concept CSVReadSchedulerLike = CSVConcreteReadSchedulerLike<Scheduler>
            && requires(Scheduler scheduler, bool enabled) {
                { scheduler.set_threading_enabled(enabled) } -> std::same_as<void>;
            };

        static_assert(
            CSVConcreteReadSchedulerLike<SynchronousCSVReadScheduler>,
            "SynchronousCSVReadScheduler must satisfy the concrete read scheduler contract."
        );
#if CSV_ENABLE_THREADS
        static_assert(
            CSVConcreteReadSchedulerLike<ThreadedCSVReadScheduler>,
            "ThreadedCSVReadScheduler must satisfy the concrete read scheduler contract."
        );
#endif
        static_assert(
            CSVReadSchedulerLike<CSVReadScheduler>,
            "CSVReadScheduler must satisfy the reader-facing scheduler contract."
        );
#endif
    }
}






#include <atomic>
#include <exception>
#include <functional>
#include <utility>

#if CSV_ENABLE_THREADS
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#endif

namespace csv {
    namespace internals {
        namespace parallel {

        class IndexedTaskPool {
        public:
            explicit IndexedTaskPool(size_t worker_count)
#if CSV_ENABLE_THREADS
                : worker_count_(worker_count)
#endif
            {
                this->start_workers(worker_count);
            }

            IndexedTaskPool(const IndexedTaskPool&) = delete;
            IndexedTaskPool& operator=(const IndexedTaskPool&) = delete;

            ~IndexedTaskPool() {
                this->stop_workers();
            }

            size_t worker_count() const noexcept {
#if CSV_ENABLE_THREADS
                return this->worker_count_;
#else
                return 0;
#endif
            }

            template<typename Fn>
            void parallel_for(size_t task_count, Fn&& fn) {
                if (task_count == 0) {
                    return;
                }

#if CSV_ENABLE_THREADS
                if (this->workers_.empty()) {
                    this->run_serial(task_count, std::forward<Fn>(fn));
                    return;
                }

                std::exception_ptr captured_exception;
                {
                    std::unique_lock<std::mutex> lock(this->mutex_);
                    this->current_task_ = std::forward<Fn>(fn);
                    this->task_exception_ = nullptr;
                    this->next_task_.store(0);
                    this->task_count_ = task_count;
                    this->active_workers_ = this->workers_.size();
                    ++this->generation_;
                }

                this->task_ready_.notify_all();

                {
                    std::unique_lock<std::mutex> lock(this->mutex_);
                    this->task_done_.wait(lock, [this]() {
                        return this->completed_generation_ == this->generation_;
                    });

                    captured_exception = this->task_exception_;
                    this->current_task_ = std::function<void(size_t, size_t)>();
                    this->task_exception_ = nullptr;
                }

                if (captured_exception) {
                    std::rethrow_exception(captured_exception);
                }
#else
                this->run_serial(task_count, std::forward<Fn>(fn));
#endif
            }

        private:
            template<typename Fn>
            void run_serial(size_t task_count, Fn&& fn) {
                for (size_t task_index = 0; task_index < task_count; ++task_index) {
                    fn(0, task_index);
                }
            }

#if CSV_ENABLE_THREADS
            size_t worker_count_ = 0;

            void start_workers(size_t worker_count) {
                if (worker_count <= 1) {
                    return;
                }

                this->workers_.reserve(worker_count);
                for (size_t worker_index = 0; worker_index < worker_count; ++worker_index) {
                    this->workers_.push_back(std::thread(&IndexedTaskPool::worker_loop, this, worker_index));
                }
            }

            void stop_workers() {
                {
                    std::lock_guard<std::mutex> lock(this->mutex_);
                    this->stop_ = true;
                    ++this->generation_;
                }

                this->task_ready_.notify_all();
                for (size_t i = 0; i < this->workers_.size(); ++i) {
                    if (this->workers_[i].joinable()) {
                        this->workers_[i].join();
                    }
                }
            }

            void worker_loop(size_t worker_index) {
                size_t seen_generation = 0;

                for (;;) {
                    {
                        std::unique_lock<std::mutex> lock(this->mutex_);
                        this->task_ready_.wait(lock, [this, &seen_generation]() {
                            return this->stop_ || this->generation_ != seen_generation;
                        });

                        if (this->stop_) {
                            return;
                        }

                        seen_generation = this->generation_;
                    }

                    for (;;) {
                        const size_t task_index = this->next_task_.fetch_add(1);
                        if (task_index >= this->task_count_) {
                            break;
                        }

                        try {
                            this->current_task_(worker_index, task_index);
                        }
                        catch (...) {
                            std::lock_guard<std::mutex> lock(this->mutex_);
                            if (!this->task_exception_) {
                                this->task_exception_ = std::current_exception();
                                this->next_task_.store(this->task_count_);
                            }
                            break;
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(this->mutex_);
                        if (--this->active_workers_ == 0) {
                            this->completed_generation_ = seen_generation;
                            this->task_done_.notify_one();
                        }
                    }
                }
            }

            std::vector<std::thread> workers_;
            std::mutex mutex_;
            std::condition_variable task_ready_;
            std::condition_variable task_done_;
            std::function<void(size_t, size_t)> current_task_;
            std::exception_ptr task_exception_ = nullptr;
            std::atomic<size_t> next_task_{0};
            size_t task_count_ = 0;
            size_t active_workers_ = 0;
            size_t generation_ = 0;
            size_t completed_generation_ = 0;
            bool stop_ = false;
#else
            void start_workers(size_t) {}
            void stop_workers() {}
#endif
        };

        }
    }
}



#include <cmath>
#include <limits>

#if CSV_ENABLE_THREADS

namespace csv {
    namespace internals {
        namespace speculative {
        constexpr size_t CSV_SPECULATIVE_PREFIX_SIZE = 64 * 1024;

        struct PrefixScanResult {
            ParserDFAState ending_state;
            size_t records_seen = 0;
            size_t first_record_end = (std::numeric_limits<size_t>::max)();
            size_t total_states = 0;
            size_t unquoted_states = 0;
            size_t max_record_length = 0;
            size_t quoted_fields = 0;
            size_t first_quote_open = (std::numeric_limits<size_t>::max)();
            size_t first_quote_close = (std::numeric_limits<size_t>::max)();
            long double log_other_start_valid_probability = 0;
        };

        struct ChunkSpeculation {
            size_t sequence_number = 0;
            size_t offset = 0;
            size_t length = 0;
            size_t prefix_length = 0;
            ParserDFAState assumed_start_state;
            PrefixScanResult outside_scan;
            PrefixScanResult inside_scan;
            bool ambiguous = false;
            bool used_probability_model = false;
            bool used_record_size_heuristic = false;
            long double quoted_start_odds = 0;
        };

        inline void observe_speculation(
            SpeculativeParseDiagnostics& diagnostics,
            const ChunkSpeculation& speculation
        ) noexcept {
            diagnostics.chunks++;
            if (speculation.ambiguous) {
                diagnostics.ambiguous_chunks++;
            }
            if (speculation.used_probability_model) {
                diagnostics.probability_model_chunks++;
            }
            if (speculation.used_record_size_heuristic) {
                diagnostics.record_size_heuristic_chunks++;
            }
            if (speculation.assumed_start_state.quote_escape) {
                diagnostics.assumed_quoted_chunks++;
            }
            else {
                diagnostics.assumed_unquoted_chunks++;
            }
        }

        class SpeculativeScanner {
        public:
            explicit SpeculativeScanner(
                const ParseFlagMap& parse_flags,
                size_t prefix_bytes = CSV_SPECULATIVE_PREFIX_SIZE
            ) : parse_flags_(parse_flags), prefix_bytes_(prefix_bytes) {}

            ChunkSpeculation speculate(
                size_t sequence_number,
                size_t offset,
                csv::string_view chunk
            ) const {
                const size_t prefix_length = std::min(chunk.size(), this->prefix_bytes_);
                const csv::string_view prefix(chunk.data(), prefix_length);

                ChunkSpeculation result;
                result.sequence_number = sequence_number;
                result.offset = offset;
                result.length = chunk.size();
                result.prefix_length = prefix_length;
                this->scan_prefix(prefix, result.outside_scan, result.inside_scan);
                result.ambiguous = this->is_ambiguous(result.outside_scan, result.inside_scan);
                result.assumed_start_state = this->choose_start_state(result);
                return result;
            }

        private:
            CONSTEXPR_17 ParseFlags parse_flag(const char ch) const noexcept {
                return parse_flags_.data()[ch + CHAR_OFFSET];
            }

            static constexpr size_t missing_index() noexcept {
                return (std::numeric_limits<size_t>::max)();
            }

            struct PrefixCounterState {
                PrefixScanResult result;
                size_t record_start = 0;
                size_t quoted_start = missing_index();
                bool quoted_field_partial = false;
                long double separator_weight = 0;
            };

            void observe_quoted_span(
                PrefixCounterState& state,
                size_t field_length
            ) const noexcept {
                state.result.quoted_fields++;

                if (field_length == 0) {
                    return;
                }

                if (state.quoted_field_partial || field_length == 1) {
                    state.separator_weight += 1;
                }
                else {
                    state.separator_weight += 2;
                }
            }

            void finish_record(PrefixCounterState& state, size_t record_end) const noexcept {
                state.result.records_seen++;
                const size_t record_length = record_end - state.record_start;
                if (record_length > state.result.max_record_length) {
                    state.result.max_record_length = record_length;
                }
                state.record_start = record_end;
            }

            bool is_separator_or_record_end(ParseFlags flag) const noexcept {
                return flag == ParseFlags::DELIMITER
                    || flag == ParseFlags::CARRIAGE_RETURN
                    || flag == ParseFlags::NEWLINE;
            }

            bool quote_can_open(csv::string_view prefix, size_t pos) const noexcept {
                if (pos == 0) {
                    return true;
                }

                return this->is_separator_or_record_end(this->parse_flag(prefix[pos - 1]));
            }

            bool quote_can_close(csv::string_view prefix, size_t pos) const noexcept {
                if (pos + 1 >= prefix.size()) {
                    return true;
                }

                return this->is_separator_or_record_end(this->parse_flag(prefix[pos + 1]));
            }

            void start_quoted_span(PrefixCounterState& state, size_t pos, bool partial) const noexcept {
                state.quoted_start = pos;
                state.quoted_field_partial = partial;
            }

            void finish_quoted_span(PrefixCounterState& state, size_t pos) const noexcept {
                if (state.quoted_start == this->missing_index()) {
                    return;
                }

                this->observe_quoted_span(state, pos - state.quoted_start);
                state.quoted_start = this->missing_index();
                state.quoted_field_partial = false;
            }

            void finish_partial_quoted_span(PrefixCounterState& state, size_t prefix_size) const noexcept {
                if (state.quoted_start == this->missing_index()) {
                    return;
                }

                this->observe_quoted_span(state, prefix_size - state.quoted_start);
                state.quoted_start = this->missing_index();
                state.quoted_field_partial = false;
            }

            void finish_scan(
                PrefixCounterState& state,
                csv::string_view prefix,
                bool ending_state,
                long double log_separator_probability
            ) const noexcept {
                const size_t tail_length = prefix.size() - state.record_start;
                if (tail_length > state.result.max_record_length) {
                    state.result.max_record_length = tail_length;
                }
                state.result.ending_state = ParserDFAState(ending_state);
                state.result.log_other_start_valid_probability = state.separator_weight == 0
                    ? 0
                    : state.separator_weight * log_separator_probability;
            }

            // The speculative pass should be much cheaper than parsing. This
            // is a quote-parity scan: q-o / o-q pattern evidence usually
            // decides the starting state, and the probability model is left
            // for the rare ambiguous prefix.
            void scan_prefix(
                csv::string_view prefix,
                PrefixScanResult& outside_scan,
                PrefixScanResult& inside_scan
            ) const {
                using internals::ParseFlags;

                PrefixCounterState outside;
                PrefixCounterState inside;
                this->start_quoted_span(inside, 0, true);

                bool odd_quotes = false;
                size_t separators = 0;

                for (size_t pos = 0; pos < prefix.size();) {
                    const ParseFlags flag = this->parse_flag(prefix[pos]);
                    size_t width = 1;
                    if (flag == ParseFlags::CARRIAGE_RETURN
                        && pos + 1 < prefix.size()
                        && this->parse_flag(prefix[pos + 1]) == ParseFlags::NEWLINE) {
                        width = 2;
                    }

                    outside.result.total_states += width;
                    inside.result.total_states += width;
                    if (!odd_quotes) {
                        outside.result.unquoted_states += width;
                    }
                    else {
                        inside.result.unquoted_states += width;
                    }

                    if (flag == ParseFlags::DELIMITER || flag == ParseFlags::CARRIAGE_RETURN || flag == ParseFlags::NEWLINE) {
                        separators++;
                    }

                    if (flag == ParseFlags::QUOTE) {
                        if (outside.result.first_quote_open == this->missing_index()
                            && this->quote_can_open(prefix, pos)) {
                            outside.result.first_quote_open = pos;
                        }
                        if (inside.result.first_quote_close == this->missing_index()
                            && this->quote_can_close(prefix, pos)) {
                            inside.result.first_quote_close = pos;
                        }

                        if (odd_quotes) {
                            this->finish_quoted_span(outside, pos);
                            this->start_quoted_span(inside, pos + 1, false);
                        }
                        else {
                            this->start_quoted_span(outside, pos + 1, false);
                            this->finish_quoted_span(inside, pos);
                        }

                        odd_quotes = !odd_quotes;
                        pos++;
                        continue;
                    }

                    if (flag == ParseFlags::CARRIAGE_RETURN || flag == ParseFlags::NEWLINE) {
                        const size_t record_end = pos + width;
                        if (!odd_quotes) {
                            this->finish_record(outside, record_end);
                            if (outside.result.first_record_end == this->missing_index()) {
                                outside.result.first_record_end = record_end;
                            }
                        }
                        else {
                            this->finish_record(inside, record_end);
                            if (inside.result.first_record_end == this->missing_index()) {
                                inside.result.first_record_end = record_end;
                            }
                        }
                    }

                    pos += width;
                }

                const long double separator_probability = prefix.empty()
                    ? 0
                    : static_cast<long double>(separators) / static_cast<long double>(prefix.size());
                const long double log_separator_probability = separator_probability > 0
                    ? std::log(separator_probability)
                    : -std::numeric_limits<long double>::infinity();

                if (odd_quotes) {
                    this->finish_partial_quoted_span(outside, prefix.size());
                }
                else {
                    this->finish_partial_quoted_span(inside, prefix.size());
                }

                this->finish_scan(outside, prefix, odd_quotes, log_separator_probability);
                this->finish_scan(inside, prefix, !odd_quotes, log_separator_probability);
                outside_scan = outside.result;
                inside_scan = inside.result;
            }

            long double unquoted_ratio(const PrefixScanResult& scan) const noexcept {
                if (scan.total_states == 0) {
                    return scan.ending_state.quote_escape ? 0 : 1;
                }

                return static_cast<long double>(scan.unquoted_states)
                    / static_cast<long double>(scan.total_states);
            }

            long double quoted_odds_from_probability_model(
                const PrefixScanResult& outside_scan,
                const PrefixScanResult& inside_scan
            ) const noexcept {
                const long double log_k = inside_scan.log_other_start_valid_probability
                    - outside_scan.log_other_start_valid_probability;

                if (log_k > 64) {
                    return std::numeric_limits<long double>::infinity();
                }
                if (log_k < -64) {
                    return 0;
                }

                const long double k = std::exp(log_k);
                const long double u_u = this->unquoted_ratio(outside_scan);
                const long double u_q = this->unquoted_ratio(inside_scan);
                const long double a = u_q;
                const long double b = u_u - k * (1 - u_q);
                const long double c = -k * (1 - u_u);
                const long double epsilon = 1e-18L;

                if (std::fabs(a) < epsilon) {
                    if (std::fabs(b) < epsilon) {
                        return k > 1 ? std::numeric_limits<long double>::infinity() : 0;
                    }

                    const long double linear_root = -c / b;
                    return linear_root > 0 ? linear_root : 0;
                }

                const long double discriminant = b * b - 4 * a * c;
                if (discriminant < 0) {
                    return 1;
                }

                const long double root = (-b + std::sqrt(discriminant)) / (2 * a);
                return root > 0 ? root : 0;
            }

            int pattern_start_state(
                const PrefixScanResult& outside_scan,
                const PrefixScanResult& inside_scan
            ) const noexcept {
                const size_t outside_open = outside_scan.first_quote_open;
                const size_t inside_close = inside_scan.first_quote_close;
                const size_t missing = (std::numeric_limits<size_t>::max)();

                if (outside_open == missing && inside_close == missing) {
                    return -1;
                }

                if (outside_open < inside_close) {
                    return 0;
                }

                if (inside_close < outside_open) {
                    return 1;
                }

                return -1;
            }

            ParserDFAState choose_start_state(ChunkSpeculation& speculation) const noexcept {
                const PrefixScanResult& outside_scan = speculation.outside_scan;
                const PrefixScanResult& inside_scan = speculation.inside_scan;
                const int pattern_state = this->pattern_start_state(outside_scan, inside_scan);

                if (pattern_state == 0) {
                    return ParserDFAState(false);
                }

                if (pattern_state == 1) {
                    return ParserDFAState(true);
                }

                if (outside_scan.records_seen > 0 && inside_scan.records_seen == 0) {
                    return ParserDFAState(false);
                }

                if (inside_scan.records_seen > 0 && outside_scan.records_seen == 0) {
                    return ParserDFAState(true);
                }

                if (speculation.ambiguous) {
                    speculation.quoted_start_odds = this->quoted_odds_from_probability_model(
                        outside_scan,
                        inside_scan
                    );

                    if (speculation.quoted_start_odds > 1.000001L) {
                        speculation.used_probability_model = true;
                        return ParserDFAState(true);
                    }

                    if (speculation.quoted_start_odds < 0.999999L) {
                        speculation.used_probability_model = true;
                        return ParserDFAState(false);
                    }

                    speculation.used_record_size_heuristic = true;
                    return inside_scan.max_record_length < outside_scan.max_record_length
                        ? ParserDFAState(true)
                        : ParserDFAState(false);
                }

                return ParserDFAState(false);
            }

            bool is_ambiguous(
                const PrefixScanResult& outside_scan,
                const PrefixScanResult& inside_scan
            ) const noexcept {
                if (this->pattern_start_state(outside_scan, inside_scan) != -1) {
                    return false;
                }

                return (outside_scan.records_seen == inside_scan.records_seen)
                    || (outside_scan.records_seen > 0 && inside_scan.records_seen > 0);
            }

            ParseFlagMap parse_flags_;
            size_t prefix_bytes_;
        };
        }
    }
}

#endif




namespace csv {
    namespace internals {
        namespace speculative {
        struct CSVRowFragment {
            CSVRowFragment() = default;
            CSVRowFragment(
                csv::string_view bytes,
                std::shared_ptr<void> owner,
                ParserDFAState initial_state = ParserDFAState(),
                ParserDFAState ending_state = ParserDFAState(),
                size_t offset = 0
            ) : bytes(bytes),
                owner(std::move(owner)),
                initial_state(initial_state),
                ending_state(ending_state),
                offset(offset),
                present(true) {}

            bool empty() const noexcept {
                return !present;
            }

            static CSVRowFragment from_row(
                const CSVRow& row,
                ParserDFAState initial_state = ParserDFAState(),
                ParserDFAState ending_state = ParserDFAState(),
                size_t chunk_offset = 0
            ) {
                return CSVRowFragment(
                    row.raw_str(),
                    row.data,
                    initial_state,
                    ending_state,
                    chunk_offset + row.data_start
                );
            }

            csv::string_view bytes;
            std::shared_ptr<void> owner;
            ParserDFAState initial_state;
            ParserDFAState ending_state;
            size_t offset = 0;
            bool present = false;
        };

        inline CSVRowFragment concatenate_row_fragments(
            const CSVRowFragment& left,
            const CSVRowFragment& right
        ) {
            if (left.empty()) {
                return right;
            }

            if (right.empty()) {
                return left;
            }

            auto bytes = std::make_shared<std::string>();
            bytes->reserve(left.bytes.size() + right.bytes.size());
            if (!left.bytes.empty()) {
                bytes->append(left.bytes.data(), left.bytes.size());
            }
            if (!right.bytes.empty()) {
                bytes->append(right.bytes.data(), right.bytes.size());
            }

            return CSVRowFragment(
                csv::string_view(*bytes),
                bytes,
                left.initial_state,
                right.ending_state,
                left.offset
            );
        }

        struct ParsedChunkRows {
            size_t sequence_number = 0;
            size_t offset = 0;
            csv::string_view chunk;
            std::shared_ptr<void> owner;
            bool starts_at_record_boundary = true;
            bool scan_bom = true;
            ParserChunkResult parse_result;
            CSVRowFragment prefix_fragment;
            std::vector<CSVRow> complete_rows;
            CSVRowFragment suffix_fragment;
        };

        inline ParsedChunkRows split_parsed_chunk_rows(
            size_t sequence_number,
            csv::string_view chunk,
            std::shared_ptr<void> owner,
            const ParserChunkResult& parse_result,
            std::vector<CSVRow> parsed_rows,
            bool starts_at_record_boundary,
            size_t chunk_offset = 0
        ) {
            ParsedChunkRows result;
            result.sequence_number = sequence_number;
            result.offset = chunk_offset;
            result.chunk = chunk;
            result.owner = owner;
            result.starts_at_record_boundary = starts_at_record_boundary
                || parse_result.initial_state.pending_linefeed;
            result.scan_bom = starts_at_record_boundary && sequence_number == 0;
            result.parse_result = parse_result;

            size_t first_complete_row = 0;
            if (!result.starts_at_record_boundary) {
                if (parsed_rows.empty()) {
                    result.prefix_fragment = CSVRowFragment(
                        chunk,
                        owner,
                        parse_result.initial_state,
                        parse_result.ending_state,
                        chunk_offset
                    );
                    return result;
                }

                result.prefix_fragment = CSVRowFragment::from_row(
                    parsed_rows.front(),
                    parse_result.initial_state,
                    ParserDFAState(),
                    chunk_offset
                );
                first_complete_row = 1;
            }

            result.complete_rows.reserve(parsed_rows.size() - first_complete_row);
            for (size_t i = first_complete_row; i < parsed_rows.size(); ++i) {
                result.complete_rows.push_back(std::move(parsed_rows[i]));
            }

            if (parse_result.complete_prefix_length < chunk.size()) {
                result.suffix_fragment = CSVRowFragment(
                    chunk.substr(parse_result.complete_prefix_length),
                    owner,
                    ParserDFAState(),
                    parse_result.ending_state,
                    chunk_offset + parse_result.complete_prefix_length
                );
            }

            return result;
        }

        template<typename Parser>
        inline std::vector<CSVRow> materialize_row_fragment(
            Parser& parser,
            const CSVRowFragment& fragment
        ) {
            std::vector<CSVRow> rows;
            if (fragment.empty()) {
                return rows;
            }

            parser.parse_chunk(
                fragment.bytes,
                fragment.owner,
                rows,
                ParserChunkOptions(ParserDFAState(), false, fragment.offset)
            );
            parser.end_feed();
            return rows;
        }

        template<typename Parser>
        inline ParsedChunkRows repair_parsed_chunk_rows(
            Parser& parser,
            const ParsedChunkRows& chunk,
            ParserDFAState corrected_initial_state
        ) {
            std::vector<CSVRow> parsed_rows;

            const ParserChunkResult parse_result = parser.parse_chunk(
                chunk.chunk,
                chunk.owner,
                parsed_rows,
                ParserChunkOptions(corrected_initial_state, chunk.scan_bom, chunk.offset)
            );

            return split_parsed_chunk_rows(
                chunk.sequence_number,
                chunk.chunk,
                chunk.owner,
                parse_result,
                std::move(parsed_rows),
                chunk.starts_at_record_boundary,
                chunk.offset
            );
        }

        /** Minimal parser shell for caller-owned chunks.
         *
         *  The SIGMOD-style speculative path treats input sourcing as an
         *  external concern. This parser core only needs delimiter/whitespace state.
         */
        template<bool EagerClassify = false>
        class ChunkParserCoreT : public CSVParserCore<
            std::vector<CSVRow>,
            PermissiveParsePolicy,
            CSVRowFieldPolicy<EagerClassify>,
            CSVRowRowPolicy> {
            using Base = CSVParserCore<
                std::vector<CSVRow>,
                PermissiveParsePolicy,
                CSVRowFieldPolicy<EagerClassify>,
                CSVRowRowPolicy>;

        public:
            ChunkParserCoreT(
                const ParseFlagMap& parse_flags,
                const WhitespaceMap& ws_flags
            ) : Base(parse_flags, ws_flags) {}

            ChunkParserCoreT(
                const ParseFlagMap& parse_flags,
                const WhitespaceMap& ws_flags,
                const ColNamesPtr& col_names
            ) : Base(parse_flags, ws_flags, col_names) {}
        };

        using ChunkParserCore = ChunkParserCoreT<false>;
        }
    }
}


#if CSV_ENABLE_THREADS

namespace csv {
    namespace internals {
        namespace speculative {
        template<typename RowSink, typename Parser = CSVParserCore<std::vector<CSVRow>>>
        class SpeculativeParseValidator {
        public:
            SpeculativeParseValidator(
                Parser& repair_parser,
                RowSink& output,
                ParserDFAState initial_state = ParserDFAState()
            ) : repair_parser_(repair_parser),
                output_(output),
                expected_start_state_(initial_state) {}

            ParserChunkResult validate_and_release(ParsedChunkRows&& chunk) {
                if (!parser_dfa_state_equal(chunk.parse_result.initial_state, this->expected_start_state_)) {
                    chunk = repair_parsed_chunk_rows(
                        this->repair_parser_,
                        chunk,
                        this->expected_start_state_
                    );
                    this->repair_count_++;
                }

                const ParserChunkResult parse_result = chunk.parse_result;
                this->release(std::move(chunk));
                return parse_result;
            }

            void finish(bool flush_pending = true) {
                if (flush_pending) {
                    this->release_pending_suffix();
                }
            }

            size_t repair_count() const noexcept {
                return this->repair_count_;
            }

            ParserDFAState expected_start_state() const noexcept {
                return this->expected_start_state_;
            }

            bool has_pending_suffix() const noexcept {
                return !this->pending_suffix_.empty();
            }

            const CSVRowFragment& pending_suffix() const noexcept {
                return this->pending_suffix_;
            }

        private:
            void release(ParsedChunkRows&& chunk) {
                if (!chunk.prefix_fragment.empty()) {
                    if (!this->pending_suffix_.empty()) {
                        this->pending_suffix_ = concatenate_row_fragments(
                            this->pending_suffix_,
                            chunk.prefix_fragment
                        );
                    }
                    else {
                        this->pending_suffix_ = chunk.prefix_fragment;
                    }

                    if (chunk.parse_result.complete_prefix_length == 0) {
                        this->expected_start_state_ = chunk.parse_result.ending_state;
                        return;
                    }

                    this->release_pending_suffix();
                }

                csv_append_rows(this->output_, std::move(chunk.complete_rows));

                this->pending_suffix_ = chunk.suffix_fragment;
                this->expected_start_state_ = chunk.parse_result.ending_state;
            }

            void release_pending_suffix() {
                if (this->pending_suffix_.empty()) {
                    return;
                }

                auto rows = materialize_row_fragment(this->repair_parser_, this->pending_suffix_);
                csv_append_rows(this->output_, std::move(rows));

                this->pending_suffix_ = CSVRowFragment();
            }

            Parser& repair_parser_;
            RowSink& output_;
            ParserDFAState expected_start_state_;
            CSVRowFragment pending_suffix_;
            size_t repair_count_ = 0;
        };
        }
    }
}

#endif


#if CSV_ENABLE_THREADS

namespace csv {
    namespace internals {
        namespace speculative {
        struct SpeculativeParseChunk {
            size_t sequence_number = 0;
            size_t offset = 0;
            csv::string_view bytes;
            std::shared_ptr<void> owner;
            ChunkSpeculation speculation;
            bool starts_at_record_boundary = false;
            bool scan_bom = false;
        };

        struct ParallelCSVParseResult {
            size_t chunks_processed = 0;
            size_t repair_count = 0;
            size_t complete_prefix_length = 0;
            bool has_pending_suffix = false;
            ParserDFAState ending_state;
            SpeculativeParseDiagnostics diagnostics;
        };

        inline std::vector<SpeculativeParseChunk> make_speculative_parse_chunks(
            csv::string_view data,
            std::shared_ptr<void> owner,
            size_t chunk_size,
            const SpeculativeScanner& scanner,
            size_t base_offset = 0,
            size_t first_sequence_number = 0,
            bool scan_bom_for_first_chunk = true
        ) {
            std::vector<SpeculativeParseChunk> chunks;
            if (chunk_size == 0) {
                return chunks;
            }

            size_t sequence_number = first_sequence_number;
            for (size_t offset = 0; offset < data.size(); offset += chunk_size) {
                const size_t length = std::min(chunk_size, data.size() - offset);
                const csv::string_view bytes(data.data() + offset, length);
                const bool first_chunk = offset == 0;

                SpeculativeParseChunk chunk;
                chunk.sequence_number = sequence_number;
                chunk.offset = base_offset + offset;
                chunk.bytes = bytes;
                chunk.owner = owner;
                chunk.speculation = scanner.speculate(sequence_number, chunk.offset, bytes);
                chunk.starts_at_record_boundary = first_chunk;
                chunk.scan_bom = first_chunk && scan_bom_for_first_chunk;

                // The first chunk in a speculative window starts at a known row
                // boundary because source windows must be aligned to incomplete-row
                // starts before the next read.
                if (first_chunk) {
                    chunk.speculation.assumed_start_state = ParserDFAState();
                }

                chunks.push_back(chunk);
                sequence_number++;
            }

            return chunks;
        }

        template<bool EagerClassify = false>
        class ParallelCSVParser {
        public:
            ParallelCSVParser(
                const ParseFlagMap& parse_flags,
                const WhitespaceMap& ws_flags,
                size_t worker_count = 1,
                const ColNamesPtr& col_names = nullptr
            ) : parse_flags_(parse_flags),
                ws_flags_(ws_flags),
                col_names_(col_names),
                task_pool_(worker_count == 0 ? 1 : worker_count) {
                this->init_worker_parsers();
            }

            ~ParallelCSVParser() {}

            ParallelCSVParser(const ParallelCSVParser&) = delete;
            ParallelCSVParser& operator=(const ParallelCSVParser&) = delete;

            ParsedChunkRows parse_chunk(const SpeculativeParseChunk& chunk) const {
                ChunkParserCoreT<EagerClassify> parser(this->parse_flags_, this->ws_flags_, this->col_names_);
                return this->parse_chunk_with(parser, chunk);
            }

            template<typename RowSink>
            ParallelCSVParseResult parse_chunks(
                const std::vector<SpeculativeParseChunk>& chunks,
                RowSink& output,
                bool finish = true
            ) {
                ParallelCSVParseResult result;
                result.chunks_processed = chunks.size();
                for (size_t i = 0; i < chunks.size(); ++i) {
                    observe_speculation(result.diagnostics, chunks[i].speculation);
                }

                std::vector<ParsedChunkRows> parsed(chunks.size());
                this->parse_chunks_into(chunks, parsed);

                ChunkParserCoreT<EagerClassify> repair_parser(this->parse_flags_, this->ws_flags_, this->col_names_);
                SpeculativeParseValidator<RowSink, ChunkParserCoreT<EagerClassify>> validator(repair_parser, output);
                for (size_t i = 0; i < parsed.size(); ++i) {
                    validator.validate_and_release(std::move(parsed[i]));
                }
                validator.finish(finish);

                result.repair_count = validator.repair_count();
                result.diagnostics.validation_repairs += result.repair_count;
                result.has_pending_suffix = validator.has_pending_suffix();
                result.ending_state = validator.expected_start_state();
                if (!chunks.empty()) {
                    if (validator.has_pending_suffix()) {
                        result.complete_prefix_length = validator.pending_suffix().offset - chunks.front().offset;
                    }
                    else {
                        result.complete_prefix_length =
                            chunks.back().offset + chunks.back().bytes.size() - chunks.front().offset;
                    }
                }
                return result;
            }

        private:
            ParsedChunkRows parse_chunk_with(
                ChunkParserCoreT<EagerClassify>& parser,
                const SpeculativeParseChunk& chunk
            ) const {
                std::vector<CSVRow> rows;
                const ParserChunkResult parse_result = parser.parse_chunk(
                    chunk.bytes,
                    chunk.owner,
                    rows,
                    ParserChunkOptions(chunk.speculation.assumed_start_state, chunk.scan_bom, chunk.offset)
                );

                ParsedChunkRows result = split_parsed_chunk_rows(
                    chunk.sequence_number,
                    chunk.bytes,
                    chunk.owner,
                    parse_result,
                    std::move(rows),
                    chunk.starts_at_record_boundary,
                    chunk.offset
                );
                result.scan_bom = chunk.scan_bom;
                return result;
            }

            void parse_chunks_into(
                const std::vector<SpeculativeParseChunk>& chunks,
                std::vector<ParsedChunkRows>& parsed
            ) {
                if (this->task_pool_.worker_count() > 1 && chunks.size() > 1) {
                    this->parse_chunks_parallel(chunks, parsed);
                    return;
                }

                ChunkParserCoreT<EagerClassify> parser(this->parse_flags_, this->ws_flags_, this->col_names_);
                for (size_t i = 0; i < chunks.size(); ++i) {
                    parsed[i] = this->parse_chunk_with(parser, chunks[i]);
                }
            }

            void parse_chunks_parallel(
                const std::vector<SpeculativeParseChunk>& chunks,
                std::vector<ParsedChunkRows>& parsed
            ) {
                this->task_pool_.parallel_for(chunks.size(), [this, &chunks, &parsed](
                    size_t worker_index,
                    size_t task_index
                ) {
                    CSV_DEBUG_ASSERT(worker_index < this->worker_parsers_.size());
                    parsed[task_index] = this->parse_chunk_with(
                        this->worker_parsers_[worker_index],
                        chunks[task_index]
                    );
                });
            }

            void init_worker_parsers() {
                const size_t worker_count = this->task_pool_.worker_count();
                if (worker_count <= 1) {
                    return;
                }

                this->worker_parsers_.reserve(worker_count);
                for (size_t i = 0; i < worker_count; ++i) {
                    this->worker_parsers_.push_back(
                        ChunkParserCoreT<EagerClassify>(this->parse_flags_, this->ws_flags_, this->col_names_)
                    );
                }
            }

            ParseFlagMap parse_flags_;
            WhitespaceMap ws_flags_;
            ColNamesPtr col_names_;
            internals::parallel::IndexedTaskPool task_pool_;
            std::vector<ChunkParserCoreT<EagerClassify>> worker_parsers_;
        };
        }
    }
}

#endif


namespace csv {
    namespace internals {
        namespace parser {
        template<bool EagerClassify = false>
        class CSVParseOrchestrator : public ICSVParseOrchestrator {
        public:
            CSVParseOrchestrator(
                const ParseFlagMap& parse_flags,
                const WhitespaceMap& ws_flags,
                const CSVFormat& format,
                size_t source_size,
                const ColNamesPtr& col_names = nullptr,
                bool enable_speculative_parallel = true,
                bool source_size_known = true
            )
                : serial_parser_(parse_flags, ws_flags, col_names)
#if CSV_ENABLE_THREADS
                  , parse_flags_(parse_flags),
                  ws_flags_(ws_flags),
                  scanner_(parse_flags)
#endif
            {
#if CSV_ENABLE_THREADS
                size_t n_threads = 1;
                if (format.is_threading_enabled()
                    && enable_speculative_parallel
                    && (!source_size_known || source_size >= format.get_speculative_parallel_min_bytes())) {
                    n_threads = format.get_speculative_parallel_threads();
                    if (n_threads == 0) {
                        const unsigned int hardware_threads = std::thread::hardware_concurrency();
                        n_threads = hardware_threads == 0 ? 2 : static_cast<size_t>(hardware_threads);
                    }
                }

                this->worker_count_ = n_threads == 0 ? 1 : n_threads;
                this->use_speculative_parallel_ = format.is_threading_enabled()
                    && enable_speculative_parallel
                    && this->worker_count_ > 1
                    && (!source_size_known || format.should_use_speculative_parallel(
                        source_size,
                        this->worker_count_
                    ));
                if (this->use_speculative_parallel_) {
                    this->speculative_parser_.reset(new speculative::ParallelCSVParser<EagerClassify>(
                        this->parse_flags_,
                        this->ws_flags_,
                        this->worker_count_,
                        col_names
                    ));
                }
#else
                (void)parse_flags;
                (void)ws_flags;
                (void)format;
                (void)source_size;
                (void)enable_speculative_parallel;
                (void)source_size_known;
#endif
            }

            size_t worker_count() const noexcept override {
#if CSV_ENABLE_THREADS
                return this->use_speculative_parallel_ ? this->worker_count_ : 1;
#else
                return 1;
#endif
            }

            SpeculativeParseDiagnostics diagnostics() const noexcept override {
                return this->speculative_diagnostics_;
            }

            size_t read_window_size(size_t chunk_size) const noexcept override {
#if CSV_ENABLE_THREADS
                if (!this->use_speculative_parallel_ || this->worker_count_ <= 1) {
                    return chunk_size;
                }

                const size_t max_size = (std::numeric_limits<size_t>::max)();
                if (chunk_size > max_size / this->worker_count_) {
                    return max_size;
                }

                return chunk_size * this->worker_count_;
#else
                return chunk_size;
#endif
            }

            bool utf8_bom() const noexcept override {
                return this->serial_parser_.utf8_bom();
            }

            void reset_with_initial_state(ParserDFAState state) noexcept override {
                this->serial_parser_.reset_with_initial_state(state);
            }

            ParserDFAState ending_state() const noexcept override {
                return this->serial_parser_.ending_state();
            }

            CSVParseWindowResult parse_window(
                csv::string_view chunk,
                std::shared_ptr<void> owner,
                size_t base_offset,
                size_t serial_chunk_size,
                bool source_exhausted,
                RowCollection& output
            ) override {
#if CSV_ENABLE_THREADS
                if (this->use_speculative_parallel_
                    && this->worker_count_ > 1
                    && serial_chunk_size > 0
                    && chunk.size() > serial_chunk_size) {
                    return this->parse_speculative_window(
                        chunk,
                        std::move(owner),
                        base_offset,
                        serial_chunk_size,
                        source_exhausted,
                        output
                    );
                }
#else
                (void)serial_chunk_size;
#endif

                return this->parse_serial_window(
                    chunk,
                    std::move(owner),
                    base_offset,
                    source_exhausted,
                    output
                );
            }

        private:
            CSVParseWindowResult parse_serial_window(
                csv::string_view chunk,
                std::shared_ptr<void> owner,
                size_t base_offset,
                bool source_exhausted,
                RowCollection& output
            ) {
                CSVParseWindowResult result;
                const ParserChunkResult parse_result = this->serial_parser_.parse_chunk(
                    chunk,
                    std::move(owner),
                    output,
                    base_offset
                );
                result.complete_prefix_length = parse_result.complete_prefix_length;
                if (source_exhausted) {
                    this->serial_parser_.end_feed();
                }
                return result;
            }

#if CSV_ENABLE_THREADS
            CSVParseWindowResult parse_speculative_window(
                csv::string_view chunk,
                std::shared_ptr<void> owner,
                size_t base_offset,
                size_t serial_chunk_size,
                bool source_exhausted,
                RowCollection& output
            ) {
                CSVParseWindowResult result;
                auto chunks = speculative::make_speculative_parse_chunks(
                    chunk,
                    owner,
                    serial_chunk_size,
                    this->scanner_,
                    base_offset,
                    0,
                    base_offset == 0
                );

                const speculative::ParallelCSVParseResult parse_result = this->speculative_parser_->parse_chunks(
                    chunks,
                    output,
                    source_exhausted
                );

                result.complete_prefix_length = parse_result.complete_prefix_length;
                this->speculative_diagnostics_.merge(parse_result.diagnostics);
                return result;
            }
#endif

            CSVParserCore<
                RowCollection,
                PermissiveParsePolicy,
                CSVRowFieldPolicy<EagerClassify>,
                CSVRowRowPolicy> serial_parser_;
            SpeculativeParseDiagnostics speculative_diagnostics_;
#if CSV_ENABLE_THREADS
            ParseFlagMap parse_flags_;
            WhitespaceMap ws_flags_;
            speculative::SpeculativeScanner scanner_;
            bool use_speculative_parallel_ = false;
            size_t worker_count_ = 1;
            std::unique_ptr<speculative::ParallelCSVParser<EagerClassify>> speculative_parser_;
#endif
        };

        inline std::unique_ptr<ICSVParseOrchestrator> make_csv_parse_orchestrator(
            const ParseFlagMap& parse_flags,
            const WhitespaceMap& ws_flags,
            const CSVFormat& format,
            size_t source_size,
            const ColNamesPtr& col_names,
            bool enable_speculative_parallel,
            bool source_size_known
        ) {
            if (format.is_eager_field_classification_enabled()) {
                return std::unique_ptr<ICSVParseOrchestrator>(new CSVParseOrchestrator<true>(
                    parse_flags,
                    ws_flags,
                    format,
                    source_size,
                    col_names,
                    enable_speculative_parallel,
                    source_size_known
                ));
            }

            return std::unique_ptr<ICSVParseOrchestrator>(new CSVParseOrchestrator<false>(
                    parse_flags,
                    ws_flags,
                    format,
                    source_size,
                    col_names,
                    enable_speculative_parallel,
                    source_size_known
                ));
        }
        }
    }
}


namespace csv {
    namespace internals {
        namespace parser {
        constexpr size_t CSV_STREAM_WINDOW_SIZE_MAX = 256 * 1024 * 1024;

        /** A class for parsing CSV data from any std::istream, including
         *  non-seekable sources such as pipes and decompression filters.
         *
         *  @par Chunk boundary handling
         *  parse() returns the byte offset of the start of the last incomplete
         *  row in the current chunk (the "remainder"). Rather than seeking back
         *  to re-read those bytes (which requires a seekable stream), they are
         *  saved in `leftover_` and prepended to the next chunk. This is
         *  semantically identical to the old seek-back approach but works on
         *  any istream and avoids the syscall overhead of seekg().
         *
         *  @par Format resolution
         *  The constructor reads a head buffer from the stream via get_csv_head()
         *  and passes it to resolve_format_from_head(), which infers the delimiter
         *  and header row when not explicitly set. The head bytes are stored in
         *  `leftover_` so the first next() call re-parses them without re-reading.
         */
        template<typename TStream>
        class StreamParser : public CSVParserDriverBase {
            using RowCollection = ThreadSafeDeque<CSVRow>;

        public:
            StreamParser(
                TStream& source,
                const CSVFormat& format,
                const ColNamesPtr& col_names = nullptr
            ) : CSVParserDriverBase(format, col_names),
                source_(source) {
                this->resolve_format_from_head(format);
                this->parse_orchestrator_ = make_csv_parse_orchestrator(
                    this->parse_flags_,
                    this->whitespace_flags(),
                    this->format.format,
                    0,
                    col_names,
                    true,
                    false
                );
            }

            StreamParser(
                TStream& source,
                ParseFlagMap parse_flags,
                WhitespaceMap ws_flags
            ) : CSVParserDriverBase(parse_flags, ws_flags),
                source_(source) {
                this->parse_orchestrator_ = make_csv_parse_orchestrator(
                    this->parse_flags_,
                    this->whitespace_flags(),
                    CSVFormat(),
                    0,
                    nullptr,
                    false,
                    false
                );
            }

            ~StreamParser() {}

            std::string& get_csv_head() override {
                leftover_ = get_csv_head_stream(this->source_);
                return this->leftover_;
            }

            void next(size_t bytes = CSV_CHUNK_SIZE_DEFAULT) override {
                if (this->eof()) return;

                auto chunk_owner = std::make_shared<std::string>();
                auto& chunk = *chunk_owner;

                // Prepend leftover bytes from the previous chunk's incomplete
                // trailing row, then read enough bytes to fill the orchestrator
                // window. The window grows to chunk_size * worker_count only
                // when speculative parsing is active.
                chunk = std::move(leftover_);
                const size_t requested_window_size = this->parse_orchestrator_->read_window_size(bytes);
                const size_t stream_window_cap = bytes > CSV_STREAM_WINDOW_SIZE_MAX
                    ? bytes
                    : CSV_STREAM_WINDOW_SIZE_MAX;
                const size_t window_size = std::min(requested_window_size, stream_window_cap);
                const size_t read_size = chunk.size() < window_size
                    ? window_size - chunk.size()
                    : bytes;
                std::unique_ptr<char[]> buf(new char[read_size]);
                source_.read(buf.get(), (std::streamsize)read_size);

                const size_t n = static_cast<size_t>(source_.gcount());

                if (n > 0) chunk.append(buf.get(), n);

                // Check for real I/O errors only (bad bit indicates unrecoverable error).
                // failbit alone is not fatal - it's set on EOF or when requesting bytes
                // beyond available data, which is normal behavior for stringstreams.
                if (source_.bad()) {
                    throw_stream_read_failure();
                }

                const bool source_exhausted = source_.eof() || chunk.empty();
                const CSVParseWindowResult result = this->parse_orchestrator_->parse_window(
                    chunk,
                    chunk_owner,
                    this->stream_pos_,
                    bytes,
                    source_exhausted,
                    this->output()
                );

                if (source_exhausted) {
                    this->eof_ = true;
                }
                else {
                    // Save the tail bytes that begin an incomplete row so they
                    // are prepended to the next chunk (see class-level comment).
                    leftover_ = chunk.substr(result.complete_prefix_length);
                    this->stream_pos_ += result.complete_prefix_length;
                }
            }

            void reset_with_initial_state(ParserDFAState state) noexcept {
                if (this->parse_orchestrator_) {
                    this->parse_orchestrator_->reset_with_initial_state(state);
                }
                else {
                    CSVParserDriverBase::reset_with_initial_state(state);
                }
            }

            void reset_with_initial_state(bool starts_in_quoted, bool in_escape = false) noexcept {
                this->reset_with_initial_state(ParserDFAState{ starts_in_quoted, in_escape });
            }

            ParserDFAState ending_state() const noexcept {
                return this->parse_orchestrator_
                    ? this->parse_orchestrator_->ending_state()
                    : CSVParserDriverBase::ending_state();
            }

        private:
            // Bytes from the previous chunk that form the start of an incomplete
            // row, plus the initial head buffer on the first call.
            std::string leftover_;
            size_t stream_pos_ = 0;

            TStream& source_;
        };
        }
    }
}


/** The all encompassing namespace */
namespace csv {
    /** @class CSVReader
     *  @brief Main class for parsing CSVs from files and in-memory sources
     *
     *  All rows are compared to the column names for length consistency
     *  - By default, rows that are too short or too long are dropped
     *  - Custom behavior can be defined by overriding bad_row_handler in a subclass
     *
     *  **Streaming semantics:** CSVReader is a single-pass streaming reader. Every read
     *  operation — read_row(), the iterator interface — pulls rows permanently
     *  from the internal queue. Rows consumed by one interface are not visible to another.
     *  There is no rewind or seek.
     *
    *  **Ownership and sharing:** CSVReader is non-copyable and move-enabled. It manages
    *  live parsing state (worker thread, internal queue, and optional owned stream), so
    *  ownership transfer should be explicit. To share or transfer a reader, wrap it in a
    *  `std::unique_ptr<CSVReader>`:
     *  @code{.cpp}
     *  auto reader = std::make_unique<csv::CSVReader>("data.csv");
     *  process(std::move(reader));   // transfer ownership
     *  @endcode
     */
    class CSVReader {
    public:
        /**
         * An input iterator capable of handling large files.
         * @note Created by CSVReader::begin() and CSVReader::end().
         *
         * @par Iterating over a file
         * @snippet tests/test_csv_iterator.cpp CSVReader Iterator 1
         *
         * @par Using with `<algorithm>` library
         * @snippet tests/test_csv_iterator.cpp CSVReader Iterator 2
         * 
         * @note This iterator is `std::input_iterator_tag` (single-pass) by design.
         *       Algorithms requiring ForwardIterator are not safe to use directly on it.
         *       Copy to `std::vector<CSVRow>` first when random-access algorithms are needed.
         *       See `include/internal/ARCHITECTURE.md` § "CSVReader::iterator is single-pass by design"
         *       for the full rationale.
         */
        class iterator {
        public:
            #ifndef DOXYGEN_SHOULD_SKIP_THIS
            using value_type = CSVRow;
            using difference_type = std::ptrdiff_t;
            using pointer = CSVRow * ;
            using reference = CSVRow & ;
            using iterator_category = std::input_iterator_tag;
            #endif

            iterator() = default;
            iterator(CSVReader* reader) : daddy(reader) {}
            iterator(CSVReader*, CSVRow&&);

            /** Access the CSVRow held by the iterator */
            CONSTEXPR_14 reference operator*() { return this->row; }
            CONSTEXPR_14 reference operator*() const { return const_cast<reference>(this->row); }

            /** Return a pointer to the CSVRow the iterator has stopped at */
            CONSTEXPR_14 pointer operator->() { return &(this->row); }
            CONSTEXPR_14 pointer operator->() const { return const_cast<pointer>(&(this->row)); }

            iterator& operator++();   /**< Pre-increment iterator */
            iterator operator++(int); /**< Post-increment iterator */

            /** Returns true if iterators were constructed from the same CSVReader
             *  and point to the same row
             */
            CONSTEXPR bool operator==(const iterator& other) const noexcept {
                return (this->daddy == other.daddy) && (this->i == other.i);
            }

            CONSTEXPR bool operator!=(const iterator& other) const noexcept { return !operator==(other); }
        private:
            CSVReader * daddy = nullptr;  // Pointer to parent
            CSVRow row;                   // Current row
            size_t i = 0;               // Index of current row
        };

        /** @name Constructors
         *  Constructors for iterating over large files and parsing in-memory sources.
         */
         ///@{
        /** @brief Construct CSVReader from filename.
         * 
         * Native builds use CODE PATH 1 of 2: MmapParser with mio for maximum performance.
         * Emscripten builds fall back to the stream-based implementation because mmap is unavailable.
         *
         * During construction, parser installation performs an initial synchronous metadata
         * read so delimiter and header information are resolved before user reads begin.
         *
         * @note On native builds, bugs can exist in this path independently of the stream path.
         * @note When writing tests that validate I/O behavior, test both filename and stream constructors.
         * @see StreamParser for the stream-based alternative.
         */
        CSVReader(csv::string_view filename, const CSVFormat& format = CSVFormat::guess_csv())
            : _format(format),
              read_scheduler_(format.is_threading_enabled()) {
#if defined(__EMSCRIPTEN__)
            this->owned_stream = std::unique_ptr<std::istream>(
                new std::ifstream(std::string(filename), std::ios::binary)
            );

            if (!(*this->owned_stream)) {
                internals::throw_cannot_open_file(filename);
            }

            this->init_from_stream(*this->owned_stream, format);
#else
            this->init_parser(std::unique_ptr<internals::parser::CSVParserDriverBase>(
                new internals::parser::MmapParser(filename, format, this->col_names)
            ));
#endif
        }

        /** @brief Construct CSVReader from std::istream
         * 
         * Uses StreamParser. On native builds this is CODE PATH 2 of 2 and remains independent
         * from the filename-based mmap path. On Emscripten, the filename constructor also funnels
         * through this implementation.
         *
         *  @tparam TStream An input stream deriving from `std::istream`
         *  @note Delimiter/header guessing is still available by default via CSVFormat::guess_csv().
         *        For deterministic parsing of known dialects, pass an explicit CSVFormat.
         *  @note On native builds, tests that validate I/O behavior should cover both constructors
         *  @see MmapParser for the memory-mapped alternative
         */
        template<typename TStream,
            csv::enable_if_t<std::is_base_of<std::istream, TStream>::value, int> = 0>
        CSVReader(TStream &source, CSVFormat format = CSVFormat::guess_csv())
            : _format(format),
              read_scheduler_(format.is_threading_enabled()) {
            this->init_from_stream(source, format);
        }

        /** @brief Construct CSVReader from an owned std::istream
         *
         *  This is an opt-in safety switch for stream lifetime management.
         *  CSVReader takes ownership and guarantees the stream outlives parsing.
         */
        CSVReader(std::unique_ptr<std::istream> source,
            const CSVFormat& format = CSVFormat::guess_csv())
            : _format(format),
              owned_stream(std::move(source)),
              read_scheduler_(format.is_threading_enabled()) {
            if (!this->owned_stream) {
                throw std::invalid_argument(internals::ERROR_READER_NULL_STREAM);
            }

            this->init_from_stream(*this->owned_stream, format);
        }
        ///@}

        CSVReader(const CSVReader&) = delete;             ///< Not copyable
        CSVReader& operator=(const CSVReader&) = delete;  ///< Not copyable

        /** Move constructor.
         *
         * Required so C++11 builds can return CSVReader by value from helpers like
         * csv::parse()/csv::parse_unsafe(), where copy elision is not guaranteed.
         *
         * Any active read scheduler on the source is joined before moving parser state to
         * avoid a thread continuing to run against the source object's address.
         */
        CSVReader(CSVReader&& other) noexcept :
            read_scheduler_(other._format.is_threading_enabled()) {
            other.read_scheduler_.join();
            this->move_state_from(other);
        }

        /** Move assignment.
         *
         * Joins active workers on both sides before transferring parser state.
         */
        CSVReader& operator=(CSVReader&& other) noexcept {
            if (this == &other) {
                return *this;
            }

            this->read_scheduler_.join();
            other.read_scheduler_.join();
            this->move_state_from(other);

            return *this;
        }

        ~CSVReader() {
            this->read_scheduler_.join();
        }

        /** @name Retrieving CSV Rows */
        ///@{
        /** Retrieve the next CSV row, returning `true` while more rows are available.
         *
         * This is the lowest-level row-consumption API on `CSVReader`. Each successful
         * call overwrites `row` with the next parsed record in stream order.
         *
         * @returns `true` if a row was produced, `false` after end-of-stream is reached.
         *
         * @note This permanently consumes rows from the stream.
         *
         * @par Performance Notes
         * - Reads chunks of data that are `csv::internals::CSV_CHUNK_SIZE_DEFAULT`
         *   bytes large at a time by default.
         * - For field-access performance details, read the documentation for
         *   `CSVRow` and `CSVField`.
         *
         * **Example:**
         * \snippet tests/test_read_csv.cpp CSVField Example
         */
        bool read_row(CSVRow &row);

        /** Read up to `max_rows` rows into a caller-owned batch buffer.
         *
         * This is the easiest way to process a CSV in bounded batches without
         * materializing the entire file. Each call clears `out`, then appends up
         * to `max_rows` newly parsed rows in stream order.
         *
         * @returns `true` if this call produced any rows. Returns `false` only
         *          after end-of-stream is reached and no rows were produced.
         *          A final partial chunk still returns `true`.
         *
         * @param[out] out Destination batch buffer. Existing contents are discarded.
         * @param[in] max_rows Maximum number of rows to place into `out`.
         *
         * @note Like read_row(), this permanently consumes rows from the stream.
         * @note `std::vector<CSVRow>` is intentionally the only supported container:
         *       it matches the public batch-consumption pattern better than the
         *       internal deque-based producer/consumer queue.
         *
         * **Example:**
         * \snippet tests/test_read_csv_file.cpp CSVReader read_chunk Example
         */
        bool read_chunk(std::vector<CSVRow>& out, size_t max_rows);
        iterator begin();
        CSV_CONST iterator end() const noexcept;

        /** Returns true if we have reached end of file */
        bool eof() const noexcept { return this->parser->eof(); }
        ///@}

        /** @name CSV Metadata */
        ///@{
        /** Return the resolved parsing format for this CSV source.
         *
         * The returned format reflects delimiter/header inference and the active
         * column names after construction.
         */
        CSVFormat get_format() const;

        /** Return the active column names in CSV order. */
        const std::vector<std::string>& get_col_names() const{
            static const std::vector<std::string> empty_col_names;
            return (this->col_names) ? this->col_names->get_col_names() : empty_col_names;
        }

        /** Internal accessor for preserving resolved column-name lookup policy across helper types. */
        internals::ConstColNamesPtr col_names_ptr() const noexcept {
            return this->col_names;
        }

        /** Return the index of `col_name`, or `csv::CSV_NOT_FOUND` if absent. */
        int index_of(csv::string_view col_name) const {
            return this->col_names->index_of(col_name);
        }
        ///@}

        /** @name CSV Metadata: Attributes */
        ///@{
        /** Whether or not the file or stream contains valid CSV rows,
         *  not including the header.
         *
         *  @note Gives an accurate answer regardless of when it is called.
         *
         */
        CONSTEXPR bool empty() const noexcept { return this->n_rows() == 0; }

        /** Retrieves the number of rows that have been read so far */
        CONSTEXPR size_t n_rows() const noexcept { return this->_n_rows; }

        /** Whether or not CSV was prefixed with a UTF-8 bom */
        bool utf8_bom() const noexcept { return this->parser->utf8_bom(); }

        /** Return speculative-parsing counters for filename-backed readers. */
        internals::SpeculativeParseDiagnostics speculative_diagnostics() const noexcept {
            return this->parser ? this->parser->speculative_diagnostics()
                : internals::SpeculativeParseDiagnostics();
        }

        /** Return the number of parser worker threads used by the active parser. */
        size_t parse_worker_count() const noexcept {
            return this->parser ? this->parser->parse_worker_count() : 1;
        }
        ///@}

    protected:
        /**
         * \defgroup csv_internal CSV Parser Internals
         * @brief Internals of CSVReader. Only maintainers and those looking to
         *        extend the parser should read this.
         * @{
         */

        /** Sets this reader's column names and associated data */
        void set_col_names(const std::vector<std::string>&);

        /** @name CSV Settings **/
        ///@{
        CSVFormat _format;
        ///@}

        /** @name Parser State */
        ///@{
        /** Pointer to a object containing column information */
        internals::ColNamesPtr col_names = std::make_shared<internals::ColNames>();

        /** Helper class which actually does the parsing */
        std::unique_ptr<internals::parser::CSVParserDriverBase> parser = nullptr;

        /** Queue of parsed CSV rows */
        std::unique_ptr<RowCollection> records{new RowCollection(100)};

        /**
         * Optional owned stream used by two paths:
         *  1) Emscripten filename-constructor fallback to stream parsing
         *  2) Opt-in ownership constructor taking std::unique_ptr<std::istream>
         */
        std::unique_ptr<std::istream> owned_stream = nullptr;

        size_t n_cols = 0;  /**< The number of columns in this CSV */
        size_t _n_rows = 0; /**< How many rows (minus header) have been read so far */

        /** @name Worker Reading Functions
         *
         *  Functions that actively drive parser execution and produce rows.
         */
        ///@{
        bool read_csv(size_t bytes = internals::CSV_CHUNK_SIZE_DEFAULT);
        ///@}

        /**@}*/

    private:
        /** Whether or not rows before header were trimmed */
        bool header_trimmed = false;
        
        /** @name Reader Scheduling: Flags and State */
        ///@{
        size_t _chunk_size = internals::CSV_CHUNK_SIZE_DEFAULT; /**< Current chunk size in bytes */
        bool _read_requested = false; /**< Flag to detect infinite read loops (Issue #218) */
        internals::CSVReadScheduler read_scheduler_;
        ///@}

        void move_state_from(CSVReader& other) noexcept {
            this->_format = std::move(other._format);
            this->col_names = std::move(other.col_names);
            this->parser = std::move(other.parser);
            this->records = std::move(other.records);
            this->owned_stream = std::move(other.owned_stream);
            this->n_cols = other.n_cols;
            this->_n_rows = other._n_rows;
            this->header_trimmed = other.header_trimmed;
            this->_chunk_size = other._chunk_size;
            this->_read_requested = other._read_requested;
            this->read_scheduler_.set_threading_enabled(this->_format.is_threading_enabled());
            this->read_scheduler_.adopt_exception(other.read_scheduler_.take_exception());
            other.reset_after_move();
        }

        void reset_after_move() noexcept {
            this->n_cols = 0;
            this->_n_rows = 0;
            this->header_trimmed = false;
            this->_read_requested = false;
            this->_chunk_size = internals::CSV_CHUNK_SIZE_DEFAULT;
        }

        /** @name Format and Header Helpers
         *
         *  Parser bootstrap and metadata/header handling shared across mmap and stream paths.
         */
        ///@{
        /** Shared parser installation after source-specific bootstrap has completed
         *  in concrete parser implementations.
         */
        void init_parser(std::unique_ptr<internals::parser::CSVParserDriverBase> parser);

        template<typename TStream,
            csv::enable_if_t<std::is_base_of<std::istream, TStream>::value, int> = 0>
        void init_from_stream(TStream& source, CSVFormat format) {
            this->init_parser(
                std::unique_ptr<internals::parser::CSVParserDriverBase>(
                    new internals::parser::StreamParser<TStream>(source, format, this->col_names)
                )
            );
        }

        /** Read initial chunk to get metadata */
        void initial_read() {
            this->read_scheduler_.run([this] { this->read_csv(this->_chunk_size); });
            this->read_scheduler_.join();
            this->read_scheduler_.rethrow_exception_if_any();
        }

        void trim_header();
        ///@}

        /** @name Reading Helpers
         *
         *  Shared consumer-side logic used by both single-row and batch reads.
         */
        ///@{
        /** Apply variable-column policy to a parsed row and emit it if accepted. */
        bool accept_row(CSVRow&& candidate, CSVRow* single_row, std::vector<CSVRow>* batch_rows);

        /**
         * Try to pull more rows from the queue, returning false if we're done.
         * 
         * Returns true if parsing is still parsing, or the queue has hot and fresh
         * rows ready.
         */
        bool check_for_rows();

        /** Drain as many already-queued rows as possible into a caller-owned chunk buffer.
         *
         *  Applies the same variable-column filtering policy as read_row(), but amortizes
         *  queue synchronization across many rows.
         */
        void drain_rows_into_chunk(std::vector<CSVRow>& out, size_t max_rows);
        ///@}
    };

    #ifdef CSV_HAS_CXX20
    static_assert(
        internals::csv_write_rows_input_range<CSVReader>,
        "CSVReader must remain compatible with csv::DelimWriter::write_rows()."
    );
    #endif
}

/** @file
  *  A standalone header file for writing delimiter-separated files
  */

#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#ifdef CSV_HAS_CXX20
#include <ranges>
#endif
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>


namespace csv {
namespace internals {
    static int DECIMAL_PLACES = 5;

        /** Compute 10 to the power of an integral exponent. */
        template<typename T>
        CSV_CONST CONSTEXPR_14
        long double pow10(const T& n) noexcept {
            static_assert(std::is_integral<T>::value, "pow10 only supports integral exponents");

            long double multiplicand = n > 0 ? 10 : 0.1,
                ret = 1;
            T iterations = n > 0 ? n : -n;

            for (T i = 0; i < iterations; i++) {
                ret *= multiplicand;
            }

            return ret;
        }

        template<>
        CSV_CONST CONSTEXPR_14
        long double pow10(const unsigned& n) noexcept {
            long double multiplicand = n > 0 ? 10 : 0.1,
                ret = 1;

            for (unsigned i = 0; i < n; i++) {
                ret *= multiplicand;
            }

            return ret;
        }

        /**
         * Calculate the absolute value of a number
         */
        template<typename T = int>
        inline T csv_abs(T x) {
            return abs(x);
        }

        template<>
        inline int csv_abs(int x) {
            return abs(x);
        }

        template<>
        inline long int csv_abs(long int x) {
            return labs(x);
        }

        template<>
        inline long long int csv_abs(long long int x) {
            return llabs(x);
        }

        template<>
        inline float csv_abs(float x) {
            return fabsf(x);
        }

        template<>
        inline double csv_abs(double x) {
            return fabs(x);
        }

        template<>
        inline long double csv_abs(long double x) {
            return fabsl(x);
        }

        /** 
         * Calculate the number of digits in a number
         */
        template<
            typename T,
            csv::enable_if_t<std::is_arithmetic<T>::value, int> = 0
        >
        int num_digits(T x)
        {
            x = csv_abs(x);
            int digits = 0;
            for (; x >= 1; digits++)
                x /= 10;

            return (x == 0) ? 1 : digits;
        }

        /** to_string() for unsigned integers */
        template<typename T,
            csv::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        inline std::string to_string(T value) {
            std::string digits_reverse = "";
            if (value == 0) return "0";

            for (; value > 0; value /= 10)
                digits_reverse += (char)('0' + (value % 10));

            return std::string(digits_reverse.rbegin(), digits_reverse.rend());
        }

        /** to_string() for signed integers */
        template<
            typename T,
            csv::enable_if_t<std::is_integral<T>::value && std::is_signed<T>::value, int> = 0
        >
        inline std::string to_string(T value) {
            return (value >= 0) ? to_string((size_t)value)
                : "-" + to_string((size_t)(value * -1));
        }

    /** to_string() for floating point numbers */
    template<
        typename T,
        csv::enable_if_t<std::is_floating_point<T>::value, int> = 0
    >
    inline std::string to_string(T value) {
        std::string result = "";

            long double integral_part;
            long double fractional_part = csv_abs(std::modf((long double)value, &integral_part));

            const long double scale = pow10(DECIMAL_PLACES);
            long double rounded_fractional = std::round(fractional_part * scale);

            // Work with the absolute value of the integral part so digit extraction
            // and carry both work correctly for negative numbers.
            long double abs_integral = csv_abs(integral_part);

            // Carry rounding overflow from fractional digits into integral digits.
            if (rounded_fractional >= scale) {
                abs_integral += 1;
                rounded_fractional = 0;
            }

            // Integral part
            if (value < 0) result = "-";

            if (abs_integral == 0) {
                result += "0";
            }
            else {
                for (int n_digits = num_digits(abs_integral); n_digits > 0; n_digits --) {
                    int digit = (int)(std::fmod(abs_integral, pow10(n_digits)) / pow10(n_digits - 1));
                    result += (char)('0' + digit);
                }
            }

            // Decimal part
            result += ".";

            if (rounded_fractional > 0) {
                for (int n_digits = DECIMAL_PLACES; n_digits > 0; n_digits--) {
                    int digit = (int)(std::fmod(rounded_fractional, pow10(n_digits)) / pow10(n_digits - 1));
                    result += (char)('0' + digit);
                }
            }
            else {
                result += "0";
            }

        return result;
    }
}

/** Sets how many places after the decimal will be written for floating point numbers. */
inline static void set_decimal_places(int precision) {
    internals::DECIMAL_PLACES = precision;
}

namespace internals {
    /** SFINAE trait: detects if a type is iterable (has std::begin/end). */
    template<typename T, typename = void>
    struct is_iterable : std::false_type {};

        template<typename T>
        struct is_iterable<T, typename std::enable_if<true>::type> {
        private:
            template<typename U>
            static auto test(int) -> decltype(
                std::begin(std::declval<const U&>()),
                std::end(std::declval<const U&>()),
                std::true_type{}
            );
            template<typename>
            static std::false_type test(...);
        public:
            static constexpr bool value = decltype(test<T>(0))::value;
        };

    /** SFINAE trait: detects if a type is a std::tuple. */
    template<typename T, typename = void>
    struct is_tuple : std::false_type {};

        template<typename T>
        struct is_tuple<T, typename std::enable_if<true>::type> {
        private:
            template<typename U>
            static auto test(int) -> decltype(std::tuple_size<U>::value, std::true_type{});
            template<typename>
            static std::false_type test(...);
        public:
            static constexpr bool value = decltype(test<T>(0))::value;
        };

}

/** @name CSV Writing */
///@{
    /** 
     *  Class for writing delimiter separated values files
     *
     *  To write formatted strings, one should
     *   -# Initialize a DelimWriter with respect to some output stream 
     *   -# Call write_row() on std::vector<std::string>s of unformatted text
     *
     *  @tparam OutputStream The output stream, e.g. `std::ofstream`, `std::stringstream`
     *  @tparam Delim        The delimiter character
     *  @tparam Quote        The quote character
     *  @par Hint
     *  Use the aliases csv::CSVWriter<OutputStream> to write CSV
     *  formatted strings and csv::TSVWriter<OutputStream>
     *  to write tab separated strings
     *
     *  @par Example w/ std::vector, std::deque, std::list
     *  @snippet test_write_csv.cpp CSV Writer Example
     *
     *  @par Example w/ std::tuple
     *  @snippet test_write_csv.cpp CSV Writer Tuple Example
     */
    template<class OutputStream, char Delim, char Quote>
    class DelimWriter {
    public:
        /** Construct a DelimWriter over the specified output stream. */

        DelimWriter(OutputStream& _out, bool _quote_minimal = true)
            : out(&_out), quote_minimal(_quote_minimal) {}

        /** Construct a DelimWriter that owns an output file stream. */
        template<typename T = OutputStream,
            csv::enable_if_t<std::is_same<T, std::ofstream>::value, int> = 0>
        DelimWriter(const std::string& filename, bool _quote_minimal = true)
            : owned_out(new std::ofstream(filename, std::ios::out)),
            out(owned_out.get()),
            quote_minimal(_quote_minimal) {
            if (!owned_out->is_open())
                internals::throw_failed_open_for_writing(filename);
        }

        DelimWriter(const DelimWriter&) = delete;
        DelimWriter& operator=(const DelimWriter&) = delete;

        DelimWriter(DelimWriter&& other) noexcept
            : owned_out(std::move(other.owned_out)),
            out(other.out),
            quote_minimal(other.quote_minimal),
            auto_flush_(other.auto_flush_),
            batch_buffer_(std::move(other.batch_buffer_)) {
            if (owned_out) {
                out = owned_out.get();
            }
            other.out = nullptr;
            other.quote_minimal = true;
        }

        DelimWriter& operator=(DelimWriter&& other) noexcept {
            if (this == &other) return *this;

            owned_out = std::move(other.owned_out);
            out = other.out;
            quote_minimal = other.quote_minimal;
            auto_flush_ = other.auto_flush_;
            batch_buffer_ = std::move(other.batch_buffer_);

            if (owned_out) {
                out = owned_out.get();
            }

            other.out = nullptr;
            other.quote_minimal = true;
            return *this;
        }

        /** Configure whether each write operation flushes the underlying stream. */
        DelimWriter& set_auto_flush(bool value) & noexcept {
            this->auto_flush_ = value;
            return *this;
        }

        /** Configure whether each write operation flushes the underlying stream. */
        DelimWriter&& set_auto_flush(bool value) && noexcept {
            this->auto_flush_ = value;
            return std::move(*this);
        }

        /** Return whether each write operation flushes the underlying stream. */
        bool get_auto_flush() const noexcept {
            return this->auto_flush_;
        }

        /** Destructor will flush remaining data. */
        ~DelimWriter() {
            if (out) {
                flush_batch();
                out->flush();
            }
        }

        /** Write a C-style array of strings as one delimited row. */
        template<typename T, size_t N>
        DelimWriter& operator<<(const T (&record)[N]) {
            write_range_impl(record);
            return *this;
        }

        /** Write a std::array of strings as one delimited row. */
        template<typename T, size_t N>
        DelimWriter& operator<<(const std::array<T, N>& record) {
            write_range_impl(record);
            return *this;
        }

        /** Write a row from any single argument accepted by operator<<
         *  (std::vector, std::array, std::tuple, C-array, C++20 range, etc.).
         *
         *  @code
         *  writer.write_row(my_vector);
         *  writer.write_row(my_tuple);
         *  @endcode
         *
         *  SFINAE ensures this overload is only viable when the argument type
         *  is accepted by an existing operator<< overload.
         */
        template<typename T>
        auto write_row(T&& record) -> decltype(*this << std::forward<T>(record)) {
            return *this << std::forward<T>(record);
        }

        /** Write a row from a variadic list of mixed-type values.
         *
         *  Requires at least two arguments; for a single container or tuple,
         *  use the single-argument overload above.
         *
         *  @code
         *  writer.write_row("Alice", 30, 1.75, "Paris");
         *  @endcode
         */
        template<typename T, typename U, typename... Rest>
        DelimWriter& write_row(T&& first, U&& second, Rest&&... rest) {
            this->write_tuple<0>(std::forward_as_tuple(
                std::forward<T>(first), std::forward<U>(second), std::forward<Rest>(rest)...));
            return *this;
        }

#ifdef CSV_HAS_CXX20
        /** Write many rows using a shared batch buffer.
         *
         *  Accepts any input_range whose elements are either:
         *   - ranges of string-like fields, or
         *   - row-like objects exposing to_sv_range().
         *
         *  Buffered writers flush the batch when it grows large or when this call ends.
         *  Auto-flushing writers additionally flush the underlying stream once at the
         *  end of the bulk call.
         *
         *  @note Requires C++20 or later.
         */
        template<std::ranges::input_range Rows>
            requires internals::csv_write_rows_input_range<Rows>
        DelimWriter& write_rows(Rows&& rows) {
            for (auto&& row : rows) {
                append_row_like(row);
                flush_batch_if_needed();
            }

            finish_write_call();
            return *this;
        }

        /** Write a range of string-like fields as one delimited row.
         *
         *  Accepts any input_range whose elements are convertible to csv::string_view.
         *  This includes std::vector<std::string>, std::vector<csv::string_view>,
         *  std::array, C++20 views, etc.
         */
        template<std::ranges::input_range Range>
        DelimWriter& operator<<(Range&& container)
            requires std::ranges::input_range<Range>
                && std::convertible_to<std::ranges::range_reference_t<Range>, csv::string_view> {
            write_range_impl(container);
            return *this;
        }

        /** Write a row-like object that exposes to_sv_range(). */
        template<typename RowLike>
        DelimWriter& operator<<(const RowLike& row)
            requires internals::has_to_sv_range<RowLike>
                && (!internals::csv_string_field_range<RowLike>) {
            append_row_like(row);
            finish_write_call();
            return *this;
        }
#else
        /** Write a range of string-like fields as one delimited row.
         *
         *  Accepts any input_range whose elements are convertible to csv::string_view.
         *  This includes std::vector<std::string>, std::vector<csv::string_view>,
         *  std::array, C++20 views, etc.
         *
         *  @note Implementation detail: Uses SFINAE for runtime compatibility.
         */
        template<typename Range>
        typename std::enable_if<
            internals::is_iterable<Range>::value 
            && !internals::is_tuple<Range>::value
            && !std::is_same<Range, std::string>::value
            && !std::is_same<Range, csv::string_view>::value,
            DelimWriter&
        >::type operator<<(const Range& record) {
            write_range_impl(record);
            return *this;
        }
#endif

        /** @copydoc operator<< */
        template<typename... T>
        DelimWriter& operator<<(const std::tuple<T...>& record) {
            this->write_tuple<0, T...>(record);
            return *this;
        }

        /** Flushes the written data. */
        void flush() {
            flush_batch();
            out->flush();
        }

    private:
        /** Append delimited fields from a range without terminating the record.
         *
         *  Shared by single-row writes and bulk row appends so escaping and
         *  delimiter handling stay on one code path.
         */
        template<typename Range>
        inline void append_range_fields(Range&& record) {
            auto it = std::begin(record);
            auto end = std::end(record);

            if (it != end) {
                write_field(*it);
                ++it;
            }

            for (; it != end; ++it) {
                batch_buffer_.push_back(Delim);
                write_field(*it);
            }
        }

        /** SIMD fast-forward for quote escape logic */
        size_t find_first_special_for_writer(csv::string_view in) const {
            size_t pos = internals::find_next_non_special(in, 0, simd_sentinels_);

            for (; pos < in.size(); ++pos) {
                char ch = in[pos];
                if (ch == Quote || ch == Delim || ch == '\r' || ch == '\n')
                    return pos;
            }

            return in.size();
        }

        /** Helper to write a complete range-backed row and apply the normal
         *  end-of-record and flush policy for operator<< entry points.
         */
        template<typename Range>
        inline void write_range_impl(const Range& record) {
            append_range_fields(record);

            end_record();
            finish_write_call();
        }

#ifdef CSV_HAS_CXX20
        template<typename Row>
        void append_row_like(Row&& row) {
            IF_CONSTEXPR(internals::csv_string_field_range<Row>) {
                append_range_fields(std::forward<Row>(row));
            }
            else {
                append_range_fields(row.to_sv_range());
            }

            end_record();
        }
#endif

        template<
            typename T,
            csv::enable_if_t<
                !std::is_convertible<T, std::string>::value
                && !std::is_convertible<T, csv::string_view>::value
            , int> = 0
        >
        void write_field(T in) {
            const std::string serialized = internals::to_string(in);
            write_raw(serialized);
        }

        template<
            typename T,
            csv::enable_if_t<
                std::is_convertible<T, std::string>::value
                || std::is_convertible<T, csv::string_view>::value
            , int> = 0
        >
        void write_field(T in) {
            IF_CONSTEXPR(std::is_convertible<T, csv::string_view>::value) {
                write_escaped_field(in);
            }
            else {
                const std::string serialized(in);
                write_escaped_field(serialized);
            }
        }

        void write_raw(csv::string_view in) {
            if (!in.empty())
                batch_buffer_.append(in.data(), in.size());
        }

        void write_escaped_field(csv::string_view in) {
            const size_t first_special = find_first_special_for_writer(in);

            if (first_special == in.size()) {
                if (!quote_minimal) {
                    batch_buffer_.push_back(Quote);
                    write_raw(in);
                    batch_buffer_.push_back(Quote);
                } else {
                    write_raw(in);
                }
                return;
            }

            write_quoted_field(in, first_special);
        }

        void write_quoted_field(csv::string_view in, size_t first_special) {
            batch_buffer_.push_back(Quote);

            size_t chunk_start = 0;
            size_t pos = first_special;
            while (pos < in.size()) {
                if (in[pos] == Quote) {
                    write_raw(in.substr(chunk_start, pos - chunk_start));
                    batch_buffer_.push_back(Quote);
                    batch_buffer_.push_back(Quote);
                    chunk_start = pos + 1;
                }

                ++pos;
            }

            write_raw(in.substr(chunk_start));
            batch_buffer_.push_back(Quote);
        }

        /** Recursive template for writing std::tuples */
        template<size_t Index = 0, typename... T>
        typename std::enable_if<Index < sizeof...(T), void>::type write_tuple(const std::tuple<T...>& record) {
            write_field(std::get<Index>(record));

            CSV_MSVC_PUSH_DISABLE(4127)
            IF_CONSTEXPR (Index + 1 < sizeof...(T)) batch_buffer_.push_back(Delim);
            CSV_MSVC_POP

            this->write_tuple<Index + 1>(record);
        }

        /** Base case for writing std::tuples */
        template<size_t Index = 0, typename... T>
        typename std::enable_if<Index == sizeof...(T), void>::type write_tuple(const std::tuple<T...>& record) {
            (void)record;
            end_record();
            finish_write_call();
        }

        /** Finalize a single CSV row inside the shared batch buffer. */
        void end_record() {
            batch_buffer_.push_back('\n');
        }

        void finish_write_call() {
            if (this->auto_flush_) {
                flush_batch();
                out->flush();
                return;
            }

            flush_batch_if_needed();
        }

        void flush_batch() {
            if (batch_buffer_.empty()) return;

            out->write(batch_buffer_.data(), static_cast<std::streamsize>(batch_buffer_.size()));
            batch_buffer_.clear();
        }

        void flush_batch_if_needed() {
            if (batch_buffer_.size() >= batch_flush_threshold_)
                flush_batch();
        }

        /**
         * An owned output stream, if the writer owns it.
         * May be null if the writer does not own its output stream, i.e.
         * if it was initialized with an output stream reference instead of a filename.
         */
        std::unique_ptr<OutputStream> owned_out;

        /** Pointer to the output stream (which may or may not be owned by this writer). */
        OutputStream* out;

        bool quote_minimal;
        bool auto_flush_ = true;
        static constexpr size_t batch_flush_threshold_ = 64 * 1024;
        std::string batch_buffer_;
        internals::SentinelVecs simd_sentinels_{Delim, Quote};
    };

    /** An alias for csv::DelimWriter for writing standard CSV files
     *
     *  @sa csv::DelimWriter::operator<<()
     *
     *  @note Use `csv::make_csv_writer()` to instantiate this class over
     *        an actual output stream.
     */
    template<class OutputStream>
    using CSVWriter = DelimWriter<OutputStream, ',', '"'>;

    /** Class for writing tab-separated values files
    *
     *  @sa csv::DelimWriter::write_row()
     *  @sa csv::DelimWriter::operator<<()
     *
     *  @note Use `csv::make_tsv_writer()` to instantiate this class over
     *        an actual output stream.
     */
    template<class OutputStream>
    using TSVWriter = DelimWriter<OutputStream, '\t', '"'>;

    /** Return a csv::CSVWriter over the output stream */
    template<class OutputStream>
    inline CSVWriter<OutputStream> make_csv_writer(OutputStream& out, bool quote_minimal=true) {
        return CSVWriter<OutputStream>(out, quote_minimal);
    }

    /** Return a csv::TSVWriter over the output stream */
    template<class OutputStream>
    inline TSVWriter<OutputStream> make_tsv_writer(OutputStream& out, bool quote_minimal=true) {
        return TSVWriter<OutputStream>(out, quote_minimal);
    }
    ///@}
}


#include <memory>
#include <stdexcept>
#include <string>
#include <vector>


#include <stdexcept>
#include <string>
#include <utility>


#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>


namespace csv {
    struct RowOverlay {
        RowOverlay() = default;
        RowOverlay(const RowOverlay&) = delete;
        RowOverlay& operator=(const RowOverlay&) = delete;

        RowOverlay(RowOverlay&& other) noexcept : values(std::move(other.values)) {
            busy.clear(std::memory_order_release);
        }

        RowOverlay& operator=(RowOverlay&& other) noexcept {
            if (this != &other) {
                values = std::move(other.values);
                busy.clear(std::memory_order_release);
            }
            return *this;
        }

        bool try_get_copy(size_t col_index, std::string& out) const {
            row_overlay_lock_guard lock(this);
            auto it = values.find(col_index);
            if (it == values.end()) {
                return false;
            }

            out = it->second;
            return true;
        }

        /** Return a view into an edited cell without copying.
         *
         *  This exists for read-only batch scans such as schema/type inference,
         *  where the caller immediately consumes the value and no concurrent
         *  sparse-overlay edits are expected. General cell access should keep
         *  using try_get_copy() or DataFrameCell so callers do not accidentally
         *  retain a view across later mutation.
         */
        bool try_get_view(size_t col_index, csv::string_view& out) const {
            row_overlay_lock_guard lock(this);
            auto it = values.find(col_index);
            if (it == values.end()) {
                return false;
            }

            out = csv::string_view(it->second);
            return true;
        }

        void set(size_t col_index, std::string value) {
            row_overlay_lock_guard lock(this);
            values[col_index] = std::move(value);
        }

        bool empty() const {
            row_overlay_lock_guard lock(this);
            return values.empty();
        }

    private:
        struct row_overlay_lock_guard {
            explicit row_overlay_lock_guard(const RowOverlay* overlay)
                : busy(const_cast<std::atomic_flag&>(overlay->busy)) {
                while (busy.test_and_set(std::memory_order_acquire)) {}
            }

            ~row_overlay_lock_guard() {
                busy.clear(std::memory_order_release);
            }

            std::atomic_flag& busy;
        };

        mutable std::atomic_flag busy = ATOMIC_FLAG_INIT;
        std::unordered_map<size_t, std::string> values;
    };

    struct RowOverlaySlot {
        RowOverlaySlot() noexcept : ptr(nullptr) {}
        RowOverlaySlot(const RowOverlaySlot&) = delete;
        RowOverlaySlot& operator=(const RowOverlaySlot&) = delete;

        RowOverlaySlot(RowOverlaySlot&& other) noexcept
            : ptr(nullptr),
            owned(std::move(other.owned)) {
            RowOverlay* overlay = owned.get();
            ptr.store(overlay, std::memory_order_release);
            other.ptr.store(nullptr, std::memory_order_release);
        }

        RowOverlaySlot& operator=(RowOverlaySlot&& other) noexcept {
            if (this != &other) {
                owned = std::move(other.owned);
                RowOverlay* overlay = owned.get();
                ptr.store(overlay, std::memory_order_release);
                other.ptr.store(nullptr, std::memory_order_release);
            }

            return *this;
        }

        RowOverlay* get() noexcept {
            return ptr.load(std::memory_order_acquire);
        }

        const RowOverlay* get() const noexcept {
            return ptr.load(std::memory_order_acquire);
        }

        RowOverlay* ensure() {
            RowOverlay* overlay = this->get();
            if (!overlay) {
                owned.reset(new RowOverlay());
                overlay = owned.get();
                ptr.store(overlay, std::memory_order_release);
            }

            return overlay;
        }

    private:
        std::atomic<RowOverlay*> ptr;
        std::unique_ptr<RowOverlay> owned;
    };
}


namespace csv {
    class DataFrameCell : public CSVField {
    public:
        using CSVField::get;
        using CSVField::get_sv;
        using CSVField::is_float;
        using CSVField::is_int;
        using CSVField::is_null;
        using CSVField::is_num;
        using CSVField::is_str;
        using CSVField::try_get;
        using CSVField::type;

        DataFrameCell() : CSVField(csv::string_view()), row(nullptr), row_overlay(nullptr), col_index(0), can_mutate(false) {}

        DataFrameCell(const DataFrameCell& other)
            : CSVField(csv::string_view()),
            row(other.row),
            row_overlay(other.row_overlay),
            col_index(other.col_index),
            can_mutate(other.can_mutate),
            owned_value_(other.owned_value_) {
            this->refresh_value();
        }

        DataFrameCell(DataFrameCell&& other) noexcept
            : CSVField(csv::string_view()),
            row(other.row),
            row_overlay(other.row_overlay),
            col_index(other.col_index),
            can_mutate(other.can_mutate),
            owned_value_(std::move(other.owned_value_)) {
            this->refresh_value();
        }

        DataFrameCell(
            const CSVRow* _row,
            RowOverlay* _row_overlay,
            size_t _col_index
        ) : CSVField(csv::string_view()),
            row(_row),
            row_overlay(_row_overlay),
            col_index(_col_index),
            can_mutate(true) {
            this->refresh_value();
        }

        DataFrameCell(
            const CSVRow* _row,
            const RowOverlay* _row_overlay,
            size_t _col_index
        ) : CSVField(csv::string_view()),
            row(_row),
            row_overlay(_row_overlay),
            col_index(_col_index),
            can_mutate(false) {
            this->refresh_value();
        }

        DataFrameCell& operator=(const DataFrameCell& other) {
            if (this != &other) {
                row = other.row;
                row_overlay = other.row_overlay;
                col_index = other.col_index;
                can_mutate = other.can_mutate;
                owned_value_ = other.owned_value_;
                this->refresh_value();
            }

            return *this;
        }

        DataFrameCell& operator=(DataFrameCell&& other) noexcept {
            if (this != &other) {
                row = other.row;
                row_overlay = other.row_overlay;
                col_index = other.col_index;
                can_mutate = other.can_mutate;
                owned_value_ = std::move(other.owned_value_);
                this->refresh_value();
            }

            return *this;
        }

        DataFrameCell& operator=(csv::string_view value) {
            return this->assign(std::string(value));
        }

        /** Const-friendly read access for proxy use in column iteration and callbacks. */
        template<typename T = std::string>
        T get() const {
            return const_cast<DataFrameCell*>(this)->CSVField::template get<T>();
        }

        bool is_null() const noexcept {
            return const_cast<DataFrameCell*>(this)->CSVField::is_null();
        }

        bool is_str() const noexcept {
            return const_cast<DataFrameCell*>(this)->CSVField::is_str();
        }

        bool is_num() const noexcept {
            return const_cast<DataFrameCell*>(this)->CSVField::is_num();
        }

        bool is_int() const noexcept {
            return const_cast<DataFrameCell*>(this)->CSVField::is_int();
        }

        bool is_float() const noexcept {
            return const_cast<DataFrameCell*>(this)->CSVField::is_float();
        }

        DataType type() const noexcept {
            return const_cast<DataFrameCell*>(this)->CSVField::type();
        }

        template<typename T>
        bool try_get(T& out) const noexcept {
            return const_cast<DataFrameCell*>(this)->CSVField::template try_get<T>(out);
        }

    private:
        void refresh_value() {
            if (!row) {
                CSVField::operator=(CSVField(csv::string_view()));
                return;
            }

            if (row_overlay && row_overlay->try_get_copy(col_index, owned_value_)) {
                CSVField::operator=(CSVField(csv::string_view(owned_value_)));
                return;
            }

            owned_value_.clear();
            CSVField::operator=(CSVField((*row)[col_index].template get<csv::string_view>()));
        }

        DataFrameCell& assign(std::string stored) {
            if (!can_mutate || !row_overlay) {
                throw std::runtime_error(internals::ERROR_CANNOT_EDIT_CONST_DF_CELL);
            }

            owned_value_ = stored;
            const_cast<RowOverlay*>(row_overlay)->set(col_index, std::move(stored));
            CSVField::operator=(CSVField(csv::string_view(owned_value_)));
            return *this;
        }

        const CSVRow* row;
        const RowOverlay* row_overlay;
        size_t col_index;
        bool can_mutate;
        std::string owned_value_;
    };
}


namespace csv {
    template<typename KeyType>
    class DataFrame;
    template<typename KeyType>
    class DataFrameColumn;
    template<typename KeyType>
    class DataFrameRow;
}


#include <cstddef>
#include <iterator>

namespace csv {
    namespace internals {
        template<typename Owner, typename Proxy, typename Accessor>
        class indexed_proxy_iterator {
        public:
            using value_type = Proxy;
            using difference_type = std::ptrdiff_t;
            using pointer = const Proxy*;
            using reference = const Proxy&;
            using iterator_category = std::random_access_iterator_tag;

            indexed_proxy_iterator() = default;
            indexed_proxy_iterator(Owner* owner, size_t index, Accessor accessor = Accessor())
                : owner_(owner), index_(index), accessor_(accessor) {}

            reference operator*() const {
                cached_proxy_ = accessor_(owner_, index_);
                return cached_proxy_;
            }

            pointer operator->() const {
                operator*();
                return &cached_proxy_;
            }

            indexed_proxy_iterator& operator++() { ++index_; return *this; }
            indexed_proxy_iterator operator++(int) { auto tmp = *this; ++index_; return tmp; }
            indexed_proxy_iterator& operator--() { --index_; return *this; }
            indexed_proxy_iterator operator--(int) { auto tmp = *this; --index_; return tmp; }

            indexed_proxy_iterator operator+(difference_type n) const {
                return indexed_proxy_iterator(owner_, static_cast<size_t>(index_ + n), accessor_);
            }

            indexed_proxy_iterator operator-(difference_type n) const {
                return indexed_proxy_iterator(owner_, static_cast<size_t>(index_ - n), accessor_);
            }

            difference_type operator-(const indexed_proxy_iterator& other) const {
                return static_cast<difference_type>(index_) - static_cast<difference_type>(other.index_);
            }

            bool operator==(const indexed_proxy_iterator& other) const {
                return owner_ == other.owner_ && index_ == other.index_;
            }

            bool operator!=(const indexed_proxy_iterator& other) const {
                return !(*this == other);
            }

        private:
            Owner* owner_ = nullptr;
            size_t index_ = 0;
            Accessor accessor_;
            mutable Proxy cached_proxy_;
        };
    }
}


namespace csv {
    template<typename KeyType>
    class DataFrameColumn {
    public:
        struct cell_accessor {
            DataFrameCell operator()(const DataFrameColumn<KeyType>* owner, size_t row_index) const {
                return owner->operator[](row_index);
            }
        };

        using iterator = internals::indexed_proxy_iterator<const DataFrameColumn<KeyType>, DataFrameCell, cell_accessor>;
        using const_iterator = iterator;

        DataFrameColumn() : frame_(nullptr), mutable_frame_(nullptr), col_index_(0), generation_value_(0) {}

        DataFrameColumn(const DataFrame<KeyType>* frame, size_t col_index)
            : frame_(frame),
            mutable_frame_(nullptr),
            col_index_(col_index),
            generation_(frame ? frame->column_generation_ : std::shared_ptr<size_t>()),
            generation_value_(generation_ ? *generation_ : 0) {}

        DataFrameColumn(DataFrame<KeyType>* frame, size_t col_index)
            : frame_(frame),
            mutable_frame_(frame),
            col_index_(col_index),
            generation_(frame ? frame->column_generation_ : std::shared_ptr<size_t>()),
            generation_value_(generation_ ? *generation_ : 0) {}

        /** Column name. */
        const std::string& name() const {
            this->require_valid();
            return (*frame_->col_names_)[col_index_];
        }

        /** Zero-based column position. */
        size_t index() const noexcept {
            return col_index_;
        }

        /** Number of rows in the parent batch. */
        size_t size() const noexcept {
            if (!this->is_valid()) {
                return 0;
            }

            return frame_->size();
        }

        /** Whether the parent batch contains no rows. */
        bool empty() const noexcept {
            return this->size() == 0;
        }

        /** Access a visible cell value by row index. */
        DataFrameCell operator[](size_t row_index) const {
            this->require_valid();
            const auto& row = frame_->rows.at(row_index);
            const auto* row_edits = frame_->find_row_edits(row_index);
            return DataFrameCell(&row, row_edits, frame_->physical_column_index(col_index_));
        }

        /** Access a visible cell value as a string_view without materializing a DataFrameCell.
         *
         *  Intended for immediate read-only scans. If the value comes from the
         *  sparse edit overlay, the returned view points into overlay-owned
         *  storage and must not be retained across DataFrame mutation.
         */
        csv::string_view get_sv(size_t row_index) const {
            this->require_valid();
            const auto& row = frame_->rows.at(row_index);
            const auto* row_edits = frame_->find_row_edits(row_index);
            const size_t physical_index = frame_->physical_column_index(col_index_);
            csv::string_view edited_value;
            if (row_edits && row_edits->try_get_view(physical_index, edited_value)) {
                return edited_value;
            }

            return row[physical_index].template get<csv::string_view>();
        }

        /** Materialize this column as a vector of converted values. */
        template<typename T = std::string>
        std::vector<T> to_vector() const {
            std::vector<T> values;
            values.reserve(this->size());

            for (size_t row_index = 0; row_index < this->size(); ++row_index) {
                values.push_back((*this)[row_index].template get<T>());
            }

            return values;
        }

        /** Convert to a vector of strings. */
        operator std::vector<std::string>() const {
            return this->to_vector<std::string>();
        }

        /** Delete this column from the parent DataFrame.
         *
         *  Structural mutation invalidates outstanding row, column, and cell proxies.
         */
        bool erase() {
            this->require_valid();

            if (!mutable_frame_) {
                throw std::runtime_error("cannot erase column from const DataFrame");
            }

            return mutable_frame_->erase_column_at_index(col_index_);
        }

#ifdef CSV_HAS_CXX20
        /** Convert this DataFrameColumn into a std::ranges::input_range of strings.
         *
         *  @note Requires C++20 or later.
         */
        auto to_sv_range() const {
            return std::views::iota(size_t{0}, this->size())
                | std::views::transform([this](size_t row_index) {
                    return (*this)[row_index].template get<std::string>();
                });
        }
#endif

        /** Iterate over visible cells in this column. */
        iterator begin() const { return iterator(this, 0); }
        iterator end() const { return iterator(this, this->size()); }
        const_iterator cbegin() const { return const_iterator(this, 0); }
        const_iterator cend() const { return const_iterator(this, this->size()); }

    private:
        bool is_valid() const noexcept {
            return generation_ && *generation_ == generation_value_;
        }

        void require_valid() const {
            if (!this->is_valid()) {
                throw std::runtime_error("DataFrameColumn proxy is invalid after structural column mutation");
            }
        }

        const DataFrame<KeyType>* frame_;
        DataFrame<KeyType>* mutable_frame_;
        size_t col_index_;
        std::shared_ptr<size_t> generation_;
        size_t generation_value_;
    };
}


#include <thread>
#include <utility>


namespace csv {
    class DataFrameExecutor {
    public:
        explicit DataFrameExecutor(size_t worker_count = default_worker_count())
            : task_pool_(worker_count) {}

        DataFrameExecutor(const DataFrameExecutor&) = delete;
        DataFrameExecutor& operator=(const DataFrameExecutor&) = delete;

        ~DataFrameExecutor() {}

        size_t worker_count() const noexcept {
            return this->task_pool_.worker_count();
        }

        template<typename Fn>
        void parallel_for(size_t task_count, Fn&& fn) {
            if (task_count == 0) {
                return;
            }

#if CSV_ENABLE_THREADS
            const size_t workers = this->task_pool_.worker_count();
            if (workers <= 1 || task_count <= workers) {
                this->run_serial(task_count, std::forward<Fn>(fn));
                return;
            }

            this->task_pool_.parallel_for(task_count, [&fn](size_t, size_t task_index) {
                fn(task_index);
            });
#else
            this->run_serial(task_count, std::forward<Fn>(fn));
#endif
        }

    private:
        template<typename Fn>
        void run_serial(size_t task_count, Fn&& fn) {
            for (size_t i = 0; i < task_count; ++i) {
                fn(i);
            }
        }

        static size_t default_worker_count() {
#if CSV_ENABLE_THREADS
            const unsigned int hw = std::thread::hardware_concurrency();
            return hw > 0 ? static_cast<size_t>(hw) : 1;
#else
            return 0;
#endif
        }

        internals::parallel::IndexedTaskPool task_pool_;
    };
}


#include <string>

namespace csv {
    class DataFrameOptions {
    public:
        DataFrameOptions() = default;

        /** Policy for handling duplicate keys when creating a keyed DataFrame */
        enum class DuplicateKeyPolicy {
            THROW,      // Throw an error if a duplicate key is encountered
            OVERWRITE,  // Overwrite the existing value with the new value
            KEEP_FIRST  // Ignore the new value and keep the existing value
        };

        DataFrameOptions& set_duplicate_key_policy(DuplicateKeyPolicy value) {
            this->duplicate_key_policy = value;
            return *this;
        }

        DuplicateKeyPolicy get_duplicate_key_policy() const {
            return this->duplicate_key_policy;
        }

        DataFrameOptions& set_key_column(const std::string& value) {
            this->key_column = value;
            return *this;
        }

        const std::string& get_key_column() const {
            return this->key_column;
        }

        DataFrameOptions& set_throw_on_missing_key(bool value) {
            this->throw_on_missing_key = value;
            return *this;
        }

        bool get_throw_on_missing_key() const {
            return this->throw_on_missing_key;
        }

    private:
        std::string key_column;

        DuplicateKeyPolicy duplicate_key_policy = DuplicateKeyPolicy::OVERWRITE;

        /** Whether to throw an error if a key column value is missing when creating a keyed DataFrame */
        bool throw_on_missing_key = true;
    };

}


#include <stdexcept>
#include <string>
#include <vector>


namespace csv {
    template<typename KeyType>
    class DataFrameRow {
    public:
        /** Default constructor (creates an unbound proxy). */
        DataFrameRow() : row(nullptr), frame(nullptr), row_index(0), row_overlay(nullptr), key_ptr(nullptr), can_mutate(false) {}

        /** Construct a mutable DataFrameRow wrapper. */
        DataFrameRow(
            const CSVRow* _row,
            DataFrame<KeyType>* _frame,
            size_t _row_index,
            RowOverlay* _edits,
            const KeyType* _key
        ) : row(_row), frame(_frame), row_index(_row_index), row_overlay(_edits), key_ptr(_key), can_mutate(true) {}

        /** Construct a read-only DataFrameRow wrapper. */
        DataFrameRow(
            const CSVRow* _row,
            const DataFrame<KeyType>* _frame,
            size_t _row_index,
            const RowOverlay* _edits,
            const KeyType* _key
        ) : row(_row), frame(_frame), row_index(_row_index), row_overlay(_edits), key_ptr(_key), can_mutate(false) {}

        /** Access a field by column name, preserving edit support. */
        DataFrameCell operator[](const std::string& col) {
            return this->make_cell(this->find_column(col));
        }

        /** Access a field by position, preserving edit support. */
        DataFrameCell operator[](size_t n) {
            return this->make_cell(n);
        }

        /** Access a field by column name, checking edits first. */
        DataFrameCell operator[](const std::string& col) const {
            return this->make_cell(this->find_column(col));
        }

        /** Access a field by position, checking edits first. */
        DataFrameCell operator[](size_t n) const {
            return this->make_cell(n);
        }

        /** Get the number of visible fields in the row. */
        size_t size() const { return frame ? frame->n_cols() : row->size(); }

        /** Check if the row is empty. */
        bool empty() const { return this->size() == 0; }

        /** Get column names. */
        const std::vector<std::string>& get_col_names() const { return frame ? frame->columns() : row->get_col_names(); }

        /** Get the underlying CSVRow for compatibility. */
        const CSVRow& get_underlying_row() const { return *row; }

        /** Get the key for this row (only valid for keyed DataFrames). */
        const KeyType& key() const { return *key_ptr; }

        /** Delete this row from the parent DataFrame.
         *
         *  Structural mutation invalidates outstanding row and cell proxies.
         */
        bool erase() {
            if (!can_mutate || !frame) {
                throw std::runtime_error(internals::ERROR_CANNOT_ERASE_CONST_DF_ROW);
            }

            return const_cast<DataFrame<KeyType>*>(frame)->erase_at_index(row_index);
        }

        /** Convert to vector of strings for CSVWriter compatibility. */
        operator std::vector<std::string>() const {
            std::vector<std::string> result;
            result.reserve(this->size());
            
            for (size_t i = 0; i < this->size(); i++) {
                result.push_back(this->make_cell(i).template get<std::string>());
            }
            return result;
        }

        /** Convert to JSON. */
        std::string to_json(const std::vector<std::string>& subset = {}) const {
            const field_string_accessor field_at(this);
            if (frame) {
                return this->get_frame_json_converter().row_to_json(this->size(), field_at, subset);
            }

            return this->make_detached_json_converter().row_to_json(this->size(), field_at, subset);
        }

        /** Convert to JSON array. */
        std::string to_json_array(const std::vector<std::string>& subset = {}) const {
            const field_string_accessor field_at(this);
            if (frame) {
                return this->get_frame_json_converter().row_to_json_array(this->size(), field_at, subset);
            }

            return this->make_detached_json_converter().row_to_json_array(this->size(), field_at, subset);
        }

#ifdef CSV_HAS_CXX20
        /** Convert this DataFrameRow into a std::ranges::input_range of strings,
         *  respecting the sparse overlay (edited values take precedence).
         *
         *  @note Requires C++20 or later.
         */
        auto to_sv_range() const {
            return std::views::iota(size_t{0}, this->size())
                | std::views::transform([this](size_t i) { return this->make_cell(i).template get<std::string>(); });
        }
#endif

    private:
        struct field_string_accessor {
            explicit field_string_accessor(const DataFrameRow* owner) : owner(owner) {}

            std::string operator()(size_t i) const {
                return owner->make_cell(i).template get<std::string>();
            }

            const DataFrameRow* owner;
        };

        const internals::JsonConverter& get_frame_json_converter() const {
            return frame->json_converter_.get_or_create([this]() {
                return std::make_shared<internals::JsonConverter>(frame->columns());
            });
        }

        internals::JsonConverter make_detached_json_converter() const {
            return internals::JsonConverter(this->get_col_names());
        }

        DataFrameCell make_cell(size_t col_index) {
            if (frame) {
                col_index = frame->physical_column_index(col_index);
            }

            return can_mutate
                ? DataFrameCell(row, const_cast<RowOverlay*>(row_overlay), col_index)
                : DataFrameCell(row, row_overlay, col_index);
        }

        DataFrameCell make_cell(size_t col_index) const {
            if (frame) {
                col_index = frame->physical_column_index(col_index);
            }

            return DataFrameCell(row, row_overlay, col_index);
        }

        size_t find_column(const std::string& col) const {
            if (frame) {
                return frame->find_column(col);
            }

            const internals::ConstColNamesPtr col_names = row->col_names_ptr();
            const int position = col_names->index_of(col);
            if (position == CSV_NOT_FOUND) {
                internals::throw_column_not_found_out_of_range(col);
            }

            return static_cast<size_t>(position);
        }

        const CSVRow* row;
        const DataFrame<KeyType>* frame;
        size_t row_index;
        const RowOverlay* row_overlay;
        const KeyType* key_ptr;
        bool can_mutate;
    };

}


namespace csv {
    template<typename KeyType = std::string>
    class DataFrame {
    public:
        friend class DataFrameRow<KeyType>;
        friend class DataFrameColumn<KeyType>;
        using row_type = DataFrameRow<KeyType>;
        using column_type = DataFrameColumn<KeyType>;

        struct mutable_row_accessor {
            DataFrameRow<KeyType> operator()(DataFrame<KeyType>* owner, size_t row_index) const {
                return owner->make_row_proxy(row_index);
            }
        };

        struct const_row_accessor {
            DataFrameRow<KeyType> operator()(const DataFrame<KeyType>* owner, size_t row_index) const {
                return owner->make_const_row_proxy(row_index);
            }
        };

        /** Row-wise iterator over DataFrameRow entries. Provides access to rows with edit support. */
        using iterator = internals::indexed_proxy_iterator<DataFrame<KeyType>, DataFrameRow<KeyType>, mutable_row_accessor>;

        /** Row-wise const iterator over DataFrameRow entries. Provides read-only access to rows with edit support. */
        using const_iterator = internals::indexed_proxy_iterator<const DataFrame<KeyType>, DataFrameRow<KeyType>, const_row_accessor>;

        static_assert(
            internals::is_hashable<KeyType>::value,
            "DataFrame<KeyType> requires KeyType to be hashable (std::hash<KeyType> specialization required)."
        );

        static_assert(
            internals::is_equality_comparable<KeyType>::value,
            "DataFrame<KeyType> requires KeyType to be equality comparable (operator== required)."
        );

        static_assert(
            std::is_default_constructible<KeyType>::value,
            "DataFrame<KeyType> requires KeyType to be default-constructible."
        );

        using DuplicateKeyPolicy = DataFrameOptions::DuplicateKeyPolicy;

        /** Construct an empty DataFrame. */
        DataFrame() = default;

        /**
         * Construct an unkeyed DataFrame from a CSV reader.
         * Rows are accessible by position only.
         */
        explicit DataFrame(CSVReader& reader) {
            this->init_unkeyed_from_reader(reader);
        }

        /** Construct an unkeyed DataFrame from an existing batch of rows. */
        explicit DataFrame(std::vector<CSVRow> rows) {
            this->init_unkeyed_from_rows(rows);
        }

        /** Construct a keyed DataFrame from a CSV reader with options.
         *
         * @throws std::runtime_error if key column is empty or not found
         */
        explicit DataFrame(CSVReader& reader, const DataFrameOptions& options) {
            this->init_from_reader(reader, options);
        }

        /** Construct a keyed DataFrame directly from a CSV file.
         *
         * @throws std::runtime_error if key column is empty or not found
         */
        DataFrame(
            csv::string_view filename,
            const DataFrameOptions& options,
            CSVFormat format = CSVFormat::guess_csv()
        ) {
            CSVReader reader(filename, format);
            this->init_from_reader(reader, options);
        }

        /** Construct a keyed DataFrame using a column name as the key.
         *
         * @throws std::runtime_error if key column is empty or not found
         */
        DataFrame(
            CSVReader& reader,
            const std::string& _key_column,
            DuplicateKeyPolicy policy = DuplicateKeyPolicy::OVERWRITE,
            bool throw_on_missing_key = true
        ) : DataFrame(
            reader,
            DataFrameOptions()
                .set_key_column(_key_column)
                .set_duplicate_key_policy(policy)
                .set_throw_on_missing_key(throw_on_missing_key)
        ) {}

        /** Construct a keyed DataFrame using a custom key function.
         *
         * @throws std::runtime_error if policy is THROW and duplicate keys are encountered
         */
        template<
            typename KeyFunc,
            csv::enable_if_t<csv::is_invocable_returning<KeyFunc, KeyType, const CSVRow&>::value, int> = 0
        >
        DataFrame(
            CSVReader& reader,
            KeyFunc key_func,
            DuplicateKeyPolicy policy = DuplicateKeyPolicy::OVERWRITE
        ) : col_names_(reader.col_names_ptr()) {
            this->is_keyed = true;
            this->key_column_index_ = CSV_NOT_FOUND;
            this->physical_col_names_ = this->col_names_;
            this->build_from_key_function(reader, key_func, policy);
        }

        /** Construct a keyed DataFrame using a custom key function with options. */
        template<
            typename KeyFunc,
            csv::enable_if_t<csv::is_invocable_returning<KeyFunc, KeyType, const CSVRow&>::value, int> = 0
        >
        DataFrame(
            CSVReader& reader,
            KeyFunc key_func,
            const DataFrameOptions& options
        ) : DataFrame(reader, key_func, options.get_duplicate_key_policy()) {}

        /** Get the number of rows in the DataFrame. */
        size_t size() const noexcept {
            return rows.size();
        }

        /** Check if the DataFrame is empty (has no rows). */
        bool empty() const noexcept {
            return rows.empty();
        }

        /** Get the number of rows in the DataFrame. Alias for size(). */
        size_t n_rows() const noexcept { return rows.size(); }
        
        /** Get the number of visible columns in the DataFrame. */
        size_t n_cols() const noexcept { return col_names_->size(); }

        /** Check if a column exists in the DataFrame. */
        bool has_column(const std::string& name) const {
            return this->index_of(name) != CSV_NOT_FOUND;
        }

        /** Get the index of a column by name. */
        int index_of(const std::string& name) const {
            return this->col_names_->index_of(name);
        }

        /** Get the column names in order. */
        const std::vector<std::string>& columns() const noexcept { return this->col_names_->get_col_names(); }

        /** Insert a new row at the specified index. Structural mutation invalidates outstanding row and cell proxies. */
        void insert_row(size_t index, const std::vector<std::string>& row) {
            if (index > this->rows.size()) {
                throw std::out_of_range("DataFrame insert_row index out of range");
            }

            this->validate_inserted_row(row);
            std::vector<std::string> physical_row = this->expand_visible_row(row);
            CSVRow inserted_row = this->make_inserted_csv_row(physical_row, this->physical_col_names_);

            if (this->is_keyed) {
                if (this->key_column_index_ == CSV_NOT_FOUND) {
                    throw std::runtime_error("insert_row requires a column-keyed DataFrame");
                }

                KeyType key = inserted_row[static_cast<size_t>(this->key_column_index_)].template get<KeyType>();
                this->ensure_key_index();
                if (this->key_index->find(key) != this->key_index->end()) {
                    throw std::runtime_error(internals::ERROR_DUPLICATE_KEY);
                }

                this->keys_.insert(this->keys_.begin() + index, std::move(key));
            }

            this->rows.insert(this->rows.begin() + index, std::move(inserted_row));
            this->edits.insert(this->edits.begin() + index, RowOverlaySlot());
            this->invalidate_key_index();
        }

        /** Insert a new column with empty values. Structural mutation materializes visible rows into fresh row storage. */
        void insert_column(size_t index, const std::string& name) {
            this->insert_column(index, name, std::string());
        }

        /** Insert a new column with a default value. Structural mutation materializes visible rows into fresh row storage. */
        void insert_column(size_t index, const std::string& name, const std::string& default_value) {
            if (index > this->n_cols()) {
                throw std::out_of_range("DataFrame insert_column index out of range");
            }

            this->validate_inserted_column_name(name);

            std::vector<std::string> new_columns = this->columns();
            new_columns.insert(new_columns.begin() + index, name);

            std::stringstream serialized;
            auto writer = make_csv_writer(serialized);
            writer << new_columns;

            const DataFrame& self = *this;
            for (size_t row_index = 0; row_index < this->rows.size(); ++row_index) {
                std::vector<std::string> row_values = self.at(row_index);
                row_values.insert(row_values.begin() + index, default_value);
                writer << row_values;
            }

            this->replace_with_reparsed_column_insert(
                serialized.str(),
                this->col_names_->get_policy(),
                index
            );
        }

        /** Append a new column with empty values. */
        void append_column(const std::string& name) {
            this->insert_column(this->n_cols(), name);
        }

        /** Append a new column with a default value. */
        void append_column(const std::string& name, const std::string& default_value) {
            this->insert_column(this->n_cols(), name, default_value);
        }

        /** Build an unkeyed DataFrame containing rows whose corresponding mask entry is true.
         *
         *  CSVRow copies share the underlying parsed row storage, so this is intended for
         *  filtered document views that should avoid reparsing or rematerializing fields.
         */
        DataFrame selected_rows(const std::vector<std::uint8_t>& include_rows) const {
            if (include_rows.size() != this->rows.size()) {
                throw std::invalid_argument("selected row mask size must match DataFrame row count");
            }

            std::vector<CSVRow> selected;
            selected.reserve(this->rows.size());
            for (size_t row_index = 0; row_index < this->rows.size(); ++row_index) {
                if (include_rows[row_index]) {
                    selected.push_back(this->rows[row_index]);
                }
            }

            DataFrame result(std::move(selected));
            result.col_names_ = this->col_names_;
            result.physical_col_names_ = this->physical_col_names_;
            result.column_indices_ = this->column_indices_;
            return result;
        }

        /** Access a column view by position. */
        DataFrameColumn<KeyType> column_view(size_t col_index) {
            if (col_index >= this->n_cols()) {
                internals::throw_column_index_out_of_range();
            }

            return DataFrameColumn<KeyType>(this, col_index);
        }

        /** Access a column view by position. */
        DataFrameColumn<KeyType> column_view(size_t col_index) const {
            if (col_index >= this->n_cols()) {
                internals::throw_column_index_out_of_range();
            }

            return DataFrameColumn<KeyType>(this, col_index);
        }

        /** Access a column view by name. */
        DataFrameColumn<KeyType> column_view(const std::string& name) {
            return this->column_view(this->find_column(name));
        }

        /** Access a column view by name. */
        DataFrameColumn<KeyType> column_view(const std::string& name) const {
            return this->column_view(this->find_column(name));
        }

        /**
         * Access a row by position (unchecked).
         *
         * @note Disabled when KeyType is an integral type to prevent ambiguity with
         *       operator[](const KeyType&). Use at(size_t) for positional access
         *       on integer-keyed DataFrames.
         *
         * @throws std::out_of_range if index is out of bounds (via std::vector::at)
         */
        template<typename K = KeyType,
            csv::enable_if_t<!std::is_integral<K>::value, int> = 0>
        DataFrameRow<KeyType> operator[](size_t i) {
            static_assert(std::is_same<K, KeyType>::value,
                "Do not explicitly instantiate this template. Use at(size_t) for positional access.");
            return this->at(i);
        }

        /** Access a row by position (unchecked, const version).
         *  Disabled when KeyType is an integral type — use at(size_t) instead. */
        template<typename K = KeyType,
            csv::enable_if_t<!std::is_integral<K>::value, int> = 0>
        DataFrameRow<KeyType> operator[](size_t i) const {
            static_assert(std::is_same<K, KeyType>::value,
                "Do not explicitly instantiate this template. Use at(size_t) for positional access.");
            return this->at(i);
        }

        /**
         * Access a row by position with bounds checking.
         *
         * @throws std::out_of_range if index is out of bounds
         */
        DataFrameRow<KeyType> at(size_t i) {
            const auto& row = rows.at(i);
            auto* row_edits = this->ensure_row_edits(i);
            return DataFrameRow<KeyType>(&row, this, i, row_edits, this->key_ptr_at(i));
        }
        
        /** Access a row by position with bounds checking (const version). */
        DataFrameRow<KeyType> at(size_t i) const {
            const auto& row = rows.at(i);
            const RowOverlay* row_edits = this->find_row_edits(i);
            return DataFrameRow<KeyType>(&row, this, i, row_edits, this->key_ptr_at(i));
        }

        /**
         * Access a row by its key.
         *
         * @throws std::runtime_error if the DataFrame was not created with a key column
         * @throws std::out_of_range if the key is not found
         */
        DataFrameRow<KeyType> operator[](const KeyType& key) {
            this->require_keyed_frame();
            auto position = this->position_of(key);
            return DataFrameRow<KeyType>(&rows.at(position), this, position, this->ensure_row_edits(position), this->key_ptr_at(position));
        }

        /** Access a row by its key (const version). */
        DataFrameRow<KeyType> operator[](const KeyType& key) const {
            this->require_keyed_frame();
            auto position = this->position_of(key);
            const RowOverlay* row_edits = this->find_row_edits(position);
            return DataFrameRow<KeyType>(&rows.at(position), this, position, row_edits, this->key_ptr_at(position));
        }

        /**
         * Check if a key exists in the DataFrame.
         *
         * @throws std::runtime_error if the DataFrame was not created with a key column
         */
        bool contains(const KeyType& key) const {
            this->require_keyed_frame();
            this->ensure_key_index();
            return key_index->find(key) != key_index->end();
        }

        /**
         * Extract all values from a column with type conversion.
         * Accounts for edited values in the overlay.
         *
         * @tparam T Target type for conversion (default: std::string)
         * @throws std::out_of_range if column is not found
         */
        template<typename T = std::string>
        std::vector<T> column(const std::string& name) const {
            const size_t col_idx = this->find_column(name);
            std::vector<T> values;

            values.reserve(rows.size());
            for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
                values.push_back(this->at(row_index)[col_idx].template get<T>());
            }

            return values;
        }

        /** Apply a batch-oriented function to each column, potentially in parallel.
         *
         * The callback receives a lightweight column view plus a mutable per-column
         * state object from `states`.
         *
         * Callbacks may safely perform read-only access through the provided
         * column view and any explicit read-only references they already hold to
         * this batch-scoped DataFrame. Sparse-overlay cell edits through
         * `DataFrameRow` or `DataFrameCell` are synchronized at row granularity,
         * but structural mutations such as `erase()` are not thread-safe.
         *
         * @throws std::invalid_argument if `states.size() != n_cols()`
         */
        template<typename State, typename Fn>
        void column_parallel_apply(
            DataFrameExecutor& executor,
            std::vector<State>& states,
            Fn&& fn
        ) const {
            if (states.size() != this->n_cols()) {
                throw std::invalid_argument(internals::ERROR_COLUMN_APPLY_STATE_COUNT);
            }

            executor.parallel_for(this->n_cols(), [this, &states, &fn](size_t column_index) {
                fn(this->column_view(column_index), states[column_index]);
            });
        }

        /** Apply a batch-oriented function to a selected subset of columns, potentially in parallel.
         *
         * The callback receives a lightweight column view plus a mutable
         * per-selected-column state object from `states`.
         *
         * @throws std::invalid_argument if `states.size() != column_indices.size()`
         * @throws std::out_of_range if any column index is invalid
         */
        template<typename State, typename Fn>
        void column_parallel_apply(
            DataFrameExecutor& executor,
            const std::vector<size_t>& column_indices,
            std::vector<State>& states,
            Fn&& fn
        ) const {
            if (states.size() != column_indices.size()) {
                throw std::invalid_argument(internals::ERROR_COLUMN_APPLY_SUBSET_STATE_COUNT);
            }

            this->validate_selected_columns(column_indices);

            executor.parallel_for(column_indices.size(), [this, &column_indices, &states, &fn](size_t selected_index) {
                const size_t column_index = column_indices[selected_index];
                fn(this->column_view(column_index), states[selected_index]);
            });
        }

        /** Apply a read-only batch function to each column, potentially in parallel.
         *
         * This overload is for callers who do not need one explicit mutable
         * state object per column and prefer to manage any output storage
         * externally.
         */
        template<typename Fn>
        void column_parallel_apply(
            DataFrameExecutor& executor,
            Fn&& fn
        ) const {
            executor.parallel_for(this->n_cols(), [this, &fn](size_t column_index) {
                fn(this->column_view(column_index));
            });
        }

        /** Apply a read-only batch function to a selected subset of columns, potentially in parallel.
         *
         * This overload is for callers who want to process only specific
         * columns and prefer to manage any output storage externally.
         *
         * @throws std::out_of_range if any column index is invalid
         */
        template<typename Fn>
        void column_parallel_apply(
            DataFrameExecutor& executor,
            const std::vector<size_t>& column_indices,
            Fn&& fn
        ) const {
            this->validate_selected_columns(column_indices);

            executor.parallel_for(column_indices.size(), [this, &column_indices, &fn](size_t selected_index) {
                fn(this->column_view(column_indices[selected_index]));
            });
        }

        /**
         * Group row positions using an arbitrary grouping function.
         *
         * @tparam GroupFunc Callable that takes a DataFrameRow and returns a hashable key
         */
        template<
            typename GroupFunc,
            typename GroupKey = invoke_result_t<GroupFunc, DataFrameRow<KeyType>>,
            csv::enable_if_t<
                internals::is_hashable<GroupKey>::value &&
                internals::is_equality_comparable<GroupKey>::value,
                int
            > = 0
        >
        std::unordered_map<GroupKey, std::vector<size_t>> group_by(GroupFunc group_func) const {
            std::unordered_map<GroupKey, std::vector<size_t>> grouped;

            for (size_t i = 0; i < rows.size(); i++) {
                GroupKey group_key = group_func(this->at(i));
                grouped[group_key].push_back(i);
            }

            return grouped;
        }

        /**
         * Group row positions by the value of a column.
         *
         * @throws std::out_of_range if column is not found
         */
        std::unordered_map<std::string, std::vector<size_t>> group_by(const std::string& name) const {
            const size_t col_idx = this->find_column(name);
            std::unordered_map<std::string, std::vector<size_t>> grouped;

            for (size_t i = 0; i < rows.size(); i++) {
                grouped[this->at(i)[col_idx].template get<std::string>()].push_back(i);
            }

            return grouped;
        }

        /** Get iterator to the first row. */
        iterator begin() { return iterator(this, 0); }
        
        /** Get iterator past the last row. */
        iterator end() { return iterator(this, this->size()); }

        /** Get const iterator to the first row. */
        const_iterator begin() const { return const_iterator(this, 0); }

        /** Get const iterator past the last row. */
        const_iterator end() const { return const_iterator(this, this->size()); }

        /** Get const iterator to the first row (explicit). */
        const_iterator cbegin() const { return const_iterator(this, 0); }

        /** Get const iterator past the last row (explicit). */
        const_iterator cend() const { return const_iterator(this, this->size()); }

    private:
        /** Whether this DataFrame was created with a key. */
        bool is_keyed = false;
        
        /** Visible column names in order. */
        internals::ConstColNamesPtr col_names_ = std::make_shared<internals::ColNames>();

        /** Physical column names used by the underlying CSVRow storage. */
        internals::ConstColNamesPtr physical_col_names_ = col_names_;

        /** Maps visible column positions to physical CSVRow field positions. */
        std::vector<size_t> column_indices_;
        
        /** Internal storage for row data. */
        std::vector<CSVRow> rows;

        /** Stored keys for keyed DataFrames only. Empty for unkeyed frames. */
        std::vector<KeyType> keys_;

        /** Column index used for keyed frames constructed from a named column. */
        int key_column_index_ = CSV_NOT_FOUND;

        /** Lazily-built index mapping keys to row positions (mutable for const methods). */
        mutable std::unique_ptr<std::unordered_map<KeyType, size_t>> key_index;
        mutable internals::lazy_shared_ptr<internals::JsonConverter> json_converter_;
        std::shared_ptr<size_t> column_generation_{ new size_t(0) };

        /**
         * Sparse per-row edit overlays. Slots stay cheap at load time; the
         * heavier synchronized overlay map is allocated only for rows that are
         * actually edited.
         */
        std::vector<RowOverlaySlot> edits;
        std::shared_ptr<std::mutex> edits_creation_lock_{ new std::mutex() };

        /** Initialize an unkeyed DataFrame from a CSV reader. */
        void init_unkeyed_from_reader(CSVReader& reader) {
            this->assert_fresh_storage(false);
            this->is_keyed = false;
            this->col_names_ = reader.col_names_ptr();
            this->physical_col_names_ = this->col_names_;
            this->reset_visible_column_indices();

            std::vector<CSVRow> batch;
            while (reader.read_chunk(batch, dataframe_read_chunk_rows())) {
                this->append_unkeyed_batch(batch);
            }
        }

        /** Initialize an unkeyed DataFrame from an existing row batch. */
        void init_unkeyed_from_rows(std::vector<CSVRow>& source_rows) {
            this->assert_fresh_storage(false);
            this->is_keyed = false;
            this->col_names_ = source_rows.empty()
                ? internals::ConstColNamesPtr(std::make_shared<internals::ColNames>())
                : source_rows.front().col_names_ptr();
            this->physical_col_names_ = this->col_names_;
            this->reset_visible_column_indices();
            this->rows = std::move(source_rows);
            this->edits.resize(this->rows.size());
        }

        /** Initialize a keyed DataFrame from a CSV reader using column-based key extraction. */
        void init_from_reader(CSVReader& reader, const DataFrameOptions& options) {
            this->assert_fresh_storage(false);
            this->is_keyed = true;
            this->col_names_ = reader.col_names_ptr();
            this->physical_col_names_ = this->col_names_;
            const std::string key_column = options.get_key_column();

            if (key_column.empty())
                throw std::runtime_error(internals::ERROR_KEY_COLUMN_EMPTY);

            const int key_column_index = this->col_names_->index_of(key_column);
            if (key_column_index == CSV_NOT_FOUND)
                throw std::runtime_error(std::string(internals::ERROR_KEY_COLUMN_NOT_FOUND) + key_column);
            this->key_column_index_ = key_column_index;

            const bool throw_on_missing_key = options.get_throw_on_missing_key();

            this->build_from_key_function(
                reader,
                [key_column, throw_on_missing_key](const CSVRow& row) -> KeyType {
                    try {
                        return row[key_column].template get<KeyType>();
                    }
                    catch (const std::exception& e) {
                        if (throw_on_missing_key) {
                            throw std::runtime_error(internals::ERROR_KEY_COLUMN_VALUE + std::string(e.what()));
                        }

                        return KeyType();
                    }
                },
                options.get_duplicate_key_policy()
            );
        }

        /** Build keyed DataFrame using a custom key extraction function. */
        template<typename KeyFunc>
        void build_from_key_function(
            CSVReader& reader,
            KeyFunc key_func,
            DuplicateKeyPolicy policy
        ) {
            std::unordered_map<KeyType, size_t> key_to_pos;
            this->assert_fresh_storage(true);
            this->reset_visible_column_indices();

            std::vector<CSVRow> batch;
            while (reader.read_chunk(batch, dataframe_read_chunk_rows())) {
                this->append_keyed_batch(batch, key_func, policy, key_to_pos);
            }
        }

        static size_t dataframe_read_chunk_rows() noexcept {
            return 50000;
        }

        template<typename Container>
        static void reserve_for_append(Container& container, size_t additional) {
            if (additional == 0) {
                return;
            }

            const size_t required = container.size() + additional;
            if (required <= container.capacity()) {
                return;
            }

            const size_t current = container.capacity();
            // Do not reserve exactly one batch ahead. DataFrame construction
            // appends fixed-size read_chunk() batches, so exact reserves would
            // force a reallocation on every batch for large inputs.
            size_t next = current == 0 ? additional : current * 2;
            if (next < required || next < current) {
                next = required;
            }

            container.reserve(next);
        }

        void append_unkeyed_batch(std::vector<CSVRow>& batch) {
            reserve_for_append(rows, batch.size());
            reserve_for_append(edits, batch.size());

            for (auto& row : batch) {
                rows.push_back(std::move(row));
                edits.emplace_back();
            }
        }

        void reset_visible_column_indices() {
            this->column_indices_.clear();
            this->column_indices_.reserve(this->col_names_->size());
            for (size_t i = 0; i < this->col_names_->size(); ++i) {
                this->column_indices_.push_back(i);
            }
        }

        size_t physical_n_cols() const noexcept {
            return this->physical_col_names_ ? this->physical_col_names_->size() : 0;
        }

        size_t physical_column_index(size_t logical_col_index) const {
            return this->column_indices_.at(logical_col_index);
        }

        template<typename KeyFunc>
        void append_keyed_batch(
            std::vector<CSVRow>& batch,
            KeyFunc& key_func,
            DuplicateKeyPolicy policy,
            std::unordered_map<KeyType, size_t>& key_to_pos
        ) {
            reserve_for_append(rows, batch.size());
            reserve_for_append(keys_, batch.size());
            reserve_for_append(edits, batch.size());

            for (auto& row : batch) {
                KeyType key = key_func(row);

                auto existing = key_to_pos.find(key);
                if (existing != key_to_pos.end()) {
                    if (policy == DuplicateKeyPolicy::THROW)
                        throw std::runtime_error(internals::ERROR_DUPLICATE_KEY);

                    if (policy == DuplicateKeyPolicy::OVERWRITE)
                        rows[existing->second] = std::move(row);

                    continue;
                }

                rows.push_back(std::move(row));
                keys_.push_back(key);
                edits.emplace_back();
                key_to_pos[key] = rows.size() - 1;
            }
        }

        /** Find the index of a column by name. Throws if the column is not found. */
        size_t find_column(const std::string& name) const {
            return index_of(name) != CSV_NOT_FOUND ? static_cast<size_t>(index_of(name)) :
                throw std::out_of_range(std::string(internals::ERROR_COLUMN_NOT_FOUND) + name);
        }

        /** Return the overlay for a specific row index. */
        const RowOverlay* find_row_edits(size_t row_index) const {
            return edits.at(row_index).get();
        }

        /** Return the row overlay, allocating it only when mutable access needs it. */
        RowOverlay* ensure_row_edits(size_t row_index) {
            RowOverlaySlot& slot = edits.at(row_index);
            RowOverlay* overlay = slot.get();
            if (overlay) {
                return overlay;
            }

            std::lock_guard<std::mutex> lock(*edits_creation_lock_);
            return slot.ensure();
        }

        DataFrameRow<KeyType> make_row_proxy(size_t row_index) {
            const auto& row = rows.at(row_index);
            return DataFrameRow<KeyType>(&row, this, row_index, this->ensure_row_edits(row_index), this->key_ptr_at(row_index));
        }

        DataFrameRow<KeyType> make_const_row_proxy(size_t row_index) const {
            const auto& row = rows.at(row_index);
            return DataFrameRow<KeyType>(&row, this, row_index, this->find_row_edits(row_index), this->key_ptr_at(row_index));
        }

        void erase_row_edits(size_t row_index) {
            if (row_index < edits.size()) {
                edits.erase(edits.begin() + row_index);
            }
        }

        static CSVRow make_inserted_csv_row(
            const std::vector<std::string>& row,
            internals::ConstColNamesPtr col_names
        ) {
            internals::RawCSVDataPtr data = std::make_shared<internals::RawCSVData>();
            data->col_names = std::const_pointer_cast<internals::ColNames>(col_names);

            size_t total_bytes = 0;
            for (const auto& field : row) {
                total_bytes += field.size();
            }
            data->quote_arena.reserve_for_source_size(total_bytes);
            data->fields.reserve_for_source_size(row.size());

            for (const auto& field : row) {
                const size_t offset = data->quote_arena.append(csv::string_view(field));
                data->fields.emplace_back(offset, field.size(), true);
            }

            return CSVRow(data, 0, 0, row.size());
        }

        void validate_inserted_row(const std::vector<std::string>& row) const {
            if (this->n_cols() == 0 && !row.empty()) {
                throw std::invalid_argument("cannot insert a non-empty row into a DataFrame without columns");
            }

            if (this->col_names_ && !this->col_names_->empty() && row.size() != this->n_cols()) {
                throw std::invalid_argument("inserted row field count must match DataFrame column count");
            }
        }

        std::vector<std::string> expand_visible_row(const std::vector<std::string>& row) const {
            if (this->column_indices_.empty() || this->physical_n_cols() == this->n_cols()) {
                return row;
            }

            std::vector<std::string> physical_row(this->physical_n_cols());
            for (size_t i = 0; i < row.size(); ++i) {
                physical_row[this->physical_column_index(i)] = row[i];
            }

            return physical_row;
        }

        void validate_inserted_column_name(const std::string& name) const {
            if (name.empty()) {
                throw std::invalid_argument("inserted column name must not be empty");
            }

            if (this->has_column(name)) {
                throw std::invalid_argument("inserted column name must not duplicate an existing column");
            }
        }

        std::vector<KeyType> build_keys_from_rows(
            const std::vector<CSVRow>& source_rows,
            size_t key_column_index
        ) const {
            std::vector<KeyType> rebuilt_keys;
            std::unordered_map<KeyType, size_t> seen_keys;
            rebuilt_keys.reserve(source_rows.size());

            for (size_t row_index = 0; row_index < source_rows.size(); ++row_index) {
                KeyType key = source_rows[row_index][key_column_index].template get<KeyType>();
                auto inserted = seen_keys.emplace(key, row_index);
                if (!inserted.second) {
                    throw std::runtime_error(internals::ERROR_DUPLICATE_KEY);
                }

                rebuilt_keys.push_back(std::move(key));
            }

            return rebuilt_keys;
        }

        void replace_with_reparsed_column_insert(
            const std::string& serialized,
            ColumnNamePolicy column_name_policy,
            size_t inserted_column_index
        ) {
            const bool was_keyed = this->is_keyed;
            const int old_key_column_index = this->key_column_index_;
            int new_key_column_index = CSV_NOT_FOUND;
            if (old_key_column_index != CSV_NOT_FOUND) {
                const auto key_position = std::find(
                    this->column_indices_.begin(),
                    this->column_indices_.end(),
                    static_cast<size_t>(old_key_column_index)
                );
                if (key_position == this->column_indices_.end()) {
                    throw std::runtime_error("key column is not visible");
                }

                const size_t key_column_index = static_cast<size_t>(
                    std::distance(this->column_indices_.begin(), key_position)
                );
                new_key_column_index = static_cast<int>(
                    inserted_column_index <= key_column_index
                        ? key_column_index + 1
                        : key_column_index
                );
            }

            CSVFormat format;
            format.delimiter(',')
                .header_row(0)
                .column_names_policy(column_name_policy);

            std::istringstream input(serialized);
            CSVReader reader(input, format);
            DataFrame<KeyType> rebuilt(reader);

            std::vector<KeyType> rebuilt_keys;
            if (was_keyed && new_key_column_index != CSV_NOT_FOUND) {
                rebuilt_keys = this->build_keys_from_rows(
                    rebuilt.rows,
                    static_cast<size_t>(new_key_column_index)
                );
            }

            this->col_names_ = rebuilt.col_names_;
            this->physical_col_names_ = rebuilt.col_names_;
            this->reset_visible_column_indices();
            this->rows = std::move(rebuilt.rows);
            this->edits.clear();
            this->edits.resize(this->rows.size());

            if (was_keyed) {
                this->is_keyed = true;
                this->key_column_index_ = new_key_column_index;
                if (new_key_column_index != CSV_NOT_FOUND) {
                    this->keys_ = std::move(rebuilt_keys);
                }
            }
            else {
                this->is_keyed = false;
                this->key_column_index_ = CSV_NOT_FOUND;
                this->keys_.clear();
            }

            this->invalidate_key_index();
            this->invalidate_column_proxies();
            this->invalidate_json_converter();
        }

        bool erase_column_at_index(size_t column_index) {
            if (column_index >= this->n_cols()) {
                return false;
            }

            const size_t physical_index = this->physical_column_index(column_index);
            if (this->is_keyed && this->key_column_index_ != CSV_NOT_FOUND &&
                physical_index == static_cast<size_t>(this->key_column_index_)) {
                throw std::runtime_error("cannot erase key column from DataFrame");
            }

            std::vector<std::string> names = this->columns();
            names.erase(names.begin() + column_index);

            internals::ColNamesPtr visible_names(new internals::ColNames());
            visible_names->set_policy(this->col_names_->get_policy());
            visible_names->set_col_names(names);

            this->col_names_ = visible_names;
            this->column_indices_.erase(this->column_indices_.begin() + column_index);
            this->invalidate_column_proxies();
            this->invalidate_json_converter();
            return true;
        }

        bool erase_at_index(size_t row_index) {
            if (row_index >= rows.size()) {
                return false;
            }

            this->erase_row_edits(row_index);
            rows.erase(rows.begin() + row_index);
            if (this->is_keyed) {
                keys_.erase(keys_.begin() + row_index);
            }
            this->invalidate_key_index();
            return true;
        }

        void validate_selected_columns(const std::vector<size_t>& column_indices) const {
            for (size_t column_index : column_indices) {
                if (column_index >= this->n_cols()) {
                    throw std::out_of_range(internals::ERROR_COLUMN_APPLY_INVALID_INDEX);
                }
            }
        }

        /** Validate that this DataFrame was created with a key column. */
        void require_keyed_frame() const {
            if (!is_keyed)
                throw std::runtime_error(internals::ERROR_UNKEYED_DATA_FRAME);
        }

        /** Invalidate the lazy key index after structural changes. */
        void invalidate_key_index() {
            key_index.reset();
        }

        /** Invalidate cached JSON column mapping after structural column changes. */
        void invalidate_json_converter() {
            json_converter_ = internals::lazy_shared_ptr<internals::JsonConverter>();
        }

        /** Invalidate outstanding column proxies after structural column changes. */
        void invalidate_column_proxies() {
            ++(*column_generation_);
        }

        /** Debug-only check that constructor helpers are starting from a pristine batch state. */
        void assert_fresh_storage(bool expected_is_keyed) const {
            CSV_DEBUG_ASSERT(this->rows.empty());
            CSV_DEBUG_ASSERT(this->keys_.empty());
            CSV_DEBUG_ASSERT(this->edits.empty());
            CSV_DEBUG_ASSERT(this->column_indices_.empty());
            CSV_DEBUG_ASSERT(this->key_index.get() == nullptr);
            CSV_DEBUG_ASSERT(this->json_converter_.get() == nullptr);
            CSV_DEBUG_ASSERT(this->is_keyed == expected_is_keyed);
        }

        /** Build the key index if it doesn't exist (lazy initialization). */
        void ensure_key_index() const {
            if (key_index) return;

            key_index = std::unique_ptr<std::unordered_map<KeyType, size_t>>(
                new std::unordered_map<KeyType, size_t>()
            );

            for (size_t i = 0; i < rows.size(); i++) {
                (*key_index)[keys_[i]] = i;
            }
        }

        /** Find the position of a key in the rows vector. */
        size_t position_of(const KeyType& key) const {
            this->ensure_key_index();
            auto it = key_index->find(key);
            return it == key_index->end() ? throw std::out_of_range(internals::ERROR_KEY_NOT_FOUND)
                : it->second;
        }

        const KeyType* key_ptr_at(size_t row_index) const {
            return this->is_keyed ? &keys_.at(row_index) : nullptr;
        }
    };

#ifdef CSV_HAS_CXX20
    static_assert(
        internals::csv_write_rows_input_range<DataFrame<>>,
        "DataFrame must remain compatible with csv::DelimWriter::write_rows()."
    );
#endif
}



#include <algorithm>
#include <cstring>
#include <istream>


namespace csv {
    namespace internals {
        /**
         * streambuf adapter over csv::string_view with no data copy.
         *
         * The underlying memory must remain valid and immutable for the entire
         * lifetime of this stream object.
         */
        class StringViewStreamBuf : public std::streambuf {
        public:
            explicit StringViewStreamBuf(csv::string_view view) {
                char* begin = const_cast<char*>(view.data());
                char* end = begin + view.size();
                this->setg(begin, begin, end);
            }

        protected:
            std::streamsize xsgetn(char* s, std::streamsize count) override {
                const std::streamsize avail = this->egptr() - this->gptr();
                const std::streamsize n = std::min(avail, count);
                if (n > 0) {
                    std::memcpy(s, this->gptr(), static_cast<size_t>(n));
                    this->gbump(static_cast<int>(n));
                }
                return n;
            }

            pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                std::ios_base::openmode which = std::ios_base::in) override {
                if (!(which & std::ios_base::in)) {
                    return pos_type(off_type(-1));
                }

                const auto begin = this->eback();
                const auto curr = this->gptr();
                const auto end = this->egptr();

                off_type base = 0;
                if (dir == std::ios_base::beg) {
                    base = 0;
                }
                else if (dir == std::ios_base::cur) {
                    base = static_cast<off_type>(curr - begin);
                }
                else if (dir == std::ios_base::end) {
                    base = static_cast<off_type>(end - begin);
                }
                else {
                    return pos_type(off_type(-1));
                }

                const off_type next = base + off;
                const off_type size = static_cast<off_type>(end - begin);
                if (next < 0 || next > size) {
                    return pos_type(off_type(-1));
                }

                this->setg(begin, begin + next, end);
                return pos_type(next);
            }

            pos_type seekpos(pos_type pos,
                std::ios_base::openmode which = std::ios_base::in) override {
                return this->seekoff(off_type(pos), std::ios_base::beg, which);
            }
        };

        /**
         * Lightweight istream over csv::string_view with zero copy.
         *
         * WARNING: The caller is responsible for ensuring the backing memory
         * outlives this stream and any CSVReader parsing it.
         */
        class StringViewStream : public std::istream {
        public:
            explicit StringViewStream(csv::string_view view)
                : std::istream(nullptr), _buf(view) {
                this->rdbuf(&_buf);
            }

            StringViewStream(const StringViewStream&) = delete;
            StringViewStream(StringViewStream&&) = delete;
            StringViewStream& operator=(const StringViewStream&) = delete;
            StringViewStream& operator=(StringViewStream&&) = delete;

        private:
            StringViewStreamBuf _buf;
        };
    }
}


#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace csv {
    /** Returned by get_file_info() */
    struct CSVFileInfo {
        std::string filename;               /**< Filename */
        std::vector<std::string> col_names; /**< CSV column names */
        char delim;                         /**< Delimiting character */
        size_t n_rows;                      /**< Number of rows in a file */
        size_t n_cols;                      /**< Number of columns in a CSV */
        size_t parse_worker_count;          /**< Number of parser worker threads used */
        internals::SpeculativeParseDiagnostics speculative_diagnostics;
    };

    /** @name Shorthand Parsing Functions
     *  @brief Convenience functions for parsing small strings
     */
     ///@{

    /** Parse CSV from a string view, copying the input into an owned buffer.
     *
     *  Safe for any string_view regardless of the caller's ownership of the
     *  underlying memory.
     *
     *  @par Example
     *  @snippet tests/test_read_csv.cpp Parse Example
     */
    inline CSVReader parse(csv::string_view in, const CSVFormat& format = CSVFormat::guess_csv()) {
        std::unique_ptr<std::istream> ss(new std::stringstream(std::string(in)));
        return CSVReader(std::move(ss), format);
    }

    /** Parse CSV from an in-memory view with zero copy.
     *
     *  WARNING: Non-owning path. The caller must ensure `in`'s backing memory
     *  remains valid and immutable while the reader may request additional rows
     *  from the source stream.
     *
     *  Rows already obtained from the reader remain valid, but unread rows
     *  still depend on the source view staying alive.
     */
    inline CSVReader parse_unsafe(csv::string_view in, CSVFormat format = CSVFormat::guess_csv()) {
        std::unique_ptr<std::istream> stream(new internals::StringViewStream(in));
        return CSVReader(std::move(stream), format);
    }

    /** Parses a CSV string with no headers. */
    inline CSVReader parse_no_header(csv::string_view in) {
        CSVFormat format;
        format.header_row(-1);
        return parse(in, format);
    }

    /** Parse a RFC 4180 CSV string.
     *
     *  String literals have static storage duration, so the zero-copy path is
     *  safe here.
     *
     *  @par Example
     *  @snippet tests/test_read_csv.cpp Escaped Comma
     */
    inline CSVReader operator ""_csv(const char* in, size_t n) {
        return parse_unsafe(csv::string_view(in, n));
    }

    /** A shorthand for csv::parse_no_header().
     *
     *  String literals have static storage duration, so the zero-copy path is
     *  safe here.
     */
    inline CSVReader operator ""_csv_no_header(const char* in, size_t n) {
        CSVFormat format;
        format.header_row(-1);
        return parse_unsafe(csv::string_view(in, n), format);
    }
    ///@}

    /** @name Utility Functions */
    ///@{
    /** Infer SQL-friendly column data types from an existing CSVReader.
     *
     *  This consumes rows from `reader` using the chunked ETL path and returns
     *  one inferred `DataType` per column name.
     */
    std::unordered_map<std::string, DataType> csv_data_types(CSVReader& reader);

    /** Infer SQL-friendly column data types from any CSVReader constructor input.
     *
     *  This convenience overload forwards its arguments directly to
     *  `CSVReader`, so it supports filenames, `std::istream` sources, owned
     *  streams, and custom `CSVFormat` combinations without additional wrapper
     *  code.
     *
     *  @par Example
     *  @code
     *  std::istringstream input("name,age\nAlice,30\nBob,41\n");
     *  CSVFormat format;
     *  format.delimiter(',').header_row(0);
     *
     *  auto dtypes = csv::csv_data_types(input, format);
     *  @endcode
     */
    template<
        typename... ReaderArgs,
        csv::enable_if_t<std::is_constructible<CSVReader, ReaderArgs...>::value, int> = 0
    >
    inline std::unordered_map<std::string, DataType> csv_data_types(ReaderArgs&&... reader_args) {
        CSVReader reader(std::forward<ReaderArgs>(reader_args)...);
        return csv_data_types(reader);
    }

    /** Apply a per-column batch function over a CSVReader using a reusable executor.
     *
     *  Reads the source in chunks, promotes each chunk into a temporary DataFrame,
     *  and applies `fn(column, states[column.index()])`.
     *
     *  Callbacks may treat each batch DataFrame as read-mostly, and sparse
     *  overlay cell edits are synchronized at row granularity. If you need more
     *  involved batch orchestration, use `CSVReader::read_chunk()` and construct
     *  a batch-scoped `DataFrame` yourself.
     *
     *  @throws std::invalid_argument if `chunk_size == 0`
     */
    template<typename State, typename Fn>
    inline void chunk_parallel_apply(
        CSVReader& reader,
        DataFrameExecutor& executor,
        std::vector<State>& states,
        Fn&& fn,
        size_t chunk_size = 50000
    ) {
        if (chunk_size == 0) {
            throw std::invalid_argument(internals::ERROR_CHUNK_PARALLEL_APPLY_ZERO);
        }

        std::vector<CSVRow> rows;
        while (reader.read_chunk(rows, chunk_size)) {
            DataFrame<> batch(std::move(rows));
            batch.column_parallel_apply(executor, states, std::forward<Fn>(fn));
        }
    }

    /** Apply a per-column batch function over a CSVReader with a temporary executor.
     *
     *  This is the convenience overload for the common case where callers do not
     *  need to reuse worker threads across multiple reader pipelines.
     */
    template<typename State, typename Fn>
    inline void chunk_parallel_apply(
        CSVReader& reader,
        std::vector<State>& states,
        Fn&& fn,
        size_t chunk_size = 50000
    ) {
        DataFrameExecutor executor;
        chunk_parallel_apply(reader, executor, states, std::forward<Fn>(fn), chunk_size);
    }

    /** Get basic information about a CSV file
     *  @include programs/csv_info.cpp
     */
    inline CSVFileInfo get_file_info(const std::string& filename) {
        CSVFormat reader_format = CSVFormat::guess_csv();

        CSVReader reader(filename, reader_format);
        CSVFormat format = reader.get_format();

        std::vector<CSVRow> rows;
        while (reader.read_chunk(rows, 50000)) {}

        return {
            filename,
            reader.get_col_names(),
            format.get_delim(),
            reader.n_rows(),
            reader.get_col_names().size(),
            reader.parse_worker_count(),
            reader.speculative_diagnostics()
        };
    }

    /** Get the column names of a CSV file using just the first 500KB. */
    inline std::vector<std::string> get_col_names(
        csv::string_view filename,
        const CSVFormat& format = CSVFormat::guess_csv()) {
        auto head = internals::parser::get_csv_head(filename);
        return parse_unsafe(head, format).get_col_names();
    }

    /** Find the position of a column in a CSV file or CSV_NOT_FOUND otherwise. */
    inline long long get_col_pos(csv::string_view filename, csv::string_view col_name,
        const CSVFormat& format = CSVFormat::guess_csv()) {
        auto col_names = get_col_names(filename, format);
        return col_names.empty() ? CSV_NOT_FOUND :
            std::distance(col_names.begin(), std::find(col_names.begin(), col_names.end(), col_name));
    }
    ///@}
}


#include <algorithm>
#include <cctype>

namespace csv {
    namespace internals {
        CSV_INLINE const std::vector<std::string>& ColNames::get_col_names() const noexcept {
            return this->col_names;
        }

        CSV_INLINE void ColNames::set_col_names(const std::vector<std::string>& cnames) {
            this->col_names = cnames;
            this->col_pos.clear();

            for (size_t i = 0; i < cnames.size(); i++) {
                if (this->_policy == csv::ColumnNamePolicy::CASE_INSENSITIVE) {
                    // For case-insensitive lookup, cache a lowercase version
                    // of the column name in the map
                    std::string lower(cnames[i]);
                    std::transform(lower.begin(), lower.end(), lower.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    this->col_pos[lower] = i;
                } else {
                    this->col_pos[cnames[i]] = i;
                }
            }
        }

        CSV_INLINE int ColNames::index_of(csv::string_view col_name) const {
            if (this->_policy == csv::ColumnNamePolicy::CASE_INSENSITIVE) {
                std::string lower(col_name);
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                auto pos = this->col_pos.find(lower);
                return (pos != this->col_pos.end()) ? (int)pos->second : CSV_NOT_FOUND;
            }

            auto pos = this->col_pos.find(std::string(col_name));
            return (pos != this->col_pos.end()) ? (int)pos->second : CSV_NOT_FOUND;
        }

        CSV_INLINE void ColNames::set_policy(csv::ColumnNamePolicy policy) {
            this->_policy = policy;
        }

        CSV_INLINE csv::ColumnNamePolicy ColNames::get_policy() const noexcept {
            return this->_policy;
        }

        CSV_INLINE size_t ColNames::size() const noexcept {
            return this->col_names.size();
        }

        CSV_INLINE const std::string& ColNames::operator[](size_t i) const {
            if (i >= this->col_names.size())
                throw_column_index_out_of_bounds();

            return this->col_names[i];
        }
    }
}

/** @file
 *  Defines an object used to store CSV format settings
 */

#include <algorithm>
#include <set>


namespace csv {
    CSV_INLINE CSVFormat& CSVFormat::delimiter(char delim) {
        this->possible_delimiters = { delim };
        this->assert_no_char_overlap();
        return *this;
    }

    CSV_INLINE CSVFormat& CSVFormat::delimiter(const std::vector<char> & delim) {
        this->possible_delimiters = delim;
        this->assert_no_char_overlap();
        return *this;
    }

    CSV_INLINE CSVFormat& CSVFormat::quote(char quote) {
        this->no_quote = false;
        this->quote_char = quote;
        this->assert_no_char_overlap();
        return *this;
    }

    CSV_INLINE CSVFormat& CSVFormat::trim(const std::vector<char> & chars) {
        this->trim_chars = chars;
        this->assert_no_char_overlap();
        return *this;
    }

    CSV_INLINE CSVFormat& CSVFormat::column_names(const std::vector<std::string>& names) {
        this->col_names = names;
        this->header = -1;
        this->col_names_explicitly_set_ = true;
        return *this;
    }

    CSV_INLINE CSVFormat& CSVFormat::header_row(int row) {
        if (row < 0) this->variable_column_policy = VariableColumnPolicy::KEEP;

        this->header = row;
        this->header_explicitly_set_ = true;
        this->col_names = {};
        this->col_names_explicitly_set_ = false;
        return *this;
    }

    CSV_INLINE CSVFormat& CSVFormat::chunk_size(size_t size) {
        if (size < internals::CSV_CHUNK_SIZE_FLOOR) {
            throw std::invalid_argument(internals::make_chunk_size_error(internals::CSV_CHUNK_SIZE_FLOOR, size));
        }
        const size_t max_chunk_size = internals::CSV_CHUNK_SIZE_MAX;
        if (size > max_chunk_size) {
            throw std::invalid_argument(internals::make_chunk_size_ceiling_error(max_chunk_size, size));
        }
        this->_chunk_size = size;
        return *this;
    }

    CSV_INLINE void CSVFormat::assert_no_char_overlap()
    {
        const std::set<char> delims(this->possible_delimiters.begin(), this->possible_delimiters.end());
        const std::set<char> trims(this->trim_chars.begin(), this->trim_chars.end());
        std::set<char> offenders;

        for (std::set<char>::const_iterator it = delims.begin(); it != delims.end(); ++it) {
            if (trims.find(*it) != trims.end()) {
                offenders.insert(*it);
            }
        }

        // Make sure quote character is not contained in possible delimiters
        // or whitespace characters.
        if (delims.find(this->quote_char) != delims.end() ||
            trims.find(this->quote_char) != trims.end()) {
            offenders.insert(this->quote_char);
        }

        if (!offenders.empty()) {
            throw std::runtime_error(internals::make_char_overlap_error(offenders));
        }
    }
}

/** @file
 *  @brief Defines functionality needed for basic CSV parsing
 */


namespace csv {
#ifdef _MSC_VER
#pragma region Reading helpers
#endif
    CSV_INLINE bool CSVReader::check_for_rows() {
        if (!this->records->empty()) return true;

        if (this->read_scheduler_.wait_if_active(
            [this] { return this->records->is_waitable(); },
            [this] { this->records->wait(); }
        )) {
            return true;
        }

        this->read_scheduler_.join();
        this->read_scheduler_.rethrow_exception_if_any();

        if (this->parser->eof()) return false;

        if (this->_read_requested && this->records->empty()) {
            internals::throw_row_too_large_for_chunk(this->_chunk_size);
        }

        this->read_scheduler_.run(
            [this] { this->read_csv(this->_chunk_size); },
            [this] { this->records->notify_all(); }
        );
        this->read_scheduler_.rethrow_exception_if_any();
        this->_read_requested = true;
        return true;
    }

#ifdef _MSC_VER
#pragma endregion Reading helpers
#endif

#ifdef _MSC_VER
#pragma region Format and header helpers
#endif
    CSV_INLINE void CSVReader::init_parser(
        std::unique_ptr<internals::parser::CSVParserDriverBase> parser_impl
    ) {
        auto resolved = parser_impl->get_resolved_format();
        this->_format = resolved.format;
        this->_chunk_size = this->_format.get_chunk_size();
        this->n_cols = resolved.n_cols;

        if (!this->_format.col_names.empty()) {
            this->set_col_names(this->_format.col_names);
        }

        this->parser = std::move(parser_impl);
        this->initial_read();
    }

    CSV_INLINE CSVFormat CSVReader::get_format() const {
        CSVFormat new_format = this->_format;

        // Since users are normally not allowed to set
        // column names and header row simulatenously,
        // we will set the backing variables directly here
        new_format.col_names = this->col_names->get_col_names();
        new_format.header = this->_format.header;

        return new_format;
    }

    CSV_INLINE void CSVReader::trim_header() {
        if (!this->header_trimmed) {
            for (int i = 0; i <= this->_format.header && !this->records->empty(); i++) {
                if (i == this->_format.header && this->col_names->empty()) {
                    this->set_col_names(this->records->pop_front());
                }
                else {
                    this->records->pop_front();
                }
            }

            this->header_trimmed = true;
        }
    }

    /** Install the active column names for this reader. */
    CSV_INLINE void CSVReader::set_col_names(const std::vector<std::string>& names)
    {
        this->col_names->set_policy(this->_format.get_column_name_policy());
        this->col_names->set_col_names(names);
        this->n_cols = names.size();
    }
#ifdef _MSC_VER
#pragma endregion Format and header helpers
#endif

#ifdef _MSC_VER
#pragma region Reading helpers
#endif
    CSV_INLINE bool CSVReader::accept_row(CSVRow&& candidate, CSVRow* single_row, std::vector<CSVRow>* batch_rows) {
        const auto policy = this->_format.variable_column_policy;
        const size_t next_row_size = candidate.size();

        if (policy == VariableColumnPolicy::KEEP_NON_EMPTY && next_row_size == 0) {
            return false;
        }

        if (next_row_size != this->n_cols &&
            (policy == VariableColumnPolicy::THROW || policy == VariableColumnPolicy::IGNORE_ROW)) {
            if (policy == VariableColumnPolicy::THROW) {
                if (candidate.size() < this->n_cols) {
                    internals::throw_line_too_short(candidate.raw_str());
                }

                internals::throw_line_too_long(candidate.raw_str());
            }

            return false;
        }

        if (single_row != nullptr) {
            *single_row = std::move(candidate);
        } else if (batch_rows != nullptr) {
            batch_rows->push_back(std::move(candidate));
        } else {
            return false;
        }

        this->_n_rows++;
        this->_read_requested = false;
        return true;
    }

    CSV_INLINE void CSVReader::drain_rows_into_chunk(std::vector<CSVRow>& out, size_t max_rows) {
        std::vector<CSVRow> drained;
        drained.reserve(max_rows - out.size());
        this->records->drain_front(drained, max_rows - out.size());

        for (size_t i = 0; i < drained.size(); ++i) {
            this->accept_row(std::move(drained[i]), nullptr, &out);
        }
    }
#ifdef _MSC_VER
#pragma endregion Reading helpers
#endif

#ifdef _MSC_VER
#pragma region Worker reading methods
#endif

    /**
     * Read a chunk of CSV data.
     *
     * @note This method may run on a worker thread or synchronously on the caller
     *       thread when CSVFormat::threading(false) is active. Only one read_csv()
     *       invocation should be active at a time.
     *
     * @see csv::internals::CSVReadScheduler
     * @see CSVReader::read_row()
     */
    CSV_INLINE bool CSVReader::read_csv(size_t bytes) {
        // SCHEDULED READ FUNCTION: Runs asynchronously when runtime threading
        // is enabled, or synchronously when CSVFormat::threading(false) is active.
        //
        // Threading model:
        // 1. notify_all() - signals read_row() that worker is active
        // 2. parser->next() - reads and parses bytes (10MB chunks)
        // 3. kill_all() - signals read_row() that worker is done
        //
        // Exception handling: CSVReadScheduler catches exceptions and rethrows
        // them on the consumer thread. Bug #282 fixed cases where worker
        // exceptions were swallowed, causing std::terminate().
        
        // Tell read_row() to listen for CSV rows
        this->records->notify_all();

        try {
            this->parser->set_output(*this->records);
            this->parser->next(bytes);

            if (!this->header_trimmed) {
                this->trim_header();
            }
        }
        catch (...) {
            this->records->kill_all();
            throw;
        }

        // Tell read_row() to stop waiting
        this->records->kill_all();

        return true;
    }

    CSV_INLINE bool CSVReader::read_row(CSVRow &row) {
        while (this->check_for_rows()) {
            if (this->records->empty())
                continue;
                
            if (this->accept_row(this->records->pop_front(), &row, nullptr))
                return true;
        }

        return false;
    }

    CSV_INLINE bool CSVReader::read_chunk(std::vector<CSVRow>& out, size_t max_rows) {
        out.clear();

        if (max_rows == 0) {
            return false;
        }

        while (out.size() < max_rows) {
            if (check_for_rows()) {
                if (this->records->empty()) {
                    continue;
                }

                const size_t before_size = out.size();
                this->drain_rows_into_chunk(out, max_rows);

                if (out.size() == before_size) {
                    continue;
                }
            }
            else return !out.empty();
        }

        return true;
    }
#ifdef _MSC_VER
#pragma endregion Worker reading methods
#endif
}

/** @file
 *  Defines an input iterator for csv::CSVReader
 */


namespace csv {
    /** Return an iterator to the first row in the reader */
    CSV_INLINE CSVReader::iterator CSVReader::begin() {
        CSVRow row;
        if (!this->read_row(row)) {
            return this->end();
        }
        return CSVReader::iterator(this, std::move(row));
    }

    /** A placeholder for the imaginary past-the-end row in a CSV.
     *  Attempting to dereference this iterator is undefined.
     */
    CSV_INLINE CSV_CONST CSVReader::iterator CSVReader::end() const noexcept {
        return CSVReader::iterator();
    }

    CSV_INLINE CSVReader::iterator::iterator(CSVReader* _daddy, CSVRow&& _row) :
        daddy(_daddy) {
        row = std::move(_row);
    }

    /** Advance the iterator by one row. If this CSVReader has an
     *  associated file, then the iterator will lazily pull more data from
     *  that file until the end of file is reached.
     *
     *  @note This iterator does **not** block the thread responsible for parsing CSV.
     *
     */
    CSV_INLINE CSVReader::iterator& CSVReader::iterator::operator++() {
        if (!daddy->read_row(this->row)) {
            this->daddy = nullptr; // this == end()
        }

        return *this;
    }

    /** Post-increment iterator */
    CSV_INLINE CSVReader::iterator CSVReader::iterator::operator++(int) {
        auto temp = *this;
        if (!daddy->read_row(this->row)) {
            this->daddy = nullptr; // this == end()
        }

        return temp;
    }
}

/** @file
 *  Defines the data type used for storing information about a CSV row
 */

#include <cassert>
#include <functional>

namespace csv {
    namespace internals {
        CSV_INLINE csv::string_view get_trimmed(csv::string_view sv, const WhitespaceMap& ws_flags) noexcept
        {
            // Lazy trim only when requested
            size_t start = 0;
            while (start < sv.size() && ws_flags[sv[start] + CHAR_OFFSET]) {
                ++start;
            }

            size_t end = sv.size();
            while (end > start && ws_flags[sv[end - 1] + CHAR_OFFSET]) {
                --end;
            }

            return sv.substr(start, end - start);
        }
    }

    /** Return a CSVField object corrsponding to the nth value in the row.
     *
     *  @note This method performs bounds checking, and will throw an
     *        `std::runtime_error` if n is invalid.
     *
     *  @complexity
     *  Constant, by calling csv::CSVRow::get_csv::string_view()
     *
     */
    CSV_INLINE CSVField CSVRow::operator[](size_t n) const {
        return this->make_field(n, this->data);
    }

    /** Retrieve a value by its associated column name. If the column
     *  specified can't be round, a runtime error is thrown.
     *
     *  @complexity
     *  Constant. This calls the other CSVRow::operator[]() after
     *  converting column names into indices using a hash table.
     */
    CSV_INLINE CSVField CSVRow::operator[](csv::string_view col_name) const {
        auto & col_names = this->data->col_names;
        auto col_pos = col_names->index_of(col_name);
        if (col_pos > -1) {
            return this->operator[](col_pos);
        }

        internals::throw_column_not_found(col_name);
    }
    CSV_INLINE CSVRow::operator std::vector<std::string>() const {
        std::vector<std::string> ret;
        for (size_t i = 0; i < size(); i++)
            ret.push_back(std::string(this->get_field(i)));

        return ret;
    }

    CSV_INLINE csv::string_view CSVRow::raw_str() const noexcept {
        if (!data) return csv::string_view();
        const csv::string_view full = data->data;
        if (data_start >= full.size()) return csv::string_view();

        if (data_end != (std::numeric_limits<size_t>::max)()
            && data_end >= data_start
            && data_end <= full.size()) {
            return full.substr(data_start, data_end - data_start);
        }

        const size_t end = full.find('\n', data_start);
        const size_t len = (end == csv::string_view::npos)
            ? (full.size() - data_start)
            : (end - data_start);
        return full.substr(data_start, len);
    }

    /** Build a map from column names to values for a given row. */
    CSV_INLINE std::unordered_map<std::string, std::string> CSVRow::to_unordered_map() const {
        std::unordered_map<std::string, std::string> row_map;
        row_map.reserve(this->size());

        for (size_t i = 0; i < this->size(); i++) {
            auto col_name = (*this->data->col_names)[i];
            row_map[col_name] = this->operator[](i).get<std::string>();
        }

        return row_map;
    }

    /** Build a map from a subset of column names to values for a given row. */
    CSV_INLINE std::unordered_map<std::string, std::string> CSVRow::to_unordered_map(
        const std::vector<std::string>& subset
    ) const {
        std::unordered_map<std::string, std::string> row_map;
        row_map.reserve(subset.size());

        for (const auto& col_name : subset)
            row_map[col_name] = this->operator[](col_name).get<std::string>();

        return row_map;
    }

    CSV_INLINE csv::string_view CSVRow::get_field(size_t index) const
    {
        return this->get_field_impl(index, this->data);
    }

    CSV_INLINE csv::string_view CSVRow::get_field_safe(size_t index, internals::RawCSVDataPtr _data) const
    {
        return this->get_field_impl(index, _data);
    }

    CSV_INLINE CSVField CSVRow::make_field(size_t index, const internals::RawCSVDataPtr& _data) const
    {
        const csv::string_view field = this->get_field_impl(index, _data);
        const size_t field_index = this->fields_start + index;
        if (_data->has_field_scalars() && field_index < _data->field_scalars.size()) {
            return CSVField(field, _data->field_scalars[field_index]);
        }

        return CSVField(field);
    }

    CSV_INLINE bool CSVField::try_parse_decimal(long double& dVal, const char decimalSymbol) {
        // If field has already been parsed to empty, no need to do it aagin:
        if (this->type_ == DataType::CSV_NULL)
                    return false;

        if (this->type_ == DataType::UNKNOWN)
            this->get_value();

        if (this->type_ == DataType::CSV_NULL)
            return false;

        if (this->type_ == DataType::CSV_STRING || this->type_ == DataType::CSV_DOUBLE) {
            double parsed_value = 0;
            if (!classify_scalar::parse_float(this->sv.data(), this->sv.data() + this->sv.size(), parsed_value, decimalSymbol)) {
                if (this->type_ == DataType::CSV_DOUBLE)
                    this->type_ = DataType::CSV_STRING;
                return false;
            }

            this->cache_parsed_value(DataType::CSV_DOUBLE, parsed_value);
        }

        // Integral types are not affected by decimalSymbol and need not be parsed again

        // Either we already had an integral type before, or we we just got any numeric type now.
        if (this->type_ >= DataType::CSV_INT8 && this->type_ <= DataType::CSV_DOUBLE) {
            dVal = this->numeric_value_as_long_double();
            return true;
        }

        // CSV_NULL or CSV_STRING, not numeric
        return false;
    }

    CSV_INLINE bool CSVField::try_parse_timestamp(std::uint64_t& out) noexcept {
        if (this->type_ == DataType::UNKNOWN)
            this->get_value();

        if (this->type_ == DataType::CSV_TIMESTAMP) {
            out = this->value_.timestamp;
            return true;
        }

        if (this->stores_integral() && this->value_.integer >= 0) {
            out = static_cast<std::uint64_t>(this->value_.integer);
            return true;
        }

        return false;
    }

#ifdef _MSC_VER
#pragma region CSVRow Iterator
#endif
    /** Return an iterator pointing to the first field. */
    CSV_INLINE CSVRow::iterator CSVRow::begin() const {
        return CSVRow::iterator(this, 0);
    }

    /** Return an iterator pointing to just after the end of the CSVRow.
     *
     *  @warning Attempting to dereference the end iterator results
     *           in dereferencing a null pointer.
     */
    CSV_INLINE CSVRow::iterator CSVRow::end() const noexcept {
        return CSVRow::iterator(this, (int)this->size());
    }

    CSV_INLINE CSVRow::reverse_iterator CSVRow::rbegin() const noexcept {
        return std::reverse_iterator<CSVRow::iterator>(this->end());
    }

    CSV_INLINE CSVRow::reverse_iterator CSVRow::rend() const {
        return std::reverse_iterator<CSVRow::iterator>(this->begin());
    }

    CSV_INLINE CSV_NON_NULL(2)
    CSVRow::iterator::iterator(const CSVRow* _reader, int _i)
        : daddy(_reader), data(_reader->data), i(_i) {
        if (_i < (int)this->daddy->size())
            this->field = std::make_shared<CSVField>(
                this->daddy->make_field(_i, this->data));
        else
            this->field = nullptr;
    }

    CSV_INLINE CSVRow::iterator::reference CSVRow::iterator::operator*() const {
        return *(this->field.get());
    }

    CSV_INLINE CSVRow::iterator::pointer CSVRow::iterator::operator->() const {
        return this->field;
    }

    CSV_INLINE CSVRow::iterator& CSVRow::iterator::operator++() {
        // Pre-increment operator
        this->i++;
        if (this->i < (int)this->daddy->size())
            this->field = std::make_shared<CSVField>(
                this->daddy->make_field(i, this->data));
        else // Reached the end of row
            this->field = nullptr;
        return *this;
    }

    CSV_INLINE CSVRow::iterator CSVRow::iterator::operator++(int) {
        // Post-increment operator
        auto temp = *this;
        this->operator++();
        return temp;
    }

    CSV_INLINE CSVRow::iterator& CSVRow::iterator::operator--() {
        // Pre-decrement operator
        this->i--;
        this->field = std::make_shared<CSVField>(
            this->daddy->make_field(this->i, this->data));
        return *this;
    }

    CSV_INLINE CSVRow::iterator CSVRow::iterator::operator--(int) {
        // Post-decrement operator
        auto temp = *this;
        this->operator--();
        return temp;
    }
    
    CSV_INLINE CSVRow::iterator CSVRow::iterator::operator+(difference_type n) const {
        // Allows for iterator arithmetic
        return CSVRow::iterator(this->daddy, i + (int)n);
    }

    CSV_INLINE CSVRow::iterator CSVRow::iterator::operator-(difference_type n) const {
        // Allows for iterator arithmetic
        return CSVRow::iterator::operator+(-n);
    }

    CSV_INLINE CSVRow::iterator& CSVRow::iterator::operator+=(difference_type n) {
        *this = *this + n;
        return *this;
    }

    CSV_INLINE CSVRow::iterator& CSVRow::iterator::operator-=(difference_type n) {
        *this = *this - n;
        return *this;
    }

    CSV_INLINE CSVRow::iterator::difference_type CSVRow::iterator::operator-(const iterator& other) const noexcept {
        return this->i - other.i;
    }
#ifdef _MSC_VER
#pragma endregion CSVRow Iterator
#endif
}


namespace csv {
    CSV_INLINE std::unordered_map<std::string, DataType> csv_data_types(CSVReader& reader) {
        std::unordered_map<std::string, DataType> csv_dtypes;
        const auto col_names = reader.get_col_names();
        std::vector<std::unordered_map<DataType, size_t>> type_counts(col_names.size());
        constexpr size_t TYPE_CHUNK_SIZE = 5000;

        chunk_parallel_apply(reader, type_counts,
            [](DataFrame<>::column_type column, std::unordered_map<DataType, size_t>& counts) {
                for (size_t row_index = 0; row_index < column.size(); ++row_index) {
                    counts[internals::data_type(column.get_sv(row_index))]++;
                }
            },
            TYPE_CHUNK_SIZE
        );

        for (size_t i = 0; i < col_names.size(); i++) {
            auto& col = type_counts[i];
            auto& col_name = col_names[i];

            if (col[DataType::CSV_STRING])
                csv_dtypes[col_name] = DataType::CSV_STRING;
            else if (col[DataType::CSV_INT64])
                csv_dtypes[col_name] = DataType::CSV_INT64;
            else if (col[DataType::CSV_INT32])
                csv_dtypes[col_name] = DataType::CSV_INT32;
            else if (col[DataType::CSV_INT16])
                csv_dtypes[col_name] = DataType::CSV_INT16;
            else if (col[DataType::CSV_INT8])
                csv_dtypes[col_name] = DataType::CSV_INT8;
            else if (col[DataType::CSV_BOOL])
                csv_dtypes[col_name] = DataType::CSV_BOOL;
            else if (col[DataType::CSV_TIMESTAMP])
                csv_dtypes[col_name] = DataType::CSV_TIMESTAMP;
            else if (col[DataType::CSV_NULL])
                csv_dtypes[col_name] = DataType::CSV_NULL;
            else
                csv_dtypes[col_name] = DataType::CSV_DOUBLE;
        }

        return csv_dtypes;
    }
}


#include <system_error>

namespace csv {
    namespace internals {
        namespace parser {
#if defined(__EMSCRIPTEN__)
        // Opens the file and delegates to the template overload to avoid duplicating the read/resize logic.
        CSV_INLINE std::string get_csv_head_stream(csv::string_view filename) {
            std::ifstream infile(std::string(filename), std::ios::binary);
            if (!infile.is_open()) {
                throw_cannot_open_file(filename);
            }
            return get_csv_head_stream(infile);
        }
#endif

#if !defined(__EMSCRIPTEN__)
        CSV_INLINE std::pair<std::string, size_t> get_csv_head_mmap(csv::string_view filename) {
            const size_t bytes = 500000;
            std::error_code error;
            auto mmap = mio::make_mmap_source(std::string(filename), 0, mio::map_entire_file, error);
            if (error) {
                throw_cannot_open_file(filename);
            }
            const size_t file_size = mmap.size();
            const size_t length = std::min(file_size, bytes);
            return { std::string(mmap.begin(), mmap.begin() + length), file_size };
        }
#endif

        CSV_INLINE std::string get_csv_head(csv::string_view filename) {
#if defined(__EMSCRIPTEN__)
            return get_csv_head_stream(filename);
#else
            return get_csv_head_mmap(filename).first;
#endif
        }
        }

        CSV_INLINE size_t get_bom_skip_or_throw(csv::string_view data, bool& utf8_bom) {
            utf8_bom = false;

            if (data.size() >= 4) {
                const unsigned char b0 = static_cast<unsigned char>(data[0]);
                const unsigned char b1 = static_cast<unsigned char>(data[1]);
                const unsigned char b2 = static_cast<unsigned char>(data[2]);
                const unsigned char b3 = static_cast<unsigned char>(data[3]);

                if ((b0 == 0xFF && b1 == 0xFE && b2 == 0x00 && b3 == 0x00)
                    || (b0 == 0x00 && b1 == 0x00 && b2 == 0xFE && b3 == 0xFF)) {
                    throw_unsupported_encoding("UTF-32");
                }
            }

            if (data.size() >= 3) {
                const unsigned char b0 = static_cast<unsigned char>(data[0]);
                const unsigned char b1 = static_cast<unsigned char>(data[1]);
                const unsigned char b2 = static_cast<unsigned char>(data[2]);

                if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) {
                    utf8_bom = true;
                    return 3;
                }
            }

            if (data.size() >= 2) {
                const unsigned char b0 = static_cast<unsigned char>(data[0]);
                const unsigned char b1 = static_cast<unsigned char>(data[1]);

                if ((b0 == 0xFF && b1 == 0xFE) || (b0 == 0xFE && b1 == 0xFF)) {
                    throw_unsupported_encoding("UTF-16");
                }
            }

            return 0;
        }

        namespace parser {

#ifdef _MSC_VER
#pragma region CSVParserDriverBase
#endif

        CSV_INLINE CSVParserDriverBase::CSVParserDriverBase(
            const CSVFormat& source_format,
            const ColNamesPtr& col_names
        ) : CSVParserCore<>(source_format, col_names) {}

        CSV_INLINE void CSVParserDriverBase::resolve_format_from_head(const CSVFormat& source_format) {
            auto head = this->get_csv_head();

            ResolvedFormat resolved;
            resolved.format = source_format;

            const bool infer_delimiter = source_format.guess_delim();
            const bool infer_header = !source_format.header_explicitly_set_
                && (infer_delimiter || !source_format.col_names_explicitly_set_);
            const bool infer_n_cols = (source_format.get_header() < 0 && source_format.get_col_names().empty());

            if (infer_delimiter || infer_header || infer_n_cols) {
                auto guess_result = guess_format(head, source_format.get_possible_delims());
                if (infer_delimiter) {
                    resolved.format.delimiter(guess_result.delim);
                }

                if (infer_header) {
                    // Inferred header should not clear user-provided column names.
                    resolved.format.header = guess_result.header_row;
                }

                resolved.n_cols = guess_result.n_cols;
            }

            if (resolved.format.no_quote) {
                this->set_parse_flags(
                    make_parse_flags(resolved.format.get_delim()),
                    resolved.format.get_delim(),
                    resolved.format.get_delim()
                );
            }
            else {
                this->set_parse_flags(
                    make_parse_flags(resolved.format.get_delim(), resolved.format.quote_char),
                    resolved.format.get_delim(),
                    resolved.format.quote_char
                );
            }

            this->format = resolved;
        }
#ifdef _MSC_VER
#pragma endregion
#endif
        }
    }
}


#include <unordered_map>
#include <vector>

namespace csv {
    namespace internals {
        namespace parser {
        CSV_INLINE GuessScore calculate_score(csv::string_view head, const CSVFormat& format) {
            // Frequency counter of row length
            std::unordered_map<size_t, size_t> row_tally = { { 0, 0 } };

            // Map row lengths to row num where they first occurred
            std::unordered_map<size_t, size_t> row_when = { { 0, 0 } };

            // Parse the CSV using the low-level constructor that takes pre-built flag
            // tables — bypasses format resolution entirely and avoids recursion back
            // into guess_format.
            std::vector<CSVRow> rows;

            const auto parse_flags = format.is_quoting_enabled()
                ? make_parse_flags(format.get_delim(), format.get_quote_char())
                : make_parse_flags(format.get_delim());
            const auto ws_flags = make_ws_flags(format.get_trim_chars());
            auto head_owner = std::make_shared<std::string>(std::string(head));
            CSVParserCore<std::vector<CSVRow>> parser(parse_flags, ws_flags);
            parser.parse_chunk(*head_owner, head_owner, rows);
            parser.end_feed();

            for (size_t i = 0; i < rows.size(); i++) {
                auto& row = rows[i];

                // Ignore zero-length rows
                if (row.size() > 0) {
                    if (row_tally.find(row.size()) != row_tally.end()) {
                        row_tally[row.size()]++;
                    }
                    else {
                        row_tally[row.size()] = 1;
                        row_when[row.size()] = i;
                    }
                }
            }

            double final_score = 0;
            size_t header_row = 0;
            size_t mode_row_length = 0;

            // Final score is equal to the largest row size times rows of that size.
            for (auto& pair : row_tally) {
                const size_t row_size = pair.first;
                const size_t row_count = pair.second;
                const double score = (double)(row_size * row_count);
                if (score > final_score) {
                    final_score = score;
                    mode_row_length = row_size;
                    header_row = row_when[row_size];
                }
            }

            // Heuristic: If first row has >= columns than mode, use it as header.
            size_t first_row_length = rows.size() > 0 ? rows[0].size() : 0;
            if (first_row_length >= mode_row_length && first_row_length > 0) {
                header_row = 0;
            }

            return { header_row, mode_row_length, final_score };
        }

        CSV_INLINE CSVGuessResult guess_format(csv::string_view head, const std::vector<char>& delims) {
            /** For each delimiter, find out which row length was most common (mode).
             *  The delimiter with the highest score (row_length * count) wins.
             *
             *  Header detection: If first row has >= columns than mode, use row 0.
             *  Otherwise use the first row with the mode length.
             */
            CSVFormat format;
            size_t max_score = 0;
            size_t header = 0;
            size_t n_cols = 0;
            char current_delim = delims[0];

            for (char cand_delim : delims) {
                auto result = calculate_score(head, format.delimiter(cand_delim));

                if ((size_t)result.score > max_score) {
                    max_score = (size_t)result.score;
                    current_delim = cand_delim;
                    header = result.header;
                    n_cols = result.mode_row_length;
                }
            }

            return { current_delim, (int)header, n_cols };
        }
        }
    }
}


#if !defined(__EMSCRIPTEN__)
namespace csv {
    namespace internals {
        namespace parser {
        CSV_INLINE MmapParser::MmapParser(
            csv::string_view filename,
            const CSVFormat& format,
            const ColNamesPtr& col_names
        ) : CSVParserDriverBase(format, col_names) {
            this->_filename = filename.data();
            auto head_and_size = get_csv_head_mmap(filename);
            this->head_ = std::move(head_and_size.first);
            this->source_size_ = head_and_size.second;
            this->resolve_format_from_head(format);

            this->parse_orchestrator_ = make_csv_parse_orchestrator(
                this->parse_flags_,
                this->whitespace_flags(),
                format,
                this->source_size_,
                col_names,
                true
            );
        }

        CSV_INLINE MmapParser::~MmapParser() = default;

        CSV_INLINE void MmapParser::finalize_loaded_chunk(
            csv::string_view chunk,
            std::shared_ptr<void> owner,
            size_t length,
            size_t chunk_size
        ) {
            const size_t base_offset = this->mmap_pos - length;
            const bool source_exhausted = this->mmap_pos == this->source_size_;
            CSVParseWindowResult result = this->parse_orchestrator_->parse_window(
                chunk,
                std::move(owner),
                base_offset,
                chunk_size,
                source_exhausted,
                this->output()
            );

            if (source_exhausted) {
                this->eof_ = true;
            }

            this->mmap_pos -= (length - result.complete_prefix_length);
        }

        CSV_INLINE size_t MmapParser::read_window_size(size_t chunk_size) const noexcept {
            return this->parse_orchestrator_
                ? this->parse_orchestrator_->read_window_size(chunk_size)
                : chunk_size;
        }

        CSV_INLINE void MmapParser::next(size_t bytes = CSV_CHUNK_SIZE_DEFAULT) {
            // CRITICAL SECTION: Chunk Transition Logic
            // This function reads 10MB chunks and must correctly handle fields that span
            // chunk boundaries. The 'remainder' calculation below ensures partial fields
            // are preserved for the next chunk.
            //
            // Bug #280: Field corruption occurred here when chunk transitions incorrectly
            // split multi-byte characters or field boundaries.

            // Reuse the pre-read head buffer (if any) as the first chunk.
            // This avoids re-reading the same bytes that were already consumed
            // for delimiter/header guessing.
            if (!head_.empty()) {
                auto head_owner = std::make_shared<std::string>(std::move(head_));
                const size_t length = head_owner->size();
                this->mmap_pos += length;

                this->finalize_loaded_chunk(*head_owner, head_owner, length, bytes);
                return;
            }

            // Create memory map
            const size_t offset = this->mmap_pos;
            const size_t remaining = (offset < this->source_size_)
                ? (this->source_size_ - offset)
                : 0;
            const size_t length = std::min(remaining, this->read_window_size(bytes));
            if (length == 0) {
                // No more data to read; mark EOF and end feed
                // (Prevent exception on empty mmap as reported by #267)
                this->eof_ = true;
                this->end_feed();
                return;
            }

            std::error_code error;
            auto mmap = mio::make_mmap_source(this->_filename, offset, length, error);
            if (error) {
                throw_mmap_failure(error, this->_filename, offset, length);
            }
            auto mmap_owner = std::make_shared<mio::basic_mmap_source<char>>(std::move(mmap));
            this->mmap_pos += length;

            auto mmap_ptr = mmap_owner.get();

            // Create string view
            csv::string_view chunk(mmap_ptr->data(), mmap_ptr->length());
            this->finalize_loaded_chunk(chunk, mmap_owner, length, bytes);
        }
        }
    }
}
#endif



#endif
