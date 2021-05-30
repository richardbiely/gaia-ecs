/*
 Copyright (c) 2020 Kang Yue Sheng Benjamin.
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef static_sort_h
#define static_sort_h

/*
 Adapted from the Bose-Nelson Sorting network code from:
 https://github.com/atinm/bose-nelson/blob/master/bose-nelson.c
 */

/**
 * A Functor class to create a sort for fixed sized arrays/containers with a
 * compile time generated Bose-Nelson sorting network.
 * \tparam NumElements  The number of elements in the array or container to sort.
 */
template <unsigned NumElements> class StaticSort
{
	// Default less than comparator
	struct LT
	{
		template <class A, class B>
		inline bool operator () (const A &a, const B &b) const
		{
			return a < b;
		}
	};
	
	template <class A, class C, int I0, int I1> struct Swap
	{
		template <class T> inline void s(T &v0, T &v1, C c)
		{
			// Explicitly code out the Min and Max to nudge the compiler
			// to generate branchless code where applicable.
			T t = c(v0, v1) ? v0 : v1; // Min
			v1 = c(v0, v1) ? v1 : v0; // Max
			v0 = t;
		}
		
		inline Swap(A &a, C c) { s(a[I0], a[I1], c); }
	};
	
	template <class A, class C, int I, int J, int X, int Y> struct PB
	{
		inline PB(A &a, C c)
		{
			enum { L = X >> 1, M = (X & 1 ? Y : Y + 1) >> 1, IAddL = I + L, XSubL = X - L };
			PB<A, C, I, J, L, M> p0(a, c);
			PB<A, C, IAddL, J + M, XSubL, Y - M> p1(a, c);
			PB<A, C, IAddL, J, XSubL, M> p2(a, c);
		}
	};
	
	template <class A, class C, int I, int J> struct PB <A, C, I, J, 1, 1>
	{
		inline PB(A &a, C c) { Swap<A, C, I - 1, J - 1> s(a, c); }
	};
	
	template <class A, class C, int I, int J> struct PB <A, C, I, J, 1, 2>
	{
		inline PB(A &a, C c) { Swap<A, C, I - 1, J> s0(a, c); Swap<A, C, I - 1, J - 1> s1(a, c); }
	};
	
	template <class A, class C, int I, int J> struct PB <A, C, I, J, 2, 1>
	{
		inline PB(A &a, C c) { Swap<A, C, I - 1, J - 1> s0(a, c); Swap<A, C, I, J - 1> s1(a, c); }
	};
	
	template <class A, class C, int I, int M, int Stop> struct PS
	{
		inline PS(A &a, C c)
		{
			enum { L = M >> 1, IAddL = I + L, MSubL = M - L};
			PS<A, C, I, L, (L <= 1)> ps0(a, c);
			PS<A, C, IAddL, MSubL, (MSubL <= 1)> ps1(a, c);
			PB<A, C, I, IAddL, L, MSubL> pb(a, c);
		}
	};
	
	template <class A, class C, int I, int M> struct PS <A, C, I, M, 1>
	{
		inline PS(A &a, C c) {}
	};
	
public:
	/**
	 * Sorts the array/container arr.
	 * \param  arr  The array/container to be sorted.
	 */
	template <class Container>
	inline void operator() (Container &arr) const
	{
		PS<Container, LT, 1, NumElements, (NumElements <= 1)> ps(arr, LT());
	};
	
	/**
	 * Sorts the array arr.
	 * \param  arr  The array to be sorted.
	 */
	template <class T>
	inline void operator() (T *arr) const
	{
		PS<T*, LT, 1, NumElements, (NumElements <= 1)> ps(arr, LT());
	};
	
	/**
	 * Sorts the array/container arr.
	 * \param  arr     The array/container to be sorted.
	 * \tparam Compare The less than comparator.
	 */
	template <class Container, class Compare>
	inline void operator() (Container &arr, Compare &lt) const
	{
		typedef Compare & C;
		PS<Container, C, 1, NumElements, (NumElements <= 1)> ps(arr, lt);
	};
	
	/**
	 * Sorts the array arr.
	 * \param  arr     The array to be sorted.
	 * \tparam Compare The less than comparator.
	 */
	template <class T, class Compare>
	inline void operator() (T *arr, Compare &lt) const
	{
		typedef Compare & C;
		PS<T*, C, 1, NumElements, (NumElements <= 1)> ps(arr, lt);
	};
	
	/**
	 * Sorts the array/container arr.
	 * \param  arr     The array/container to be sorted.
	 * \tparam Compare The less than comparator.
	 */
	template <class Container, class Compare>
	inline void operator() (Container &arr, const Compare &lt) const
	{
		typedef const Compare & C;
		PS<Container, C, 1, NumElements, (NumElements <= 1)> ps(arr, lt);
	};
	
	/**
	 * Sorts the array arr.
	 * \param  arr     The array to be sorted.
	 * \tparam Compare The less than comparator.
	 */
	template <class T, class Compare>
	inline void operator() (T *arr, const Compare &lt) const
	{
		typedef const Compare & C;
		PS<T*, C, 1, NumElements, (NumElements <= 1)> ps(arr, lt);
	};
};


/**
 * A Functor class to create a sort for fixed sized arrays/containers with a
 * compile time generated Bose-Nelson sorting network.
 * Inspired by TimSort, this scans through the array first.
 * It skips the sorting-network if it is strictly increasing or decreasing. ;)
 * \tparam NumElements  The number of elements in the array or container to sort.
 */
template <unsigned NumElements> class StaticTimSort
{
	// Default less than comparator
	struct LT
	{
		template <class A, class B>
		inline bool operator () (const A &a, const B &b) const
		{
			return a < b;
		}
	};
	
	template <class A, class C> struct Intro
	{
		template <class T>
		static inline void reverse(T _, A &a)
		{
			if (NumElements > 1) {
				unsigned left = 0, right = NumElements - 1;
				while (left < right) {
					T temp = a[left];
					a[left++] = a[right];
					a[right--] = temp;
				}
			}
		}
		
		template <class T>
		static inline bool sorted(T prev, A &a, C c)
		{
			if (NumElements < 8) return false;
			
			bool hasDecreasing = false;
			bool hasIncreasing = false;
			
			for (unsigned i = 1; i < NumElements; ++i) {
				T curr = a[i];
				if (c(curr, prev)) {
					hasDecreasing = true;
				}
				if (c(prev, curr)) {
					hasIncreasing = true;
				}
				prev = curr;
				if (NumElements > 22)
					if (hasIncreasing && hasDecreasing)
						return false;
			}
			if (!hasDecreasing) {
				return true;
			}
			if (!hasIncreasing) {
				reverse(a[0], a);
				return true;
			}
			return false;
		}
	};
	
	
	
public:
	/**
	 * Sorts the array/container arr.
	 * \param  arr  The array/container to be sorted.
	 */
	template <class Container>
	inline void operator() (Container &arr) const
	{
		if (!Intro<Container, LT>::sorted(arr[0], arr, LT()))
			StaticSort<NumElements>()(arr);
	};
	
	/**
	 * Sorts the array arr.
	 * \param  arr  The array to be sorted.
	 */
	template <class T>
	inline void operator() (T *arr) const
	{
		if (!Intro<T*, LT>::sorted(arr[0], arr, LT()))
			StaticSort<NumElements>()(arr);
	};
	
	/**
	 * Sorts the array/container arr.
	 * \param  arr     The array/container to be sorted.
	 * \tparam Compare The less than comparator.
	 */
	template <class Container, class Compare>
	inline void operator() (Container &arr, Compare &lt) const
	{
		typedef Compare & C;
		if (!Intro<Container, C>::sorted(arr[0], arr, lt))
			StaticSort<NumElements>()(arr, lt);
	};
	
	/**
	 * Sorts the array arr.
	 * \param  arr     The array to be sorted.
	 * \tparam Compare The less than comparator.
	 */
	template <class T, class Compare>
	inline void operator() (T *arr, Compare &lt) const
	{
		typedef Compare & C;
		if (!Intro<T*, C>::sorted(arr[0], arr, lt))
			StaticSort<NumElements>()(arr, lt);
	};
	
	/**
	 * Sorts the array/container arr.
	 * \param  arr     The array/container to be sorted.
	 * \tparam Compare The less than comparator.
	 */
	template <class Container, class Compare>
	inline void operator() (Container &arr, const Compare &lt) const
	{
		typedef const Compare & C;
		if (!Intro<Container, C>::sorted(arr[0], arr, lt))
			StaticSort<NumElements>()(arr, lt);
	};
	
	/**
	 * Sorts the array arr.
	 * \param  arr     The array to be sorted.
	 * \tparam Compare The less than comparator.
	 */
	template <class T, class Compare>
	inline void operator() (T *arr, const Compare &lt) const
	{
		typedef const Compare & C;
		if (!Intro<T*, C>::sorted(arr[0], arr, lt))
			StaticSort<NumElements>()(arr, lt);
	};
};
#endif