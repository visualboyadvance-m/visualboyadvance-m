#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <algorithm>
#include <iterator>
#include <cstddef>
#include "array.h"


template <typename T> class RingBuffer
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
  Array<T> m_buffer;
  size_type m_size, m_pos_read, m_pos_write;

  public:
  RingBuffer(size_type size = 0) : m_size(0), m_pos_read(0), m_pos_write(0)
  {
    this->reset(size);
  }

  void reset(size_type size)
  {
    this->m_size = size+1; //Add one to allow for a seperator between the write and read pointers, and avoid various issues
    this->m_pos_read = this->m_pos_write = 0;
    this->m_buffer.reset(size ? this->m_size : 0);
  }

  size_type size() const
  {
    return(this->m_size-1);
  }

  void clear()
  {
    this->m_pos_read = this->m_pos_write = 0;
  }

  void fill(T value)
  {
    std::fill(this->m_buffer+0, this->m_buffer+this->m_buffer->size(), value);
    this->m_pos_read = 0;
    this->m_pos_write = this->size();
  }

  size_type avail() const
  {
    return((this->m_size-1) - this->used()); //Lose one from the size to prevent writing an entry casuing write to equal read pointer and causing other checks to think the buffer is empty
  }

  size_type used() const
  {
    return
    (
      (this->m_pos_write < this->m_pos_read) ?
        this->m_pos_write + (this->m_size - this->m_pos_read) :
        this->m_pos_write - this->m_pos_read
    );
  }

  void read(pointer buffer, size_type size)
  {
    size_type amount = std::min(size, this->m_size-this->m_pos_read);
    std::copy(this->m_buffer+this->m_pos_read, this->m_buffer+this->m_pos_read+amount, buffer);
    this->m_pos_read = (this->m_pos_read + amount) % this->m_size;
    size -= amount;
    std::copy(this->m_buffer+this->m_pos_read, this->m_buffer+this->m_pos_read+size, buffer+amount);
    this->m_pos_read = (this->m_pos_read + size) % this->m_size;
  }

  void write(const_pointer buffer, size_type size)
  {
    size_type amount = std::min(size, this->m_size-this->m_pos_write);
    std::copy(buffer, buffer+amount, this->m_buffer+m_pos_write);
    this->m_pos_write = (this->m_pos_write + amount) % this->m_size;
    std::copy(buffer+amount, buffer+size, this->m_buffer+m_pos_write);
    size -= amount;
    this->m_pos_write = (this->m_pos_write + size) % this->m_size;
  }
};

#endif
