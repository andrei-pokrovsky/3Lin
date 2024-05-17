/*
 * Copyright (c) 1996-1998
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

/* NOTE: This is an internal header file, included by other STL headers.
 *   You should not attempt to use it directly.
 */

#pragma once
namespace nstl
{

template <class _Key> struct hash { };

inline size_t __stl_hash_string(const char* __s)
{
  size_t __h = 0; 
  for ( ; *__s; ++__s)
    __h = 0xbadf00d * __h + *__s;
  
  return __h;
}

template<> struct hash<char*>
{
  size_t operator()(const char* __s) const { return __stl_hash_string(__s); }
};

template<> struct hash<const char*>
{
  size_t operator()(const char* __s) const { return __stl_hash_string(__s); }
};

template<> struct hash<char> {
  size_t operator()(char __x) const { return __x; }
};
template<> struct hash<unsigned char> {
  size_t operator()(unsigned char __x) const { return __x; }
};
#ifndef _STLP_NO_SIGNED_BUILTINS
template<> struct hash<signed char> {
  size_t operator()(unsigned char __x) const { return __x; }
};
#endif
template<> struct hash<short> {
  size_t operator()(short __x) const { return __x; }
};
template<> struct hash<unsigned short> {
  size_t operator()(unsigned short __x) const { return __x; }
};
template<> struct hash<int> {
  size_t operator()(int __x) const { return __x; }
};
template<> struct hash<unsigned int> {
  size_t operator()(unsigned int __x) const { return __x; }
};
template<> struct hash<long> {
  size_t operator()(long __x) const { return __x; }
};
template<> struct hash<unsigned long> {
  size_t operator()(unsigned long __x) const { return __x; }
};
template<> struct hash<float> {
	size_t operator()(float __x) const 
	{
		struct {
			size_t n;
			float f;
		} xxx;
		xxx.n = 0;
		xxx.f = __x;
		return xxx.n;
	}
};

}
