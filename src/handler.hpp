/*
	Mediastorage-proxy is a HTTP proxy for mediastorage based on elliptics
	Copyright (C) 2013-2015 Yandex

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef MDS_PROXY__SRC__HANDLER__HPP
#define MDS_PROXY__SRC__HANDLER__HPP

#include "error.hpp"
#include "loggers.hpp"

#include <functional>
#include <memory>
#include <type_traits>

namespace elliptics {

namespace detail {

template <typename BaseStream, typename Callback>
class callback_helper_t;

template <typename Stream, typename Callback>
struct handler_traits {
	typedef Stream stream_type;
	typedef typename std::remove_reference<Callback>::type callback_type;
	typedef callback_helper_t<stream_type, callback_type> callback_helper_type;
};

template <typename Stream, typename Callback>
class callback_helper_t {
public:
	typedef Callback callback_t;

	callback_helper_t(std::shared_ptr<Stream> stream_, callback_t callback_)
		: stream(std::move(stream_))
		, callback(std::move(callback_))
	{}

	template <typename... Args>
	void
	operator() (Args &&...args) {
		try {
			callback(std::forward<Args>(args)...);
		} catch (const http_error &ex) {
			std::ostringstream oss;
			oss
				<< "http_error: http_status = " << ex.http_status()
				<< " ; description = " << ex.what();
			auto msg = oss.str();

			if (ex.is_server_error()) {
				MDS_LOG_ERROR("%s", msg.c_str());
			} else {
				MDS_LOG_INFO("%s", msg.c_str());
			}

			stream->send_reply(ex.http_status());
		} catch (const std::exception &ex) {
			MDS_LOG_ERROR("uncaughted exception: http_status = 500 ; description = %s"
					, ex.what());
			stream->send_reply(500);
		}
	}

private:
	auto
	logger() -> decltype(std::declval<Stream>().logger()) {
		return stream->logger();
	}

	std::shared_ptr<Stream> stream;
	callback_t callback;

};

template <typename Stream, typename Callback>
typename handler_traits<Stream, Callback>::callback_helper_type
make_callback_helper_t(std::shared_ptr<Stream> stream, Callback callback) {
	return typename handler_traits<Stream, Callback>::callback_helper_type(
			std::move(stream), std::move(callback));
}

} // namespace detail

template <typename BaseStream>
class handler_t
	: public BaseStream
	, public std::enable_shared_from_this<handler_t<BaseStream>>
{
public:
	typedef BaseStream stream_base_type;
	typedef handler_t<stream_base_type> self_type;
	typedef std::enable_shared_from_this<self_type> enable_shared_from_this_base_type;

	typedef std::function<void (void)> unsafe_callback_t;

	using stream_base_type::send_reply;
	using stream_base_type::logger;

	void
	safe_call(unsafe_callback_t unsafe_callback) {
		try {
			unsafe_callback();
		} catch (const http_error &ex) {
			std::ostringstream oss;
			oss
				<< "http_error: http_status = " << ex.http_status()
				<< " ; description = " << ex.what();
			auto msg = oss.str();

			if (ex.is_server_error()) {
				MDS_LOG_ERROR("%s", msg.c_str());
			} else {
				MDS_LOG_INFO("%s", msg.c_str());
			}

			send_reply(ex.http_status());
		} catch (const std::exception &ex) {
			MDS_LOG_ERROR("uncaughted exception: http_status = 500 ; description = %s"
					, ex.what());
			send_reply(500);
		}
	}


	template <typename Callback>
	ioremap::thevoid::detail::attributes_bind_handler<
		typename detail::handler_traits<self_type, Callback>::callback_helper_type
	>
	safe_wrapper(Callback callback) {
		return stream_base_type::wrap(safe_wrapper_impl(std::move(callback)));
	}

private:
	template <typename Callback>
	typename detail::handler_traits<self_type, Callback>::callback_helper_type
	safe_wrapper_impl(Callback callback) {
		auto self = enable_shared_from_this_base_type::shared_from_this();
		return detail::make_callback_helper_t(std::move(self), std::move(callback));
	}

};

} // namespace elliptics

#endif /* MDS_PROXY__SRC__HANDLER__HPP */

