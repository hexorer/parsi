#ifndef PARSI_FN_ALTERS_HPP
#define PARSI_FN_ALTERS_HPP

#include "parsi/base.hpp"

namespace parsi::fn {
/**
 * A combinator that consists of the a `condition` and a `successor` parser,
 * which passes the result of the `condition` parser to the `successor` parser.
 *
 * This type is rather meant to be used by multiple alternatives combinator.
 * @see AlterSet
 */
template <is_parser ConditionF, is_parser SuccessorF>
struct Alter {
    ConditionF condition_parser;
    SuccessorF successor_parser;

    [[nodiscard]] constexpr Result operator()(Stream stream) const {
        Result result = condition_parser(stream);
        if (!result) {
            return Result{stream, false};
        }
        return successor_parser(result.stream());
    }
};

/**
 * A combinator that alters between alternatives' conditions until a condition is met,
 * then the stream is passed on to that alternative's successor without any further
 * alternations on parser failure.
 *
 * The chosen alternative's successor parser's result is propagated regardless of validity.
 */
template <is_parser ...Fs>
struct AlterSet {
    static_assert(sizeof...(Fs) == 0, "all types must be an instantiation of Alter.");
    static_assert(sizeof...(Fs) != 0, "all types must be an instantiation of Alter.");
};

template <typename F1, typename G1>
struct AlterSet<Alter<F1, G1>> {
    Alter<F1, G1> parser;

    [[nodiscard]] constexpr Result operator()(Stream stream) const {
        return parser(stream);
    }
};

template <typename F1, typename G1, typename F2, typename G2, typename ...RestTs>
struct AlterSet<Alter<F1, G1>, Alter<F2, G2>, RestTs...> {
    Alter<F1, G1> parser;
    AlterSet<Alter<F2, G2>, RestTs...> rest;

    template <typename ArgF1, typename ArgF2, typename ...RestFs>
    constexpr explicit AlterSet(ArgF1&& parser1, ArgF2&& parser2, RestFs&&... parsers)
        : parser(std::forward<ArgF1>(parser1))
        , rest(std::forward<ArgF2>(parser2), std::forward<RestFs>(parsers)...)
    {}

    [[nodiscard]] constexpr Result operator()(Stream stream) const {
        Result result = parser.condition_parser(stream);
        if (!result) {
            return rest(stream);
        }
        return parser.successor_parser(result.stream());
    }
};
}  // namespace parsi::fn

#endif  // PARSI_FN_ALTERS_HPP
