//
// N3949 - Scoped Resource - Generic RAII Wrapper for the Standard Library
// Peter Sommerlad and Andrew L. Sandoval
//
#ifndef UNIQUE_RESOURCE_H_
#define UNIQUE_RESOURCE_H_

#include <type_traits>

#ifndef _MSC_VER
#define UNIQUE_RESOURCE_NOEXCEPT noexpect
#else
#define UNIQUE_RESOURCE_NOEXCEPT
#endif

#if _HAS_EXCEPTIONS
#define UNIQUE_RESOURCE_TRY_BEGIN   try {
#define UNIQUE_RESOURCE_CATCH_ALL   } catch (...) {
#define UNIQUE_RESOURCE_CATCH_END   }
#else
#define UNIQUE_RESOURCE_TRY_BEGIN   {{
#define UNIQUE_RESOURCE_CATCH_ALL   \
    __pragma(warning(push))         \
    __pragma(warning(disable:4127)) \
    } if (0) {                      \
    __pragma(warning(pop))
#define UNIQUE_RESOURCE_CATCH_END   }}
#endif


namespace std {
    namespace experimental {

        enum class invoke_it { once, again };

        template<typename R, typename D>
        class unique_resource_t
        {
            R resource;
            D deleter;
            bool execute_on_destruction; // exposition only
            unique_resource_t& operator=(unique_resource_t const &) = delete;
            unique_resource_t(unique_resource_t const &) = delete; // no copies!

        public:
            // construction
            explicit unique_resource_t(R && resource, D && deleter, bool shouldrun = true) UNIQUE_RESOURCE_NOEXCEPT
                : resource(std::move(resource))
                , deleter(std::move(deleter))
                , execute_on_destruction{ shouldrun }
            {}

            // move
            unique_resource_t(unique_resource_t &&other) UNIQUE_RESOURCE_NOEXCEPT
                : resource(std::move(other.resource))
                , deleter(std::move(other.deleter))
                , execute_on_destruction{ other.execute_on_destruction }
            {
                other.release();
            }

            unique_resource_t& operator=(unique_resource_t &&other) UNIQUE_RESOURCE_NOEXCEPT
            {
                this->invoke(invoke_it::once);
                deleter = std::move(other.deleter);
                resource = std::move(other.resource);
                execute_on_destruction = other.execute_on_destruction;
                other.release();
                return *this;
            }

            // resource release
            ~unique_resource_t()
            {
                this->invoke(invoke_it::once);
            }

            void invoke(invoke_it const strategy = invoke_it::once) UNIQUE_RESOURCE_NOEXCEPT
            {
                if (execute_on_destruction)
                {
                    UNIQUE_RESOURCE_TRY_BEGIN
                    this->get_deleter()(resource);
                    UNIQUE_RESOURCE_CATCH_ALL
                    UNIQUE_RESOURCE_CATCH_END
                }
                execute_on_destruction = strategy == invoke_it::again;
            }

            R const & release() UNIQUE_RESOURCE_NOEXCEPT
            {
                execute_on_destruction = false;
                return this->get();
            }

            void reset(R && newresource) UNIQUE_RESOURCE_NOEXCEPT
            {
                invoke(invoke_it::again);
                resource = std::move(newresource);
            }

            // resource access
            R const & get() const UNIQUE_RESOURCE_NOEXCEPT
            {
                return resource;
            }

            operator R const &() const UNIQUE_RESOURCE_NOEXCEPT
            {
                return resource;
            }

            R operator->() const UNIQUE_RESOURCE_NOEXCEPT
            {
                return resource;
            }

            std::add_lvalue_reference_t<std::remove_pointer_t<R>> operator*() const
            {
                return *resource;
            }

            // deleter access
            const D & get_deleter() const UNIQUE_RESOURCE_NOEXCEPT
            {
                return deleter;
            }
        };

        //factories
        template<typename R, typename D>
        unique_resource_t<R, D> unique_resource(R && r, D t) UNIQUE_RESOURCE_NOEXCEPT
        {
            return unique_resource_t<R, D>(std::move(r), std::move(t), true);
        }

        template<typename R, typename D>
        unique_resource_t<R, D> unique_resource_checked(R r, R invalid, D t) UNIQUE_RESOURCE_NOEXCEPT
        {
            bool shouldrun = (r != invalid);
            return unique_resource_t<R, D>(std::move(r), std::move(t), shouldrun);
        }
    }
}


#undef UNIQUE_RESOURCE_TRY_BEGIN
#undef UNIQUE_RESOURCE_CATCH_ALL
#undef UNIQUE_RESOURCE_CATCH_END
#undef UNIQUE_RESOURCE_NOEXCEPT
#endif /* UNIQUE RESOURCE H */
