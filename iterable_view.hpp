#pragma once

#include "iterable.hpp"

#include <iterator>

namespace it
{
    template<typename ValueType>
    class iterable_view
    {
    public:
        using value_type = ValueType;

        static_assert(std::is_same_v<
            value_type, std::remove_cv_t<std::remove_reference_t<value_type>>>,
            "ValueType must be a non-const, non-volatile, non-reference type");

        template<typename It>
        iterable_view(It first, It last) :
            _first(first),
            _last(last)
        {}

        template<typename Iterable>
        iterable_view(Iterable& it) :
            _first(std::begin(it)),
            _last(std::end(it))
        {}

        it::iterator<value_type>& begin()
        {
            return _first;
        }

        it::iterator<value_type>& end()
        {
            return _last;
        }

    private:
        it::iterator<value_type> _first;
        it::iterator<value_type> _last;
    };

    template<typename It>
    iterable_view(It, It)->
        iterable_view<typename std::iterator_traits<It>::value_type>;

    template<typename Iterable>
    iterable_view(Iterable&)->
        iterable_view<typename std::iterator_traits<
        decltype(std::begin(std::declval<Iterable>()))>::value_type>;
}
