/**
 * @file debug.h
 * @brief Definition of some macros/functions to help printing debug information
 * @author Laurent Georget
 * @version 0.1
 * @date 2016-03-24
 */
#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>
#include <string>

#ifdef NDEBUG
//hijack the pretty-printing
#define yices_pp_term(...) (void)0
#endif

class DebugMe : std::basic_ostream<char> {
	private:
		std::basic_ostream<char>& _output;

	public:
		DebugMe(std::basic_ostream<char>& output = std::cerr) :
			_output(output)
		{}
		static DebugMe INSTANCE;

#ifndef NDEBUG
		template<typename T>
		inline DebugMe& operator<<(const T& s) {
			_output << s;
			return *this;
		}
		template<typename T>
		inline DebugMe& operator<<(std::ios_base& (*func)(std::ios_base&)) {
			_output << func;
			return *this;
		}
		inline DebugMe& operator<<(std::basic_ios<char>& (*func)(std::basic_ios<char>&)) {
			_output << func;
			return *this;
		}
		inline DebugMe& operator<<(std::basic_ostream<char>& (*func)(std::basic_ostream<char>&)) {
			_output << func;
			return *this;
		}
#else
		template<typename T>
		inline DebugMe& operator<<(const T&) {
			return *this;
		}
		template<typename T>
		inline DebugMe& operator<<(std::ios_base& (*)(std::ios_base&)) {
			return *this;
		}
		inline DebugMe& operator<<(std::basic_ios<char>& (*)(std::basic_ios<char>&)) {
			return *this;
		}
		inline DebugMe& operator<<(std::basic_ostream<char>& (*)(std::basic_ostream<char>&)) {
			return *this;
		}
#endif

};

namespace {
	DebugMe& debug() {
		return DebugMe::INSTANCE;
	}
}
#endif
