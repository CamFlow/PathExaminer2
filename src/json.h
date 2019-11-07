#ifndef JSON_H
#define JSON_H

#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

template<typename T>
struct is_container : std::false_type {};

template<typename T>
struct is_associative_container : std::false_type {};

//vector overload
template<typename T>
struct is_container< std::vector<T> > : std::true_type {};

//map overload
template<typename K, typename V>
struct is_container< std::map<K, V> > : std::true_type {};

template<typename V>
struct is_associative_container< std::map<std::string, V> > : std::true_type {};



class JsonStream {
	private:
		std::ofstream _stream;

		template<typename T>
		std::string dump(const T& t) const {
			return dump_value_or_container(t, typename is_container<T>::type());
		}

		template<typename T>
		std::string dump_value_or_container(const T& t, std::false_type) const {
			return dump_value(t);
		}

		template<typename T>
    		std::string dump_value_or_container(const T& t, std::true_type) const {
        		return dump_simple_or_associative_container(t, typename is_associative_container<T>::type());
    		}

    		template<typename T>
    		std::string dump_simple_or_associative_container(const T& t, std::false_type) const {
        		return dump_simple_container(t);
    		}

    		template<typename T>
    		std::string dump_simple_or_associative_container(const T& t, std::true_type) const {
        		return dump_associative_container(t);
    		}

    		// implement type specific serialization
    		template<typename V>
    		std::string dump_value(const V& value) const {
        		std::ostringstream oss;
        		oss << sanitize(value);
        		return oss.str();
    		}

    		std::string dump_value(const std::string& value) const {
        		return "\"" + value + "\"";
    		}

    		template<typename K, typename V>
    		std::string dump_value(const std::pair<const K, V>& pair) const {
        		std::ostringstream oss;
        		oss << "[" << dump(pair.first) << ", " << dump(pair.second) << "]";
        		return oss.str();
    		}

    		template<typename V>
    		std::string dump_pair(const std::pair<const std::string, V>& pair) const {
        		std::ostringstream oss;
        		oss << dump(pair.first) << ": " << dump(pair.second);
        		return oss.str();
    		}

    		template<typename C>
    		std::string dump_simple_container(const C& container) const {
        		std::ostringstream oss;
        		typename C::const_iterator it = container.begin();

        		oss << "[" << dump(*it);
        		for (++ it ; it != container.end() ; ++ it) {
            			oss << ", " << dump(*it);
        		}
        		oss << "]";

        		return oss.str();
    		}

    		template<typename M>
    		std::string dump_associative_container(const M& map) const {
        		std::ostringstream oss;
        		typename M::const_iterator it = map.begin();

        		oss << "{" << dump_pair(*it);
        		for (++ it ; it != map.end() ; ++ it) {
            			oss << ", " << dump_pair(*it);
        		}
        		oss << "}";

        		return oss.str();
    		}

    		template<typename T>
    		T sanitize(const T& t) const {
        		return t;
    		}
	public:
		JsonStream(const std::string& path = "json.log") : _stream(path.c_str(), std::ofstream::out | std::ofstream::app)
		{}
	
		template<typename T>
		JsonStream& operator<<(const T& data) {
			if (_stream.is_open()) {
				_stream << dump(data);
			}
			return *this;
		}
	
		static JsonStream INSTANCE;
};

namespace {
        /**
         * @brief Gets a reference on the debug sink
         * @return Gets a reference on the static global instance of DebugMe
         */
        JsonStream& json() {
                return JsonStream::INSTANCE;
        }
}

#endif
