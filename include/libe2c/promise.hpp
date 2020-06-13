/**
 * mit license
 * copyright (c) 2018 ted yin <tederminant@gmail.com>
 *
 * permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "software"), to deal
 * in the software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the software, and to permit persons to whom the software is
 * furnished to do so, subject to the following conditions:
 *
 * the above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the software.
 *
 * the software is provided "as is", without warranty of any kind, express or
 * implied, including but not limited to the warranties of merchantability,
 * fitness for a particular purpose and noninfringement. in no event shall the
 * authors or copyright holders be liable for any claim, damages or other
 * liability, whether in an action of contract, tort or otherwise, arising from,
 * out of or in connection with the software or the use or other dealings in the
 * software.
 */

#pragma once

#include <stack>
#include <vector>
#include <memory>
#include <functional>
#include <type_traits>

#if __cplusplus >= 201703l
#ifdef __has_include
#   if __has_include(<any>)
#       include <any>
#       ifdef __cpp_lib_any
#           define _cppromise_std_any
#       endif
#   endif
#endif
#endif

#ifndef _cppromise_std_any
#include <boost/any.hpp>
#endif

/**
 * implement type-safe promise primitives similar to the ones specified by
 * javascript promise/a+.
 */
namespace promise {
#ifdef _cppromise_std_any
    using pm_any_t = std::any;
    template<typename t>
    constexpr auto any_cast = static_cast<t(*)(const std::any&)>(std::any_cast<t>);
    using bad_any_cast = std::bad_any_cast;
#else
#   warning "using boost::any"
#   pragma message "using boost::any"
    using pm_any_t = boost::any;
    template<typename t>
    constexpr auto any_cast = static_cast<t(*)(const boost::any&)>(boost::any_cast<t>);
    using bad_any_cast = boost::bad_any_cast;
#endif
    using callback_t = std::function<void()>;
    using values_t = std::vector<pm_any_t>;

    /* match lambdas */
    template<typename t>
    struct function_traits_impl:
        public function_traits_impl<decltype(&t::operator())> {};

    template<typename returntype>
    struct function_traits_impl<returntype()> {
        using ret_type = returntype;
        using arg_type = void;
        using empty_arg = void;
    };

    /* match plain functions */
    template<typename returntype, typename argtype>
    struct function_traits_impl<returntype(argtype)> {
        using ret_type = returntype;
        using arg_type = argtype;
        using non_empty_arg = void;
    };

    /* match function pointers */
    template<typename returntype, typename... argtype>
    struct function_traits_impl<returntype(*)(argtype...)>:
        public function_traits_impl<returntype(argtype...)> {};

    /* match const member functions */
    template<typename classtype, typename returntype, typename... argtype>
    struct function_traits_impl<returntype(classtype::*)(argtype...) const>:
        public function_traits_impl<returntype(argtype...)> {};

    /* match member functions */
    template<typename classtype, typename returntype, typename... argtype>
    struct function_traits_impl<returntype(classtype::*)(argtype...)>:
        public function_traits_impl<returntype(argtype...)> {};

    template<typename func>
    struct function_traits:
        public function_traits_impl<typename std::remove_reference_t<func>> {};

    template<typename func, typename returntype>
    using enable_if_return = typename std::enable_if_t<
            std::is_same<typename function_traits<func>::ret_type,
                         returntype>::value>;

    template<typename func, typename returntype>
    using disable_if_return = typename std::enable_if_t<
            !std::is_same<typename function_traits<func>::ret_type,
                         returntype>::value>;

    template<typename func, typename argtype>
    using enable_if_arg = typename std::enable_if_t<
            std::is_same<typename function_traits<func>::arg_type,
                         argtype>::value>;

    template<typename func, typename argtype>
    using disable_if_arg = typename std::enable_if_t<
            !std::is_same<typename function_traits<func>::arg_type,
                         argtype>::value>;

    template<typename t, typename u>
    using disable_if_same_ref = typename std::enable_if_t<
            !std::is_same<
                std::remove_cv_t<std::remove_reference_t<t>>, u>::value>;

    class promise;
    //class promise_t: public std::shared_ptr<promise> {
    class promise_t {
        promise *pm;
        size_t *ref_cnt;
        public:
        friend promise;
        template<typename plist> friend promise_t all(const plist &promise_list);
        template<typename plist> friend promise_t race(const plist &promise_list);

        inline promise_t();
        inline ~promise_t();
        template<typename func, disable_if_same_ref<func, promise_t> * = nullptr>
                inline promise_t(func &&callback);

        void swap(promise_t &other) {
            std::swap(pm, other.pm);
            std::swap(ref_cnt, other.ref_cnt);
        }

        promise_t &operator=(const promise_t &other) {
            if (this != &other)
            {
                promise_t tmp(other);
                tmp.swap(*this);
            }
            return *this;
        }

        promise_t &operator=(promise_t &&other) {
            if (this != &other)
            {
                promise_t tmp(std::move(other));
                tmp.swap(*this);
            }
            return *this;
        }

        promise_t(const promise_t &other):
            pm(other.pm),
            ref_cnt(other.ref_cnt) {
            ++*ref_cnt;
        }

        promise_t(promise_t &&other):
            pm(other.pm),
            ref_cnt(other.ref_cnt) {
            other.pm = nullptr;
        }

        promise *operator->() const {
            return pm;
        }

        template<typename t> inline void resolve(t result) const;
        template<typename t> inline void reject(t reason) const;
        inline void resolve() const;
        inline void reject() const;

        template<typename funcfulfilled>
        inline promise_t then(funcfulfilled &&on_fulfilled) const;

        template<typename funcfulfilled, typename funcrejected>
        inline promise_t then(funcfulfilled &&on_fulfilled,
                            funcrejected &&on_rejected) const;

        template<typename funcrejected>
        inline promise_t fail(funcrejected &&on_rejected) const;
    };

#define promise_err_invalid_state do {throw std::runtime_error("invalid promise state");} while (0)
#define promise_err_mismatch_type do {throw std::runtime_error("mismatching promise value types");} while (0)

    class promise {
        template<typename plist> friend promise_t all(const plist &promise_list);
        template<typename plist> friend promise_t race(const plist &promise_list);
        std::vector<callback_t> fulfilled_callbacks;
        std::vector<callback_t> rejected_callbacks;
#ifdef cppromise_use_stack_free
        std::vector<promise *> fulfilled_pms;
        std::vector<promise *> rejected_pms;
#endif
        enum class state {
            pending,
#ifdef cppromise_use_stack_free
            prefulfilled,
            prerejected,
#endif
            fulfilled,
            rejected,
        } state;
        pm_any_t result;
        pm_any_t reason;

        void add_on_fulfilled(callback_t &&cb) {
            fulfilled_callbacks.push_back(std::move(cb));
        }

        void add_on_rejected(callback_t &&cb) {
            rejected_callbacks.push_back(std::move(cb));
        }

        template<typename func,
            typename function_traits<func>::non_empty_arg * = nullptr>
        static constexpr auto cps_transform(
                func &&f, const pm_any_t &result, const promise_t &npm) {
            return [&result, npm, f = std::forward<func>(f)]() mutable {
#ifndef cppromise_use_stack_free
                f(result)->then(
                    [npm] (pm_any_t result) {npm->resolve(result);},
                    [npm] (pm_any_t reason) {npm->reject(reason);});
#else
                promise_t rpm{f(result)};
                rpm->then(
                    [rpm, npm] (pm_any_t result) {
                        npm->_resolve(result);
                    },
                    [rpm, npm] (pm_any_t reason) {
                        npm->_reject(reason);
                    });
                rpm->_dep_resolve(npm);
                rpm->_dep_reject(npm);
#endif
            };
        }

        template<typename func,
            typename function_traits<func>::empty_arg * = nullptr>
        static constexpr auto cps_transform(
                func &&f, const pm_any_t &, const promise_t &npm) {
            return [npm, f = std::forward<func>(f)]() mutable {
#ifndef cppromise_use_stack_free
                f()->then(
                    [npm] (pm_any_t result) {npm->resolve(result);},
                    [npm] (pm_any_t reason) {npm->reject(reason);});
#else
                promise_t rpm{f()};
                rpm->then(
                    [rpm, npm] (pm_any_t result) {
                        npm->_resolve(result);
                    },
                    [rpm, npm] (pm_any_t reason) {
                        npm->_reject(reason);
                    });
                rpm->_dep_resolve(npm);
                rpm->_dep_reject(npm);
#endif
            };
        }

        template<typename func,
            enable_if_return<func, promise_t> * = nullptr>
        constexpr auto gen_on_fulfilled(func &&on_fulfilled, const promise_t &npm) {
            return cps_transform(std::forward<func>(on_fulfilled), this->result, npm);
        }

        template<typename func,
            enable_if_return<func, promise_t> * = nullptr>
        constexpr auto gen_on_rejected(func &&on_rejected, const promise_t &npm) {
            return cps_transform(std::forward<func>(on_rejected), this->reason, npm);
        }


        template<typename func,
            enable_if_return<func, void> * = nullptr,
            typename function_traits<func>::non_empty_arg * = nullptr>
        constexpr auto gen_on_fulfilled(func &&on_fulfilled, const promise_t &npm) {
            return [this, npm,
                    on_fulfilled = std::forward<func>(on_fulfilled)]() mutable {
                on_fulfilled(result);
                npm->_resolve();
            };
        }

        template<typename func,
            enable_if_return<func, void> * = nullptr,
            typename function_traits<func>::empty_arg * = nullptr>
        constexpr auto gen_on_fulfilled(func &&on_fulfilled, const promise_t &npm) {
            return [on_fulfilled = std::forward<func>(on_fulfilled), npm]() mutable {
                on_fulfilled();
                npm->_resolve();
            };
        }

        template<typename func,
            enable_if_return<func, void> * = nullptr,
            typename function_traits<func>::non_empty_arg * = nullptr>
        constexpr auto gen_on_rejected(func &&on_rejected, const promise_t &npm) {
            return [this, npm,
                    on_rejected = std::forward<func>(on_rejected)]() mutable {
                on_rejected(reason);
                npm->_reject();
            };
        }

        template<typename func,
            enable_if_return<func, void> * = nullptr,
            typename function_traits<func>::empty_arg * = nullptr>
        constexpr auto gen_on_rejected(func &&on_rejected, const promise_t &npm) {
            return [npm,
                    on_rejected = std::forward<func>(on_rejected)]() mutable {
                on_rejected();
                npm->_reject();
            };
        }

        template<typename func,
            enable_if_return<func, pm_any_t> * = nullptr,
            typename function_traits<func>::non_empty_arg * = nullptr>
        constexpr auto gen_on_fulfilled(func &&on_fulfilled, const promise_t &npm) {
            return [this, npm,
                    on_fulfilled = std::forward<func>(on_fulfilled)]() mutable {
                npm->_resolve(on_fulfilled(result));
            };
        }

        template<typename func,
            enable_if_return<func, pm_any_t> * = nullptr,
            typename function_traits<func>::empty_arg * = nullptr>
        constexpr auto gen_on_fulfilled(func &&on_fulfilled, const promise_t &npm) {
            return [npm, on_fulfilled = std::forward<func>(on_fulfilled)]() mutable {
                npm->_resolve(on_fulfilled());
            };
        }

        template<typename func,
            enable_if_return<func, pm_any_t> * = nullptr,
            typename function_traits<func>::non_empty_arg * = nullptr>
        constexpr auto gen_on_rejected(func &&on_rejected, const promise_t &npm) {
            return [this, npm, on_rejected = std::forward<func>(on_rejected)]() mutable {
                npm->_reject(on_rejected(reason));
            };
        }

        template<typename func,
            enable_if_return<func, pm_any_t> * = nullptr,
            typename function_traits<func>::empty_arg * = nullptr>
        constexpr auto gen_on_rejected(func &&on_rejected, const promise_t &npm) {
            return [npm, on_rejected = std::forward<func>(on_rejected)]() mutable {
                npm->_reject(on_rejected());
            };
        }

#ifdef cppromise_use_stack_free
        void _trigger() {
            std::stack<std::tuple<
                std::vector<promise *>::const_iterator,
                std::vector<promise *> *,
                promise *>> s;
            auto push_frame = [&s](promise *pm) {
                if (pm->state == state::prefulfilled)
                {
                    pm->state = state::fulfilled;
                    for (auto &cb: pm->fulfilled_callbacks) cb();
                    s.push(std::make_tuple(pm->fulfilled_pms.begin(),
                                          &pm->fulfilled_pms,
                                          pm));
                }
                else if (pm->state == state::prerejected)
                {
                    pm->state = state::rejected;
                    for (auto &cb: pm->rejected_callbacks) cb();
                    s.push(std::make_tuple(pm->rejected_pms.begin(),
                                          &pm->rejected_pms,
                                          pm));
                }
            };
            push_frame(this);
            while (!s.empty())
            {
                auto &u = s.top();
                auto &it = std::get<0>(u);
                auto vec = std::get<1>(u);
                auto pm = std::get<2>(u);
                if (it == vec->end())
                {
                    s.pop();
                    vec->clear();
                    pm->fulfilled_callbacks.clear();
                    pm->rejected_callbacks.clear();
                    continue;
                }
                push_frame(*it++);
            }
        }

        void trigger_fulfill() {
            state = state::prefulfilled;
            _trigger();
        }

        void trigger_reject() {
            state = state::prerejected;
            _trigger();
        }

        void _resolve() {
            if (state == state::pending) state = state::prefulfilled;
        }

        void _reject() {
            if (state == state::pending) state = state::prerejected;
        }

        void _dep_resolve(const promise_t &npm) {
            if (state == state::pending)
                fulfilled_pms.push_back(npm.pm);
            else
                npm->_trigger();
        }

        void _dep_reject(const promise_t &npm) {
            if (state == state::pending)
                rejected_pms.push_back(npm.pm);
            else
                npm->_trigger();
        }

        void _resolve(pm_any_t _result) {
            if (state == state::pending)
            {
                result = _result;
                state = state::prefulfilled;
            }
        }

        void _reject(pm_any_t _reason) {
            if (state == state::pending)
            {
                reason = _reason;
                state = state::prerejected;
            }
        }
#else
        void _resolve() { resolve(); }
        void _reject() { reject(); }
        void _resolve(pm_any_t result) { resolve(result); }
        void _reject(pm_any_t reason) { reject(reason); }

        void trigger_fulfill() {
            state = state::fulfilled;
            for (const auto &cb: fulfilled_callbacks) cb();
            fulfilled_callbacks.clear();
        }

        void trigger_reject() {
            state = state::rejected;
            for (const auto &cb: rejected_callbacks) cb();
            rejected_callbacks.clear();
        }
#endif
        public:

        promise(): state(state::pending) {}
        ~promise() {}

        template<typename funcfulfilled, typename funcrejected>
        promise_t then(funcfulfilled &&on_fulfilled,
                      funcrejected &&on_rejected) {
            switch (state)
            {
                case state::pending:
                return promise_t([this,
                                on_fulfilled = std::forward<funcfulfilled>(on_fulfilled),
                                on_rejected = std::forward<funcrejected>(on_rejected)
                                ](promise_t &npm) {
                    add_on_fulfilled(gen_on_fulfilled(std::move(on_fulfilled), npm));
                    add_on_rejected(gen_on_rejected(std::move(on_rejected), npm));
#ifdef cppromise_use_stack_free
                    _dep_resolve(npm);
                    _dep_reject(npm);
#endif
                });
                case state::fulfilled:
                return promise_t([this,
                                on_fulfilled = std::forward<funcfulfilled>(on_fulfilled)
                                ](promise_t &npm) {
                    gen_on_fulfilled(std::move(on_fulfilled), npm)();
                });
                case state::rejected:
                return promise_t([this,
                                on_rejected = std::forward<funcrejected>(on_rejected)
                                ](promise_t &npm) {
                    gen_on_rejected(std::move(on_rejected), npm)();
                });
                default: promise_err_invalid_state;
            }
        }

        template<typename funcfulfilled>
        promise_t then(funcfulfilled &&on_fulfilled) {
            switch (state)
            {
                case state::pending:
                return promise_t([this,
                                on_fulfilled = std::forward<funcfulfilled>(on_fulfilled)
                                ](promise_t &npm) {
                    add_on_fulfilled(gen_on_fulfilled(std::move(on_fulfilled), npm));
                    add_on_rejected([this, npm]() {npm->_reject(reason);});
#ifdef cppromise_use_stack_free
                    _dep_resolve(npm);
                    _dep_reject(npm);
#endif
                });
                case state::fulfilled:
                return promise_t([this,
                                on_fulfilled = std::forward<funcfulfilled>(on_fulfilled)
                                ](promise_t &npm) {
                    gen_on_fulfilled(std::move(on_fulfilled), npm)();
                });
                case state::rejected:
                return promise_t([this](promise_t &npm) {npm->_reject(reason);});
                default: promise_err_invalid_state;
            }
        }

        template<typename funcrejected>
        promise_t fail(funcrejected &&on_rejected) {
            switch (state)
            {
                case state::pending:
                return promise_t([this,
                                on_rejected = std::forward<funcrejected>(on_rejected)
                                ](promise_t &npm) {
                    callback_t ret;
                    add_on_rejected(gen_on_rejected(std::move(on_rejected), npm));
                    add_on_fulfilled([this, npm]() {npm->_resolve(result);});
#ifdef cppromise_use_stack_free
                    _dep_resolve(npm);
                    _dep_reject(npm);
#endif
                });
                case state::fulfilled:
                return promise_t([this](promise_t &npm) {npm->_resolve(result);});
                case state::rejected:
                return promise_t([this,
                                on_rejected = std::forward<funcrejected>(on_rejected)
                                ](promise_t &npm) {
                    gen_on_rejected(std::move(on_rejected), npm)();
                });
                default: promise_err_invalid_state;
            }
        }

        void resolve() {
            if (state == state::pending) trigger_fulfill();
        }

        void reject() {
            if (state == state::pending) trigger_reject();
        }

        void resolve(pm_any_t _result) {
            if (state == state::pending)
            {
                result = _result;
                trigger_fulfill();
            }
        }

        void reject(pm_any_t _reason) {
            if (state == state::pending)
            {
                reason = _reason;
                trigger_reject();
            }
        }
    };

    template<typename plist> promise_t all(const plist &promise_list) {
        return promise_t([&promise_list] (promise_t &npm) {
            auto size = std::make_shared<size_t>(promise_list.size());
            auto results = std::make_shared<values_t>();
            if (!*size) promise_err_mismatch_type;
            results->resize(*size);
            size_t idx = 0;
            for (const auto &pm: promise_list) {
                pm->then(
                    [results, size, idx, npm](pm_any_t result) {
                        (*results)[idx] = result;
                        if (!--(*size))
                            npm->_resolve(*results);
                    },
                    [npm](pm_any_t reason) {npm->_reject(reason);});
#ifdef cppromise_use_stack_free
                pm->_dep_resolve(npm);
                pm->_dep_reject(npm);
#endif
                idx++;
            }
        });
    }

    template<typename plist> promise_t race(const plist &promise_list) {
        return promise_t([&promise_list] (promise_t &npm) {
            for (const auto &pm: promise_list) {
                pm->then([npm](pm_any_t result) {npm->_resolve(result);},
                        [npm](pm_any_t reason) {npm->_reject(reason);});
#ifdef cppromise_use_stack_free
                pm->_dep_resolve(npm);
                pm->_dep_reject(npm);
#endif
            }
        });
    }

    template<typename func, disable_if_same_ref<func, promise_t> *>
    inline promise_t::promise_t(func &&callback):
            pm(new promise()),
        ref_cnt(new size_t(1)) {
        callback(*this);
    }

    inline promise_t::promise_t():
        pm(new promise()),
        ref_cnt(new size_t(1)) {}

    inline promise_t::~promise_t() {
        if (pm)
        {
            if (--*ref_cnt) return;
            delete pm;
            delete ref_cnt;
        }
    }

    template<typename t>
    inline void promise_t::resolve(t result) const { (*this)->resolve(result); }

    template<typename t>
    inline void promise_t::reject(t reason) const { (*this)->reject(reason); }

    inline void promise_t::resolve() const { (*this)->resolve(); }
    inline void promise_t::reject() const { (*this)->reject(); }

    template<typename t>
    struct callback_types {
        using arg_type = typename function_traits<t>::arg_type;
        using ret_type = typename std::conditional<
            std::is_same<typename function_traits<t>::ret_type, promise_t>::value,
            promise_t, pm_any_t>::type;
    };

    template<typename func,
        disable_if_arg<func, pm_any_t> * = nullptr,
        enable_if_return<func, void> * = nullptr,
        typename function_traits<func>::non_empty_arg * = nullptr>
    constexpr auto gen_any_callback(func &&f) {
        using func_t = callback_types<func>;
        return [f = std::forward<func>(f)](pm_any_t v) mutable {
            try {
                f(any_cast<typename func_t::arg_type>(v));
            } catch (bad_any_cast &e) { promise_err_mismatch_type; }
        };
    }

    template<typename func,
        enable_if_arg<func, pm_any_t> * = nullptr,
        enable_if_return<func, void> * = nullptr,
        typename function_traits<func>::non_empty_arg * = nullptr>
    constexpr auto gen_any_callback(func &&f) {
        return [f = std::forward<func>(f)](pm_any_t v) mutable {f(v);};
    }

    template<typename func,
        enable_if_return<func, void> * = nullptr,
        typename function_traits<func>::empty_arg * = nullptr>
    constexpr auto gen_any_callback(func &&f) { return std::forward<func>(f); }

    template<typename func,
        enable_if_arg<func, pm_any_t> * = nullptr,
        disable_if_return<func, void> * = nullptr,
        typename function_traits<func>::non_empty_arg * = nullptr>
    constexpr auto gen_any_callback(func &&f) {
        using func_t = callback_types<func>;
        return [f = std::forward<func>(f)](pm_any_t v) mutable {
            return typename func_t::ret_type(f(v));
        };
    }

    template<typename func,
        disable_if_arg<func, pm_any_t> * = nullptr,
        disable_if_return<func, void> * = nullptr,
        typename function_traits<func>::non_empty_arg * = nullptr>
    constexpr auto gen_any_callback(func &&f) {
        using func_t = callback_types<func>;
        return [f = std::forward<func>(f)](pm_any_t v) mutable {
            try {
                return typename func_t::ret_type(
                    f(any_cast<typename func_t::arg_type>(v)));
            } catch (bad_any_cast &e) { promise_err_mismatch_type; }
        };
    }

    template<typename func,
        disable_if_return<func, void> * = nullptr,
        typename function_traits<func>::empty_arg * = nullptr>
    constexpr auto gen_any_callback(func &&f) {
        using func_t = callback_types<func>;
        return [f = std::forward<func>(f)]() mutable {
            return typename func_t::ret_type(f());
        };
    }

    template<typename funcfulfilled>
    inline promise_t promise_t::then(funcfulfilled &&on_fulfilled) const {
        return (*this)->then(gen_any_callback(std::forward<funcfulfilled>(on_fulfilled)));
    }

    template<typename funcfulfilled, typename funcrejected>
    inline promise_t promise_t::then(funcfulfilled &&on_fulfilled,
                                    funcrejected &&on_rejected) const {
        return (*this)->then(gen_any_callback(std::forward<funcfulfilled>(on_fulfilled)),
                            gen_any_callback(std::forward<funcrejected>(on_rejected)));
    }

    template<typename funcrejected>
    inline promise_t promise_t::fail(funcrejected &&on_rejected) const {
        return (*this)->fail(gen_any_callback(std::forward<funcrejected>(on_rejected)));
    }
}
