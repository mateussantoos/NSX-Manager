// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <utility>
#include <variant>

/// @brief Pure, platform-independent building blocks.
/// @details Nothing in this namespace may include libnx, curl or Borealis - it
///          compiles with a host compiler so it can be unit tested without a
///          console. See ADR-0014.
namespace nsx::core {

/// @brief Either a value or an error, never both, never neither.
///
/// @details The project returns errors rather than throwing across layer
///          boundaries. `std::expected` would do this job, but it is C++23 and
///          this project targets C++20 (ADR-0002), so `Result` fills the role.
///
///          Two behaviours are deliberate:
///          - Construction is through the named factories @ref ok and @ref err,
///            so `Result<std::string, std::string>` is never ambiguous.
///          - @ref value and @ref error have preconditions rather than throwing.
///            Check with @ref hasValue (or `operator bool`) first. Throwing here
///            would reintroduce exactly the control flow this type exists to
///            avoid.
///
/// @tparam T The success type.
/// @tparam E The error type.
/// @since 0.1.0
template <typename T, typename E>
class Result
{
public:
    /// @brief Construct a successful result.
    /// @param value The success value.
    /// @return A Result holding @p value.
    [[nodiscard]] static Result ok(T value)
    {
        return Result(std::in_place_index<0>, std::move(value));
    }

    /// @brief Construct a failed result.
    /// @param error The error value.
    /// @return A Result holding @p error.
    [[nodiscard]] static Result err(E error)
    {
        return Result(std::in_place_index<1>, std::move(error));
    }

    /// @brief Whether this holds a value rather than an error.
    /// @return True when a value is present.
    [[nodiscard]] bool hasValue() const noexcept { return m_state.index() == 0; }

    /// @brief Whether this holds a value rather than an error.
    /// @return True when a value is present.
    [[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }

    /// @brief Access the success value.
    /// @pre @ref hasValue must be true.
    /// @return A reference to the contained value.
    [[nodiscard]] const T& value() const& { return std::get<0>(m_state); }

    /// @brief Access the success value.
    /// @pre @ref hasValue must be true.
    /// @return An rvalue reference to the contained value.
    [[nodiscard]] T&& value() && { return std::get<0>(std::move(m_state)); }

    /// @brief Access the error.
    /// @pre @ref hasValue must be false.
    /// @return A reference to the contained error.
    [[nodiscard]] const E& error() const& { return std::get<1>(m_state); }

    /// @brief Access the value, or a fallback when this holds an error.
    /// @param fallback Returned when no value is present.
    /// @return The contained value, or @p fallback.
    [[nodiscard]] T valueOr(T fallback) const&
    {
        return hasValue() ? std::get<0>(m_state) : std::move(fallback);
    }

private:
    template <std::size_t I, typename U>
    explicit Result(std::in_place_index_t<I> tag, U&& v) : m_state(tag, std::forward<U>(v))
    {
    }

    std::variant<T, E> m_state;
};

}  // namespace nsx::core
