/*
 *  Copyright (c) 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef KRITA_CONTAINER_UTILS_H
#define KRITA_CONTAINER_UTILS_H

#include <functional>
#include <QList>

namespace KritaUtils
{

template <class T>
    bool compareListsUnordered(const QList<T> &a, const QList<T> &b) {
    if (a.size() != b.size()) return false;

    Q_FOREACH(const T &t, a) {
        if (!b.contains(t)) return false;
    }

    return true;
}

template <class C>
    void makeContainerUnique(C &container) {
    std::sort(container.begin(), container.end());
    auto newEnd = std::unique(container.begin(), container.end());

    while (newEnd != container.end()) {
        newEnd = container.erase(newEnd);
    }
}


template <class C, typename KeepIfFunction>
    auto filterContainer(C &container, KeepIfFunction keepIf)
        -> decltype(bool(keepIf(container[0])), void()) {

        auto newEnd = std::remove_if(container.begin(), container.end(), [keepIf] (typename C::reference p) { return !keepIf(p); });
        while (newEnd != container.end()) {
           newEnd = container.erase(newEnd);
        }
}

template<typename T>
struct is_container
{
    typedef typename std::remove_const<T>::type test_type;

    template<typename A>
    static constexpr bool test(
            A *pointer,
            A const *const_pointer = nullptr,
            decltype(pointer->begin()) * = nullptr,
            decltype(pointer->end()) * = nullptr,
            decltype(const_pointer->begin()) * = nullptr,
            decltype(const_pointer->end()) * = nullptr,
            typename A::iterator * it = nullptr,
            typename A::const_iterator * constIt = nullptr,
            typename A::value_type * = nullptr) {

        typedef typename A::iterator iterator;
        typedef typename A::const_iterator const_iterator;
        typedef typename A::value_type value_type;
        return  std::is_same<decltype(pointer->begin()),iterator>::value &&
                std::is_same<decltype(pointer->end()),iterator>::value &&
                std::is_same<decltype(const_pointer->begin()),const_iterator>::value &&
                std::is_same<decltype(const_pointer->end()),const_iterator>::value &&
                std::is_same<decltype(**it),value_type &>::value &&
                std::is_same<decltype(**constIt),value_type const &>::value;

    }

    template<typename A>
    static constexpr bool test(...) {
        return false;
    }

    static const bool value = test<test_type>(nullptr);
};

template<typename T>
struct is_appendable_container
{
    typedef typename std::remove_const<T>::type test_type;

    template<typename A, typename R = decltype(std::declval<A&>().push_back(std::declval<typename A::value_type>()))>
        static constexpr bool test(A *pointer) {
            return  is_container<A>::value && std::is_same<R, void>::value;
        }

    template<typename A>
    static constexpr bool test(...) {
        return false;
    }

    static const bool value = test<test_type>(nullptr);
};

}


#endif // KRITA_CONTAINER_UTILS_H

