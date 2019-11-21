#ifndef CONTAINS_HPP_
#define CONTAINS_HPP_

template <class C, class V>
bool contains(const C& container, const V& val)
{
    return (container.find(val) != container.end());
}

#endif /* CONTAINS_HPP_ */
