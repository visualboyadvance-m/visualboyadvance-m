/*
                Range
                =====

    Copyright (c) 2009-2011 Khaled Alshaya

    Distributed under the Boost Software License, version 1.0
    (See the license at: http://www.boost.org/license_1_0.txt).
*/

/*
                Rationale
                =========

    In Python, there is a beautiful function called "range".
    "range" allows the programmer to iterate over a range elegantly.
    This concept is not as general as "for-loops" in C++,
    but non the less, it expresses the intent of the programmer
    clearer than the general "for-loops" in many cases.


                Design
                ======

    Range is made to be STL-like library. In fact, it is 
    built on top of the concepts of STL. The library is designed to
    work with STL algorithms as well. Range is more flexible
    than the Python "range", because:
    
    Range is an "immutable ordered random access container"


                Specifications
                ==============

    Range satisfies the following requirements:

        * Immutable.
        * Random Access Container.
        * Random Access Iterator Interface.
        * Constant Time Complexity Operations.


    Range models an ordered sequence of elements,
    where a range is defined by:

        [begin, end)

        * begin: the first element in the range. (Inclusive)
        * end  : the last element in the range.  (Exclusive)
        * step : the distance between two consecutive elements in a range.

        where each element in the range is defined by:

        element = begin + step * i

        * i: is the index of the element in range.

        The following precondition must be met for the sequence
        to be a valid range:

            step != 0
            &&
            (    
                begin <= end && step > 0
                ||
                begin >= end && step < 0
            )


                Portability
                ===========

    Range Generator is written in standard C++ (C++98). It depends
    -only- on the standard C++ library.
*/

#ifndef range_h__
#define range_h__

// using std::range
// using std::size_t from <cstddef>
// using std::ceil   from <cmath>
#include <iterator>
#include <stdexcept>
#include <cstddef>
#include <cmath>

namespace Range
{
    template <class IntegerType>
    struct basic_range
    {
        struct const_iterator_impl
        {
            typedef IntegerType        value_type;
            typedef std::size_t        size_type;
            typedef IntegerType        difference_type;
            typedef value_type*        pointer;
            typedef value_type&        reference;
            typedef
                std::random_access_iterator_tag
                iterator_category;

            const_iterator_impl(): r(0), index(0)
            { }

            const_iterator_impl(const const_iterator_impl& rhs)
                : r(rhs.r), index(rhs.index)
            { }

            const_iterator_impl(basic_range<IntegerType> const * p_range, size_type p_index)
                :r(p_range), index(p_index)
            { }

            const_iterator_impl& operator=(const const_iterator_impl& rhs)
            {
                r = rhs.r;
                index = rhs.index;
                return *this;
            }

            bool operator==(const const_iterator_impl& rhs) const
            {
                return *r == *(rhs.r) && index == rhs.index;
            }

            bool operator!=(const const_iterator_impl& rhs) const
            {
                return !(*this == rhs);
            }

            bool operator<(const const_iterator_impl& rhs) const
            {
                return index < rhs.index;
            }

            bool operator>(const const_iterator_impl& rhs) const
            {
                return index > rhs.index;
            }

            bool operator<=(const const_iterator_impl& rhs) const
            {
                return index <= rhs.index;
            }

            bool operator>=(const const_iterator_impl& rhs) const
            {
                return index >= rhs.index;
            }

            value_type operator*() const
            {
                return r->m_first_element + r->m_step*index;
            }

            // operator->
            // is not implemented because the value_type is an integer type
            // and primitive types in C++ don't define member functions.

            const_iterator_impl& operator++()
            {
                ++index;
                return *this;
            }

            const_iterator_impl operator++(int)
            {
                const_iterator_impl temp = *this;
                ++index;
                return temp;
            }

            const_iterator_impl& operator--()
            {
                --index;
                return *this;
            }

            const_iterator_impl operator--(int)
            {
                const_iterator_impl temp = *this;
                --index;
                return temp;
            }

            const_iterator_impl& operator+=(difference_type increment)
            {
                index += increment;
                return *this;
            }

            // operator+
            // is friend operator but operator-
            // is not, because we want to allow the following for "+":
            // iterator+5
            // 5+iterator
            // For the "-" it is not correct to do so, because
            // iterator-5 != 5-iterator
            friend const_iterator_impl operator+
                (const const_iterator_impl& lhs, difference_type increment)
            {
                const_iterator_impl sum;
                sum.r = lhs.r;
                sum.index = lhs.index + increment;
                return sum;
            }

            const_iterator_impl& operator-=(difference_type decrement)
            {
                index -= decrement;
                return *this;
            }

            const_iterator_impl operator-(difference_type decrement) const
            {
                const_iterator_impl shifted_iterator;
                shifted_iterator.r = r;
                shifted_iterator.index = index - decrement;
                return shifted_iterator;
            }

            difference_type operator-(const const_iterator_impl& rhs) const
            {
                return index - rhs.index;
            }

            value_type operator[](difference_type offset) const
            {
                size_type new_index = index + offset;
                return r->m_first_element + r->m_step*new_index;
            }

        private:
            basic_range<IntegerType> const * r;
            size_type index;
        };


        struct const_reverse_iterator_impl
        {
            typedef IntegerType        value_type;
            typedef std::size_t        size_type;
            typedef IntegerType        difference_type;
            typedef value_type*        pointer;
            typedef value_type&        reference;
            typedef
                std::random_access_iterator_tag
                iterator_category;


            const_reverse_iterator_impl(): r(0), index(0)
            { }

            const_reverse_iterator_impl(const const_reverse_iterator_impl& rhs)
                : r(rhs.r), index(rhs.index)
            { }

            const_reverse_iterator_impl(basic_range<IntegerType> const * p_range, size_type p_index)
                :r(p_range), index(p_index)
            { }

            const_reverse_iterator_impl& operator=(const const_reverse_iterator_impl& rhs)
            {
                r = rhs.r;
                index = rhs.index;
                return *this;
            }

            bool operator==(const const_reverse_iterator_impl& rhs) const
            {
                return *r == *(rhs.r) && index == rhs.index;
            }

            bool operator!=(const const_reverse_iterator_impl& rhs) const
            {
                return !(*this == rhs);
            }

            bool operator<(const const_reverse_iterator_impl& rhs) const
            {
                return index < rhs.index;
            }

            bool operator>(const const_reverse_iterator_impl& rhs) const
            {
                return index > rhs.index;
            }

            bool operator<=(const const_reverse_iterator_impl& rhs) const
            {
                return index <= rhs.index;
            }

            bool operator>=(const const_reverse_iterator_impl& rhs) const
            {
                return index >= rhs.index;
            }

            value_type operator*() const
            {
                size_type reverse_index
                    = (r->m_element_count - 1) - index;
                return r->m_first_element + r->m_step*reverse_index;
            }

            // operator->
            // is not implemented because the value_type is integer type
            // and primitive types in C++ don't define member functions.

            const_reverse_iterator_impl& operator++()
            {
                ++index;
                return *this;
            }

            const_reverse_iterator_impl operator++(int)
            {
                const_reverse_iterator_impl temp = *this;
                ++index;
                return temp;
            }

            const_reverse_iterator_impl& operator--()
            {
                --index;
                return *this;
            }

            const_reverse_iterator_impl operator--(int)
            {
                const_reverse_iterator_impl temp = *this;
                --index;
                return temp;
            }

            const_reverse_iterator_impl& operator+=(difference_type increment)
            {
                index += increment;
                return *this;
            }

            // operator+
            // is friend operator but operator-
            // is not, because we want to allow the following for "+":
            // iterator+5
            // 5+iterator
            // For the "-" it is not correct to do so, because
            // iterator-5 != 5-iterator
            friend const_reverse_iterator_impl operator+
                (const const_reverse_iterator_impl& lhs, difference_type increment)
            {
                const_reverse_iterator_impl sum;
                sum.r = lhs.r;
                sum.index = lhs.index + increment;
                return sum;
            }

            const_reverse_iterator_impl& operator-=(difference_type decrement)
            {
                index -= decrement;
                return *this;
            }

            const_reverse_iterator_impl operator-(difference_type decrement) const
            {
                const_reverse_iterator_impl shifted_iterator;
                shifted_iterator.r = r;
                shifted_iterator.index = index - decrement;
                return shifted_iterator;
            }

            difference_type operator-(const const_reverse_iterator_impl& rhs) const
            {
                return index - rhs.index;
            }

            value_type operator[](difference_type offset) const
            {
                size_type new_reverse_index
                    = (r->m_element_count - 1) - (index + offset);
                return r->m_first_element + r->m_step*new_reverse_index;
            }

        private:
            basic_range<IntegerType> const * r;
            size_type index;
        };

        typedef IntegerType                        value_type;
        typedef const_iterator_impl                iterator;
        typedef const_iterator_impl                const_iterator;
        typedef const_reverse_iterator_impl        reverse_iterator;
        typedef const_reverse_iterator_impl        const_reverse_iterator;
        typedef value_type&                        reference;
        typedef const value_type&                  const_reference;
        typedef value_type*                        pointer;
        typedef IntegerType                        difference_type;
        typedef std::size_t                        size_type;

        // In the case of default construction,
        // the range is considered as an empty range with no elements.
        // step can be anything other than 0. 1 is
        // an implementation convention, and it doesn't have
        // a significance in this case because the range is empty.
        basic_range(): m_first_element(0), m_element_count(0), m_step(1)
        { }

        // first_element: is begin in specifications.
        // last_element: is end in specifications.
        basic_range(value_type first_element, value_type last_element, value_type step)
            :    m_first_element(first_element),
                m_step(step)
        {
            // We need to count the number of elements.
            // The only case where a range is invalid,
            // when the step=0. It means that the range
            // is infinite, because the number of elements
            // in a range, is the length of that range
            // divided by the difference between 
            // every two successive elements.

            if(step == 0)
                throw std::out_of_range("Invalid Range: step can't be equal to zero!");
            if(first_element < last_element && step < 0)
                throw std::out_of_range("Invalid Range: step can't be backward, while the range is forward!");
            if(first_element > last_element && step > 0)
                throw std::out_of_range("Invalid Range: step can't be forward, while the range is backward!");

            m_element_count = (last_element-first_element)/step;
            if( (last_element-first_element)%step != 0 )
                ++m_element_count;
        }
        
        // The following constructor, determines the step
        // automatically. If the range is forward, then 
        // step will be one. If the range is backward,
        // step will be minus one. If the begin is equal
        // to end, then the step must not equal to zero
        // and it is set to one as a convention.
        basic_range(value_type first_element, value_type last_element)
            : m_first_element(first_element)
        {
            if(last_element >= first_element) *this = basic_range<IntegerType>(first_element, last_element, 1);
            else *this = basic_range<IntegerType>(first_element, last_element, -1);

        }
        
        // The following constructor is a shortcut
        // if you want the first element as zero.
        // the step is determined automatically, based
        // on the last element. If the last element is
        // positive, then step is one, but if it is negative
        // then step is minus one.
        basic_range<IntegerType>(value_type last_element)
            : m_first_element(0)
        {
            if(last_element >= m_first_element) *this = basic_range<IntegerType>(m_first_element, last_element, 1);
            else *this = basic_range<IntegerType>(m_first_element, last_element, -1);
        }

        basic_range<IntegerType>(const basic_range<IntegerType>& r)
            :   m_first_element(r.m_first_element),
                m_element_count(r.m_element_count),
                m_step(r.m_step)
        { }
        
        basic_range<IntegerType>& operator=(const basic_range<IntegerType>& r)
        {
            m_first_element = r.m_first_element;
            m_element_count = r.m_element_count;
            m_step = r.m_step;

            return *this;
        }

        bool operator==(const basic_range<IntegerType>& r) const
        {
            return  m_first_element == r.m_first_element
                    &&
                    m_element_count == r.m_element_count
                    &&
                    m_step == r.m_step;
        }
        
        bool operator!=(const basic_range<IntegerType>& r) const
        {
            return !(*this == r);
        }

        // The following four functions enable the user to compare
        // ranges using ( <, >, <=, >=).
        // The comparison between two ranges is a simple lexicographical
        // comparison(element by element). By convention, if two ranges
        // R1, R2 where R1 has a smaller number of elements. Then if
        // R1 contains more elements but all R1 elements are found in R2
        // R1 is considered less than R2.
        bool operator<(const basic_range<IntegerType>& r) const
        {
            // ********** This function needs refactoring.

            if(m_element_count == 0 && r.m_element_count == 0)
                return false;
            if(m_element_count == 0 && r.m_element_count > 0)
                return true;
            if(m_element_count > 0 && r.m_element_count == 0)
                return false;

            // At this point, both has at least one element.
            if(m_first_element < r.m_first_element)
                return true;
            if(m_first_element > r.m_first_element)
                return false;

            // At this point, the first element of both are equal.
            if(m_element_count == 1 && r.m_element_count == 1)
                return false;
            if(m_element_count == 1 && r.m_element_count > 1)
                return true;
            if(m_element_count > 1 && r.m_element_count == 1)
                return false;

            // At this point, both have at least two elements with
            // a similar first element. Note than the final answer
            // in this case depends on the second element only, because
            // we don't need to compare the elements further.
            // Note that the second element is at (index == 1), because
            // the first element is at (index == 0).
            if(m_first_element+m_step*1 < r.m_first_element+r.m_step*1)
                return true;
            if(m_first_element+m_step*1 > r.m_first_element+r.m_step*1)
                return false;

            // if the first two elements of both ranges are equal, then
            // they are co-linear ranges(because the step is constant).
            // In that case, they comparison depends only on 
            // the size of the ranges by convention.
            return m_element_count < r.m_element_count;
        }

        bool operator>(const basic_range<IntegerType>& r) const
        {
            // ********** This function needs refactoring.

            if(m_element_count == 0 && r.m_element_count == 0)
                return false;
            if(m_element_count == 0 && r.m_element_count > 0)
                return false;
            if(m_element_count > 0 && r.m_element_count == 0)
                return true;

            // At this point, both has at least one element.
            if(m_first_element < r.m_first_element)
                return false;
            if(m_first_element > r.m_first_element)
                return true;

            // At this point, the first element of both are equal.
            if(m_element_count == 1 && r.m_element_count == 1)
                return false;
            if(m_element_count == 1 && r.m_element_count > 1)
                return false;
            if(m_element_count > 1 && r.m_element_count == 1)
                return true;

            // At this point, both have at least two elements with
            // a similar first element. Note than the final answer
            // in this case depends on the second element only, because
            // we don't need to compare the elements further.
            // Note that the second element is at (index == 1), because
            // the first element is at (index == 0).
            if(m_first_element+m_step*1 < r.m_first_element+r.m_step*1)
                return false;
            if(m_first_element+m_step*1 > r.m_first_element+r.m_step*1)
                return true;

            // if the first two elements of both ranges are equal, then
            // they are co-linear ranges(because the step is constant).
            // In that case, they comparison depends only on 
            // the size of the ranges by convention.
            return m_element_count > r.m_element_count;
        }

        bool operator <=(const basic_range<IntegerType>& r) const
        {
            return !(*this > r);
        }

        bool operator >=(const basic_range<IntegerType>& r) const
        {
            return !(*this < r);
        }

        const_iterator begin() const
        {
            return const_iterator(this, 0);
        }

        const_iterator end() const
        {
            return const_iterator(this, m_element_count);
        }

        const_reverse_iterator rbegin() const
        {
            return const_reverse_iterator(this, 0);
        }

        const_reverse_iterator rend() const
        {
            return const_reverse_iterator(this, m_element_count);
        }

        size_type size() const
        {
            return m_element_count;
        }

        size_type max_size() const
        {
            // Because this is an immutable container,
            // max_size() == size()
            return m_element_count;
        }

        bool empty() const
        {
            return m_element_count == 0;
        }

        // exist() and find() are similar except that
        // find() returns the index of the element.
        iterator find(value_type element) const
        {
            value_type element_index = (element - m_first_element) / m_step;
            bool in_range = element_index >= 0 && element_index < m_element_count &&
                            (element - m_first_element) % m_step == 0;
            if(in_range)
                return begin() + element_index;
            return end();
        }
        
        bool exist(value_type element) const
        {
            return find(element) != end();
        }

        // In the standard, the operator[]
        // should return a const reference.
        // Because Range Generator doesn't store its elements
        // internally, we return a copy of the value.
        // In any case, this doesn't affect the semantics of the operator.
        value_type operator[](size_type index) const
        {
            return m_first_element + m_step*index;
        }

    private:
        // m_first_element: begin (see specifications).
        // m_element_count: (end - begin) / step
        value_type m_first_element, m_element_count, m_step;
    };
    
    // This is the default type of range!
    typedef basic_range<int> range;
}

#endif // range_h__