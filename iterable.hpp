#pragma once

#include <type_traits>
#include <memory>
#include <iterator>
#include <cassert>

#include <iostream>

namespace it
{
    namespace detail
    {
    }

    template<typename T>
    class iterator
    {
    public:
        static_assert(std::is_same_v<T, std::remove_cv_t<std::remove_reference_t<T>>>,
            "T must be a non-const, non-volatile, non-reference type");

        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = std::input_iterator_tag;

    private:
        template<typename, typename = void>
        struct has_arrow_operator : std::false_type {};

        template<typename U>
        struct has_arrow_operator<U, std::void_t<
            decltype(std::declval<U>().operator->())>
        >
            : std::true_type
        {};

        template<typename It>
        using is_input_iterator = std::is_convertible<
            typename std::iterator_traits<It>::iterator_category, iterator_category>;

        template<typename It>
        using valid_iterator = std::conjunction<
            is_input_iterator<It>,
            std::is_same<value_type, typename std::iterator_traits<It>::value_type>
        >;

        struct it_concept
        {
            virtual ~it_concept() = default;
            virtual std::unique_ptr<it_concept> clone() const = 0;
            virtual void clone_static(void*) const = 0;

            virtual reference operator*() = 0;
            virtual pointer operator->() = 0;
            virtual void operator++() = 0;

            bool operator==(const it_concept& other) const
            {
                return typeid(*this) == typeid(other) && equal(other);
            }

            bool operator!=(const it_concept& other) const
            {
                return !(*this == other);
            }

        private:
            virtual bool equal(const it_concept& other) const = 0;
        };

        template<typename It>
        struct it_model final : it_concept
        {
            It data;

            template<typename U = It,
                typename = std::enable_if_t<valid_iterator<U>::value>
            > explicit it_model(It x) : data(std::move(x)) {}

            std::unique_ptr<it_concept> clone() const override
            {
                return std::make_unique<it_model>(*this);
            }

            void clone_static(void* into) const override
            {
                new (into) auto(*this);
            }

            reference operator*() override
            {
                return *data;
            }

            pointer operator->() override
            {
                if constexpr (std::is_pointer_v<It>)
                    return data;
                else if constexpr (has_arrow_operator<It>::value)
                    return data.operator->();
                else
                    return &(*data);
            }

            void operator++() override
            {
                ++data;
            }

        private:
            bool equal(const it_concept& other) const override
            {
                return data == static_cast<const it_model&>(other).data;
            }
        };

        struct empty_model final : it_concept
        {
            std::unique_ptr<it_concept> clone() const override
            {
                return std::make_unique<empty_model>(*this);
            }

            void clone_static(void* into) const override
            {
                new (into) auto(*this);
            }

            reference operator*() override
            {
                throw std::logic_error("empty_model operator*()");
            }

            pointer operator->() override
            {
                throw std::logic_error("empty_model operator->()");
            }

            void operator++() override {}

        private:
            bool equal(const it_concept&) const override { return true; }
        };

        using aligned_storage =
            std::aligned_storage_t<sizeof(it_model<void*>), alignof(it_model<void*>)>;

        template<typename U>
        struct goes_to_stack
        {
            static constexpr bool value = sizeof(U) <= sizeof(aligned_storage);
        };

        template<typename U>
        struct goes_to_heap
        {
            static constexpr bool value = !goes_to_stack<U>::value;
        };

        template<typename It>
        using stack_iterator =
            std::conjunction<valid_iterator<It>, goes_to_stack<it_model<It>>>;

        template<typename It>
        using heap_iterator =
            std::conjunction<valid_iterator<It>, goes_to_heap<it_model<It>>>;

    public:
        template<typename It, std::enable_if_t<stack_iterator<It>::value, bool> = true>
        iterator(It iter) :
            _on_stack(true)
        {
            new (&_stack) it_model<decltype(iter)>(iter);
        }

        template<typename It, std::enable_if_t<heap_iterator<It>::value, bool> = true>
        iterator(It iter) :
            _on_stack(false),
            _heap(std::make_unique<it_model<decltype(iter)>>(iter))
        {}

        iterator(const iterator& other) :
            _on_stack(other._on_stack)
        {
            transfer(other);
        }

        iterator(iterator&& other) :
            _on_stack(other._on_stack)
        {
            transfer(std::move(other));
        }

        iterator& operator=(const iterator& other)
        {
            if (this != &other)
            {
                this->~iterator();
                _on_stack = other._on_stack;
                transfer(other);
            }
            return *this;
        }

        iterator& operator=(iterator&& other)
        {
            if (this != &other)
            {
                this->~iterator();
                _on_stack = other._on_stack;
                transfer(std::move(other));
            }
            return *this;
        }

        ~iterator()
        {
            std::cout << (_on_stack ? "stack" : "heap") << " destroyed\n";
            if (_on_stack)
                stack_concept().~it_concept();
            else
                _heap.~unique_ptr();
        }

        reference operator*()
        {
            return *get_concept();
        }

        pointer operator->()
        {
            return get_concept().operator->();
        }

        iterator& operator++()
        {
            ++get_concept();
            return *this;
        }

        iterator operator++(int)
        {
            iterator current = *this;
            ++(*this);
            return current;
        }

        friend bool operator==(const iterator& lhs, const iterator& rhs)
        {
            return lhs.get_concept() == rhs.get_concept();
        }

        friend bool operator!=(const iterator& lhs, const iterator& rhs)
        {
            return !(lhs == rhs);
        }

    private:
        void transfer(const iterator& other)
        {
            if (_on_stack)
            {
                std::cout << "stack copy\n";
                other.stack_concept().clone_static(&_stack);
            }
            else
            {
                std::cout << "heap copy\n";
                new (&_heap) auto(other._heap->clone());
            }
        };

        void transfer(iterator&& other)
        {
            if (_on_stack)
            {
                std::cout << "stack move\n";
                new (&_stack) empty_model{};
                std::swap(_stack, other._stack);
            }
            else
            {
                std::cout << "heap move\n";
                new (&_heap) auto(std::move(other._heap));
            }
        }

        it_concept& stack_concept();
        const it_concept& stack_concept() const;
        it_concept& get_concept();
        const it_concept& get_concept() const;

        bool _on_stack;
        union
        {
            aligned_storage _stack;
            std::unique_ptr<it_concept> _heap;
        };
    };

    template<typename T>
    typename iterator<T>::it_concept& iterator<T>::stack_concept()
    {
        using model = it_model<void*>;
        return *std::launder(reinterpret_cast<model*>(&_stack));
    }

    template<typename T>
    const typename iterator<T>::it_concept& iterator<T>::stack_concept() const
    {
        using model = const it_model<void*>;
        return *std::launder(reinterpret_cast<model*>(&_stack));
    }

    template<typename T>
    typename iterator<T>::it_concept& iterator<T>::get_concept()
    {
        return _on_stack ? stack_concept() : *_heap;
    }

    template<typename T>
    const typename iterator<T>::it_concept& iterator<T>::get_concept() const
    {
        return _on_stack ? stack_concept() : *_heap;
    }

    template<typename It>
    iterator(It)->iterator<typename std::iterator_traits<It>::value_type>;
}
