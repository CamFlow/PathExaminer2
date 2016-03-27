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
/**
 * @brief Replaces yices_pp_term() by a no-op when compiling for release mode
 */
#define yices_pp_term(...) (void)0
#endif

/**
 * @brief Output stream for debug information
 *
 * The design of this class is arguably strange because it both inherits from
 * and contains a reference to a std::ostream object (std::cerr by default).
 * This class is normally used as a singleton. The function debug() returns a
 * reference to a DebugMe static global instance.
 * The inheritance enables uses like <pre>debug() << std::endl;</pre> to
 * compile but actually everything that is output to the debug sink is forwarded
 * to the attribute ostream. When the code is compiled in release mode (NDEBUG
 * is defined), all the operators <pre>operator&lt;&lt;()</pre> become no-ops.
 */
class DebugMe : std::basic_ostream<char> {
	private:
		/**
		 * @brief The underlying ostream, to which the information are
		 * forwarded
		 */
		std::basic_ostream<char>& _output;

	public:
		/**
		 * @brief Builds a debug sink
		 *
		 * Usually, calling this method is not necessary. It is better
		 * to use the static global instance.
		 * @param output the "real" ostream that will receive al the
		 * flows sent to the DebugMe object
		 */
		DebugMe(std::basic_ostream<char>& output = std::cerr) :
			_output(output)
		{}
		/**
		 * @brief A static global instance of DebugMe
		 */
		static DebugMe INSTANCE;

#ifndef NDEBUG
		/**
		 * @brief Prints anything to the debug sink
		 * @tparam T the type of the object to print
		 * @param s the object to print
		 * @return a reference to this DebugMe itself
		 */
		template<typename T>
		inline DebugMe& operator<<(const T& s) {
			_output << s;
			return *this;
		}
		/**
		 * @brief Applies a I/O manipulator to the debug sink
		 * @param func the operation to apply on the DebugMe instance
		 * @return a reference to this DebugMe itself
		 */
		inline DebugMe& operator<<(std::ios_base& (*func)(std::ios_base&)) {
			_output << func;
			return *this;
		}
		/**
		 * @brief Applies a I/O manipulator to the debug sink
		 * @param func the operation to apply on the DebugMe instance
		 * @return a reference to this DebugMe itself
		 */
		inline DebugMe& operator<<(std::basic_ios<char>& (*func)(std::basic_ios<char>&)) {
			_output << func;
			return *this;
		}
		/**
		 * @brief Applies a I/O manipulator to the debug sink
		 * @param func the operation to apply on the DebugMe instance
		 * @return a reference to this DebugMe itself
		 */
		inline DebugMe& operator<<(std::basic_ostream<char>& (*func)(std::basic_ostream<char>&)) {
			_output << func;
			return *this;
		}
#else
		template<typename T>
		inline DebugMe& operator<<(const T&) {
			return *this;
		}
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
	/**
	 * @brief Gets a reference on the debug sink
	 * @return Gets a reference on the static global instance of DebugMe
	 */
	DebugMe& debug() {
		return DebugMe::INSTANCE;
	}
}
#endif
