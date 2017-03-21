//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_MESSAGE_PARSER_HPP
#define BEAST_HTTP_MESSAGE_PARSER_HPP

#include <beast/http/message.hpp>
#include <beast/http/header_parser.hpp>
#include <beast/core/detail/clamp.hpp>
#include <boost/optional.hpp>
#include <array>
#include <type_traits>
#include <utility>

namespace beast {
namespace http {

/** A parser for producing HTTP/1 messages.

    This class uses the basic HTTP/1 wire format parser to convert
    a series of octets into a @ref message.

    @tparam isRequest Indicates whether a request or response
    will be parsed.

    @tparam Body The type used to represent the body.

    @tparam Fields The type of container used to represent the fields.

    @note A new instance of the parser is required for each message.
*/
template<bool isRequest, class Body, class Fields>
class message_parser
    : public basic_parser<isRequest,
        message_parser<isRequest, Body, Fields>>
{
    using base_type = basic_parser<isRequest,
        message_parser<isRequest, Body, Fields>>;

    using reader_type = typename Body::reader;

    message<isRequest, Body, Fields> m_;
    boost::optional<typename Body::reader> r_;

public:
    /// The type of message returned by the parser
    using value_type = message<isRequest, Body, Fields>;

    /// The type of buffer sequence representing the body
    using mutable_buffers_type =
        typename reader_type::mutable_buffers_type;

    /// Constructor (default)
    message_parser() = default;

    /// Copy constructor (disallowed)
    message_parser(message_parser const&) = delete;

    /// Copy assignment (disallowed)
    message_parser& operator=(message_parser const&) = delete;

    /** Move constructor.

        After the move, the only valid operation
        on the moved-from object is destruction.
    */
    message_parser(message_parser&& other);

    /** Constructor

        @param args Optional arguments forwarded to the 
        @ref http::header constructor.

        @note This function participates in overload
        resolution only if the first argument is not a
        @ref http::header_parser or @ref message_parser.
    */
#if GENERATING_DOCS
    template<class... Args>
    explicit
    msesage_parser(Args&&... args);
#else
    template<class Arg1, class... ArgN,
        class = typename std::enable_if<
            ! std::is_same<typename
                std::decay<Arg1>::type,
                    header_parser<isRequest, Fields>>::value &&
            ! std::is_same<typename
                std::decay<Arg1>::type, message_parser>::value
                    >::type>
    explicit
    message_parser(Arg1&& arg1, ArgN&&... argn);
#endif

    /** Construct a message parser from a @ref header_parser.

        @param parser The header parser to construct from.

        @param args Optional arguments forwarded to the message
        constructor.
    */
    template<class... Args>
    explicit
    message_parser(header_parser<
        isRequest, Fields>&& parser, Args&&... args);

    /** Returns the parsed message.

        Depending on the progress of the parser, portions
        of this object may be incomplete.
    */
    value_type const&
    get() const
    {
        return m_;
    }

    /** Returns the parsed message.

        Depending on the progress of the parser, portions
        of this object may be incomplete.
    */
    value_type&
    get()
    {
        return m_;
    }

    /** Returns ownership of the parsed message.

        Ownership is transferred to the caller.
        Depending on the progress of the parser, portions
        of this object may be incomplete.

        @par Requires

        @ref value_type is @b MoveConstructible
    */
    value_type
    release()
    {
        static_assert(std::is_move_constructible<decltype(m_)>::value,
            "MoveConstructible requirements not met");
        return std::move(m_);
    }

    /** Copy body bytes already present in the buffer.
    */
    template<class DynamicBuffer>
    void
    copy(DynamicBuffer& dynabuf)
    {
        this->maybe_begin_body();
        using boost::asio::buffer_copy;
        auto const n = (std::min)(
            dynabuf.size(), this->remain());
        if(n == 0)
            return;
        buffer_copy(
            r_->prepare(n), dynabuf.data());
        this->consume(n);
        dynabuf.consume(n);
        r_->commit(n);
    }

    template<class DynamicBuffer>
    mutable_buffers_type
    prepare(DynamicBuffer& dynabuf, std::size_t limit)
    {
        BOOST_ASSERT(limit > 0);
        BOOST_ASSERT(this->remain() > 0);
        this->maybe_begin_body();
        auto const n = (std::min)(
            static_cast<std::size_t>(
                this->remain()), limit);
        return r_->prepare(n);
    }

    void
    commit(std::size_t n)
    {
        BOOST_ASSERT(n <= this->remain());
        r_->commit(n);
        this->consume(n);
    }

private:
    friend class basic_parser<isRequest, message_parser>;

    void
    on_begin_request(
        boost::string_ref const& method,
            boost::string_ref const& path,
                int version, error_code&)
    {
        m_.url = std::string{
            path.data(), path.size()};
        m_.method = std::string{
            method.data(), method.size()};
        m_.version = version;
    }

    void
    on_begin_response(int status,
        boost::string_ref const& reason,
            int version, error_code&)
    {
        m_.status = status;
        m_.reason = std::string{
            reason.data(), reason.size()};
        m_.version = version;
    }

    void
    on_field(boost::string_ref const& name,
        boost::string_ref const& value,
            error_code&)
    {
        m_.fields.insert(name, value);
    }

    void
    on_end_header(error_code& ec)
    {
    }

    void
    on_begin_body()
    {
        r_.emplace(m_);
        r_->init(this->content_length());
    }

    mutable_buffers_type
    on_prepare_body(std::size_t limit)
    {
        return r_->prepare(
            beast::detail::clamp(remain(), limit));
    }

    void
    on_chunk(std::uint64_t,
        boost::string_ref const&,
            error_code&)
    {
    }

    void
    on_body(boost::string_ref const& data,
        error_code&)
    {
        using boost::asio::buffer;
        using boost::asio::buffer_copy;
        r_->commit(buffer_copy(
            r_->prepare(data.size()),
                buffer(data.data(), data.size())));
    }

    void
    on_end_body(error_code&)
    {
        r_->finish();
    }

    void
    on_end_message(error_code&)
    {
    }
};

} // http
} // beast

#include <beast/http/impl/message_parser.ipp>

#endif