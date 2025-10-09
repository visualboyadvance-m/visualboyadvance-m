#ifndef ARRAY_H
#define ARRAY_H

#include <cstddef>
#include <iterator>

template <typename T> class Array
{
        public:
        typedef T value_type;
        typedef size_t size_type;
        typedef ptrdiff_t difference_type;
        typedef T &reference;
        typedef const T &const_reference;
        typedef T *pointer;
        typedef const T *const_pointer;
        typedef T *iterator;
        typedef const T *const_iterator;
        typedef std::reverse_iterator<iterator> reverse_iterator;
        typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

        private:
        pointer m_p;
        size_type m_size;
        bool dealloc;

        public:
        Array(size_type size = 0)
        {
            m_p = NULL;
            dealloc = false;

            if (size) {
                m_p = new value_type[size];
                m_size = size;
                dealloc = true;
            }
        }

        void reset(size_t size)
        {
                if (this->m_p) {
                    delete[] this->m_p;
                    this->m_p = NULL;
                }

                if (size) {
                    this->m_p = new value_type[size];
                    dealloc = true;
                }
                this->m_size = size;
        }

        ~Array()
        {
            if (dealloc) delete[] m_p;
        }

        size_type size() const
        {
                return (this->m_size);
        }

        operator pointer()
        {
                return (this->m_p);
        }
        operator const_pointer() const
        {
                return (this->m_p);
        }

        /// TODO: Add more functions from here: http://en.cppreference.com/w/cpp/container/array
};
#endif
