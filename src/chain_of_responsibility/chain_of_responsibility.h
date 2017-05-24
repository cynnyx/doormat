#ifndef DOOR_MAT_CHAIN_OF_RESPONSIBILITY_H
#define DOOR_MAT_CHAIN_OF_RESPONSIBILITY_H

#include <utility>
#include <cstddef>
#include <utility>
#include <iostream>
#include <tuple>
#include <memory>
#include <functional>
#include "index_sequence.h"
#include "node_interface.h"

template<typename T, typename S, typename... N>
struct chain;

template<typename I, std::size_t N, std::size_t M, typename... T>
struct node
{
	using prev_type = node<I, N-1, M, T...>;
	using next_type = node<I, N+1, M, T...>;

	node(prev_type* p, next_type* n, logging::access_recorder *aclogger = nullptr)
		: prev{p}
		, next{n}
		, t(
			[this](http::http_request&& message){ next->request_preamble(std::move(message)); },
			[this](auto&& data, auto len) { next->request_body(std::move(data), len); },
			[this](auto&& k, auto&& v) { next->request_trailer(std::move(k),std::move(v)); },
			[this](const errors::error_code&ec){next->request_canceled(ec);},
			[this](){next->request_finished(); },
			[this](http::http_response &&header) { prev->header(std::move(header)); },
			[this](auto&& data, auto len) {prev->body(std::move(data), len); },
			[this](auto&& k, auto&& v) {prev->trailer(std::move(k),std::move(v)); },
			[this](){prev->end_of_message();},
			[this](const errors::error_code &ec){prev->error(ec);},
			[this](){prev->response_continue();}, aclogger)
	{}

	template<typename... Args>
	void request_preamble(Args&&... args)
	{
		t.on_request_preamble(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_body(Args &&... args)
	{
		t.on_request_body(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_trailer(Args &&... args)
	{
		t.on_request_trailer(std::forward<Args>(args)...);
	}

	template<typename...Args>
	void request_canceled(Args&&... args)
	{
		t.on_request_canceled(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_finished(Args &&... args)
	{
		t.on_request_finished(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void header(Args&&... args)
	{
		t.on_header(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void body(Args&&... args)
	{
		t.on_body(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void trailer(Args&&... args)
	{
		t.on_trailer(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void end_of_message(Args&&... args)
	{
		t.on_end_of_message(std::forward<Args>(args)...);
	}


	template<typename... Args>
	void error(Args&&... args)
	{
		t.on_error(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void response_continue(Args&&... args)
	{
		t.on_response_continue(std::forward<Args>(args)...);
	}

private:
	prev_type* prev{nullptr};
	next_type* next{nullptr};
	typename std::tuple_element<N, std::tuple<T...>>::type t;
};

template<typename I, std::size_t M, typename... T>
struct node<I, 0, M, T...>
{
	using prev_type = chain<I,decltype(impl::make_index_sequence<sizeof...(T)>()), T...>;
	using next_type = node<I, 1, M, T...>;

	node(prev_type* p, next_type* n, logging::access_recorder *aclogger = nullptr)
		: prev{p}
		, next{n}
		, t(
			[this](http::http_request&& message){ next->request_preamble(std::move(message)); },
			[this](auto&& data, auto len){ next->request_body(std::move(data), len); },
			[this](auto&& k, auto&& v){ next->request_trailer(std::move(k), std::move(v)); },
			[this](const errors::error_code&ec){next->request_canceled(ec);},
			[this](){next->request_finished(); },
			[this](http::http_response &&header) { prev->header(std::move(header)); },
			[this](auto&& data, auto len) {prev->body(std::move(data), len); },
			[this](auto&& k, auto&& v) {prev->trailer(std::move(k), std::move(v));},
			[this](){prev->end_of_message(); },
			[this](const errors::error_code &ec){prev->error(ec); },
			[this](){prev->response_continue();}, aclogger)
	{}

	template<typename... Args>
	void request_preamble(Args&&... args)
	{
		t.on_request_preamble(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_body(Args &&... args)
	{
		t.on_request_body(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_trailer(Args &&... args)
	{
		t.on_request_trailer(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_canceled(Args&&... args)
	{
		t.on_request_canceled(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_finished(Args &&... args)
	{
		t.on_request_finished(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void header(Args&&... args)
	{
		t.on_header(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void body(Args&&... args)
	{
		t.on_body(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void trailer(Args&&... args)
	{
		t.on_trailer(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void end_of_message(Args&&... args)
	{
		t.on_end_of_message(std::forward<Args>(args)...);
	}


	template<typename... Args>
	void error(Args&&... args)
	{
		t.on_error(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void response_continue(Args&&... args)
	{
		t.on_response_continue(std::forward<Args>(args)...);
	}

private:
	prev_type* prev{nullptr};
	next_type* next{nullptr};
	typename std::tuple_element<0, std::tuple<T...>>::type t;
};

template<typename I, typename T>
struct node<I, 0, 0, T>
{
	using prev_type = chain<I,impl::integer_sequence<std::size_t, 0>, T>;

	node(prev_type* p, void *, logging::access_recorder *aclogger = nullptr)
		: prev{p}
		, t(
			[this](http::http_request&&){ /*do nothing: we need to receive body. */},
			[this](auto&&, auto) { /* do nothing, we don't know if it is the last one! */},
			[this](auto&&, auto&&){},
			[this](const errors::error_code&ec){this->error(ec);},
			[this](){ },
			[this](http::http_response &&header) { prev->header(std::move(header)); },
			[this](auto&& data, auto len) { prev->body(std::move(data), len); },
			[this](auto&& k, auto&& v) { prev->trailer(std::move(k), std::move(v)); },
			[this](){ prev->end_of_message(); },
			[this](const errors::error_code &ec){ prev->error(ec); },
			[this](){prev->response_continue();}, aclogger)
	{}

	template<typename... Args>
	void request_preamble(Args&&... args)
	{
		t.on_request_preamble(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_body(Args &&... args)
	{
		t.on_request_body(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_trailer(Args &&... args)
	{
		t.on_request_trailer(std::forward<Args>(args)...);
	}

	template<typename...Args>
	void request_canceled(Args &&... args)
	{
		t.on_request_canceled(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_finished(Args &&... args)
	{
		t.on_request_finished(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void header(Args&&... args)
	{
		t.on_header(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void body(Args&&... args)
	{
		t.on_body(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void trailer(Args&&... args)
	{
		t.on_trailer(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void end_of_message(Args&&... args)
	{
		t.on_end_of_message(std::forward<Args>(args)...);
	}


	template<typename... Args>
	void error(Args &&... args)
	{
		t.on_error(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void response_continue(Args&&... args)
	{
		t.on_response_continue(std::forward<Args>(args)...);
	}

private:
	prev_type* prev{nullptr};
	T t;
};

template<typename I, std::size_t N, typename... T>
struct node<I,N, N, T...>
{
	using prev_type = node<I,N-1, N, T...>;

	node(prev_type* p, void*, logging::access_recorder *aclogger = nullptr)
		: prev{p}
		, t(
			//request forward callback
			[this](http::http_request&&){},
			[this](auto&&, auto) {},
			[this](auto&&,auto&&) {},
			[this](const errors::error_code&ec){this->error(ec); },
			[this](){ },
			[this](http::http_response &&header) { prev->header(std::move(header));},
			[this](auto&& data, auto len){ prev->body(std::move(data), len); },
			[this](auto&& k, auto&& v){ prev->trailer(std::move(k), std::move(v)); },
			[this](){prev->end_of_message();},
			[this](const errors::error_code &ec){prev->error(ec);},
			[this](){prev->response_continue();}, aclogger)
	{}

	template<typename... Args>
	void request_preamble(Args&&... args)
	{
		t.on_request_preamble(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_body(Args &&... args)
	{
		t.on_request_body(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_trailer(Args &&... args)
	{
		t.on_request_trailer(std::forward<Args>(args)...);
	}

	template<typename...Args>
	void request_canceled(Args &&...args)
	{
		t.on_request_canceled(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_finished(Args &&... args)
	{
		t.on_request_finished(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void header(Args&&... args)
	{
		t.on_header(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void body(Args&&... args)
	{
		t.on_body(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void trailer(Args&&... args)
	{
		t.on_trailer(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void end_of_message(Args&&... args)
	{
		t.on_end_of_message(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void error(Args &&... args)
	{
		t.on_error(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void response_continue(Args&&... args)
	{
		t.on_response_continue(std::forward<Args>(args)...);
	}

private:
	prev_type* prev{nullptr};
	typename std::tuple_element<N, std::tuple<T...>>::type t;
};

template<typename T, std::size_t... I, typename... N>
struct chain<T,impl::integer_sequence<std::size_t, I...>, N...>: public T
	, public node<T,I, sizeof...(N)-1, N...>...
{
	chain(logging::access_recorder *aclogger = nullptr)
		: T(
			[this](http::http_request&& message){ this->request_preamble(std::move(message));},
			[this](auto&& data, auto len) { this->request_body(std::move(data), len); },
			[this](auto&& k, auto&& v) { this->request_trailer(std::move(k), std::move(v)); },
			[this](const errors::error_code&ec) { this->request_canceled(ec); },
			[this](){ this->request_finished();},
			[](http::http_response &&){ },
			[](auto&&, auto){ },
			[](auto&& , auto&&){ },
			[](){ },
			[](const errors::error_code &){ },
			[](){}, nullptr
			)
		, node<T, I, sizeof...(N)-1, N...>{this, this, aclogger}...
	{}

	template<typename... Args>
	void request_preamble(Args&&... args)
	{
		node<T,0, sizeof...(N)-1, N...>::request_preamble(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_body(Args &&... args)
	{
		node<T, 0, sizeof...(N)-1, N...>::request_body(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_trailer(Args &&... args)
	{
		node<T, 0, sizeof...(N)-1, N...>::request_trailer(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_canceled(Args &&... args)
	{
		node<T, 0, sizeof...(N)-1, N...>::request_canceled(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void request_finished(Args &&... args)
	{
		node<T, 0, sizeof...(N)-1, N...>::request_finished(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void header(Args&&... args)
	{
		T::base::on_header(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void body(Args&&... args)
	{
		T::base::on_body(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void trailer(Args&&... args)
	{
		T::base::on_trailer(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void end_of_message(Args&&... args)
	{
		T::base::on_end_of_message(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void error(Args &&... args)
	{
		T::base::on_error(std::forward<Args>(args)...);
	}

	template<typename... Args>
	void response_continue(Args&&... args)
	{
		T::base::on_response_continue(std::forward<Args>(args)...);
	}

};

template<typename T,typename ...Args>
static std::unique_ptr<T> make_unique_chain(logging::access_recorder *aclogger = nullptr)
{
	return std::unique_ptr<T>{new chain<T,decltype(impl::make_index_sequence<sizeof...(Args)>()), Args...>(aclogger)};
}

#endif //DOOR_MAT_CHAIN_OF_RESPONSIBILITY_H

