#pragma once

#include <optional>
#include <type_traits>
#include <memory>

namespace it
{
    template<typename ValueType>
    class any_reference
    {
    public:
        using value_type = ValueType;

        static_assert(std::is_same_v<
            value_type, std::remove_cv_t<std::remove_reference_t<value_type>>>,
            "ValueType must be a non-const, non-volatile, non-reference type");

    private:
        template<typename T>
        using is_same_ref = std::is_same<std::remove_reference_t<T>, value_type>;

        template<typename T>
        using is_proxy_ref = std::conjunction<
            std::is_convertible<T, value_type>,
            std::negation<is_same_ref<T>>
        >;

    public:
        template<typename Ref, typename = std::enable_if_t<is_proxy_ref<Ref>::value>>
        any_reference(Ref ref) :
            _self(std::make_unique<ref_proxy<Ref>>(std::move(ref)))
        {}

        template<typename Ref, typename = std::enable_if_t<is_same_ref<Ref>::value>>
        any_reference(Ref& ref) :
            _self(std::make_unique<ref_model<Ref>>(ref))
        {}

        any_reference& operator=(const value_type& val)
        {
            _self->set(val);
            return *this;
        }

        any_reference& operator=(value_type&& val)
        {
            _self->set(std::move(val));
            return *this;
        }

        const value_type& get() const
        {
            return _self->get();
        }

        operator const value_type& () const
        {
            return get();
        }

    private:
        struct ref_concept
        {
            virtual ~ref_concept() = default;

            virtual std::unique_ptr<ref_concept> clone() const = 0;

            virtual const value_type& get() const = 0;
            virtual void set(const value_type&) = 0;
        };

        template<typename Ref>
        struct ref_model final : ref_concept
        {
            std::reference_wrapper<Ref> data;

            ref_model(Ref& x) : data(x) {};

            std::unique_ptr<ref_concept> clone() const override
            {
                return std::make_unique<ref_model>(*this);
            }

            const value_type& get() const override
            {
                return data.get();
            }

            void set(const value_type& val) override
            {
                data.get() = val;
            }
        };

        template<typename Ref>
        struct ref_proxy final : ref_concept
        {
            Ref data;
            mutable std::optional<value_type> value;

            ref_proxy(Ref x) :
                data(std::move(x)),
                value(std::nullopt)
            {};

            std::unique_ptr<ref_concept> clone() const override
            {
                return std::make_unique<ref_proxy>(*this);
            }

            const value_type& get() const override
            {
                value = value_type(data);
                return *value;
            }

            void set(const value_type& val) override
            {
                data = val;
            }
        };

        std::unique_ptr<ref_concept> _self;
    };
}
