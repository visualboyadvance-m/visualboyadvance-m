#ifndef ARRAY_H
#define ARRAY_H

#include <iterator>
#include <cstddef>

template<typename T>
class Array
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

  public:

  Array(size_type size = 0) : m_p(size ? new value_type[size] : 0), m_size(size) {}

  void reset(size_t size)
  {
    delete[] this->m_p;
    this->m_p = size ? new value_type[size] : 0;
    this->m_size = size;
  }

  size_type size() const { return(this->m_size); }

  operator pointer() { return(this->m_p); }
  operator const_pointer() const { return(this->m_p); }

  ///TODO: Add more functions from here: http://en.cppreference.com/w/cpp/container/array
};
#endif
