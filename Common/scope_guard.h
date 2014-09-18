//
// N3949 - Scoped Resource - Generic RAII Wrapper for the Standard Library
// Peter Sommerlad and Andrew L. Sandoval
//
#ifndef SCOPE_GUARD_H_
#define SCOPE_GUARD_H_

#ifndef _MSC_VER
#define SCOPE_GUARD_NOEXCEPT noexcept
#else
#define SCOPE_GUARD_NOEXCEPT
#endif

#if _HAS_EXCEPTIONS
#define SCOPE_GUARD_TRY_BEGIN   try {
#define SCOPE_GUARD_CATCH_ALL   } catch (...) {
#define SCOPE_GUARD_CATCH_END   }
#else
#define SCOPE_GUARD_TRY_BEGIN   {{
#define SCOPE_GUARD_CATCH_ALL       \
    __pragma(warning(push))         \
    __pragma(warning(disable:4127)) \
    } if (0) {                      \
    __pragma(warning(pop))
#define SCOPE_GUARD_CATCH_END   }}
#endif


// modeled slightly after Andrescu's talk and article(s)
namespace std {
    namespace experimental {

        template <typename D>
        struct scope_guard_t
        {
            // construction
            explicit scope_guard_t(D &&f) SCOPE_GUARD_NOEXCEPT
                : deleter(std::move(f))
                , execute_on_destruction{ true }
            {}

            // move
            scope_guard_t(scope_guard_t &&rhs) SCOPE_GUARD_NOEXCEPT
                : deleter(std::move(rhs.deleter))
                , execute_on_destruction{ rhs.execute_on_destruction }
            {
                rhs.release();
            }

            // release
            ~scope_guard_t()
            {
                if (execute_on_destruction)
                {
                    SCOPE_GUARD_TRY_BEGIN
                    deleter();
                    SCOPE_GUARD_CATCH_ALL
                    SCOPE_GUARD_CATCH_END
                }
            }

            void release() SCOPE_GUARD_NOEXCEPT 
            { 
                execute_on_destruction = false; 
            }

        private:
            scope_guard_t(scope_guard_t const &) = delete;
            void operator=(scope_guard_t const &) = delete;
            scope_guard_t& operator=(scope_guard_t &&) = delete;
            D deleter;
            bool execute_on_destruction; // exposition only
        };

        // usage: auto guard=scope guard([] std::cout << "done...";);
        template <typename D>
        scope_guard_t<D> scope_guard(D && deleter) SCOPE_GUARD_NOEXCEPT 
        {
            return scope_guard_t<D>(std::move(deleter)); // fails with curlies
        }
    }
}


#undef SCOPE_GUARD_TRY_BEGIN
#undef SCOPE_GUARD_CATCH_ALL
#undef SCOPE_GUARD_CATCH_END
#undef SCOPE_GUARD_NOEXCEPT
#endif /* SCOPE GUARD H */
