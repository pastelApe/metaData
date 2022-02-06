#pragma clang diagnostic push
#pragma ide diagnostic ignored "HidingNonVirtualFunction"
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma ide diagnostic ignored "readability-convert-member-functions-to-static"
//STL
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <vector>

//Boost
#include "boost/algorithm/string.hpp"
#include "boost/variant.hpp"

//C++ Requests: Curl for People
#include "cpr/cpr.h"

//RapidJson
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"



/*--------------------------------------------------------------------------------------------------------------------*/

namespace  fs = std::filesystem;
namespace  rj = rapidjson;

/*--------------------------------------------------------------------------------------------------------------------*/

typedef boost::make_recursive_variant<
        bool,
        int64_t,
        uint64_t,
        double,
        std::string,
        std::vector<boost::recursive_variant_>,
        std::map<std::string, boost::recursive_variant_>>::type Variant_Type;

/*--------------------------------------------------------------------------------------------------------------------*/

struct Type_Handler : public rj::BaseReaderHandler<rj::UTF8<>, Type_Handler> {
private:
    Variant_Type _props;
public:
    // && is normally only used to declare a parameter of a function. And it only takes an r-value expression.
    Variant_Type&& Get_Value() {
        return std::move(_props);
    }

    bool Null() { return true; }
    bool Bool(bool b) { _props = b; return true; }
    bool Int(int i) { _props = static_cast<int64_t> (i); return true; }
    bool Uint(unsigned u) { _props = static_cast<uint64_t> (u);  return true; }
    bool Int64(int64_t i64) { _props = static_cast<int64_t> (i64); return true; }
    bool Uint64(uint64_t u64) { _props = static_cast<uint64_t> (u64) ; return true; }
    bool Double(double d) { _props = d; return true; }
    bool String(const char* str, rj::SizeType length, bool copy) {
        _props = std::string(str, length);
        return true;
    }
    bool StartObject() { return true; }
    bool Key(const char* str, rj::SizeType length, bool copy) {
        _props = std::string(str, length);
        return true;
    }
    bool EndObject(rj::SizeType memberCount) { return true; }
    bool StartArray() { return true; }
    bool EndArray(rj::SizeType elementCount) { return true; }
};

/*--------------------------------------------------------------------------------------------------------------------*/

//Value_Handler is a recursive function. Checks all values including child objects and arrays.
Variant_Type Value_Handler(rj::Value& value) {
    if (value.IsObject()) {
        std::map<std::string, Variant_Type> map;
        for (auto& [obj_key, obj_val] : value.GetObject()) {
            map.emplace(obj_key.GetString(), Value_Handler(obj_val));

        }
        return map;
    }
    else if (value.IsArray()) {
        // To remove comma from printing after last value in array, subtract 1.
        std::vector<Variant_Type> vec;
        for (auto& arr_value: value.GetArray()) {
            vec.push_back(Value_Handler(arr_value));
        }

        return vec;
    }
    else {
        Type_Handler val_type;
        value.Accept(val_type);
        return val_type.Get_Value();
    }
}
/*--------------------------------------------------------------------------------------------------------------------*/

std::map<std::string ,Variant_Type> Process_Document (rj::Document& document) {
    std::map<std::string ,Variant_Type> meta_Data;

    if (document.HasParseError()) {
        fprintf(stderr, "Error (offset %u): %s",
                (unsigned)document.GetErrorOffset(),
                rj::GetParseError_En(document.GetParseError()));
        std::cout << std::endl;
    }

    if (document.IsArray()) {
        for (auto& prop : document.GetArray()) {
            if (prop.IsObject()) {
                for (auto& [key, value]: prop.GetObject()) {
                    meta_Data.emplace(key.GetString(), Value_Handler(value));
                }
            }
        }
    } else if (document.IsObject()) {
        for (auto& [key, value] : document.GetObject()) {
            meta_Data.emplace(key.GetString(), Value_Handler(value));
        }
    }
    return meta_Data;
}

/*--------------------------------------------------------------------------------------------------------------------*/

std::map<std::string, Variant_Type> Json_Handler() {
    std::map<std::string, Variant_Type> map {};

        rj::Document document;
        const char* json = "[\n{\"author\": \"Xavier\","
                           "\"Author\": \"Casey\","
                           "\"creator\": \"Xavier\","
                           "\"Created-By\": [\"Xavier\", \"Casey\"],"
                           "\"hello\": \"world\","
                           "\"true\": true,"
                           "\"false\": false,"
                           "\"int64\": 123,"
                           "\"uint64\": 123456,"
                           "\"pi\": 3.1416,"
                           "\"array\": [1, 2, 3, 4],"
                           "\"nested::array\": [\"This\", \"is\", \"level\", 1, \".\","
                           "[\"This\", \"is\", \"level\", 2, \".\"],"
                           "[\"This\", \"is\", \"level\", 3, \".\"]],"
                           "\"nested::object\":{\"level2\": {\"level3\": \"sub-value\"}},"
                           "\"array::object\":[{\"level2\": {\"level3\": [1,2,3,4]}}, [1,2,3,4]]}\n]";
        document.Parse(json);
        map = Process_Document(document);

    return map;
}

/*--------------------------------------------------------------------------------------------------------------------*/
struct Recursive_Print_T {
    using result_type = void;
    std::ostream& _os;

    // forwards for `operator()`
    template <typename T> void call(T const& v) const { return operator()(v); }

    // dispatch for variants
    template <typename... Ts> void operator()(boost::variant<Ts...> const& v) const {
        return boost::apply_visitor(*this, v);
    }
    void operator()(nullptr_t) const {_os << "Null"; }
    void operator()(bool b) const { _os << std::boolalpha << b; }
    void operator()(int64_t i64) const { _os << i64; }
    void operator()(u_int64_t u64) const { _os << u64; }
    void operator()(double d) const { _os << d; }
    void operator()(int i) const { _os << i; }
    void operator()(std::string const &s) const { _os << std::quoted(s); }

    template <typename... Ts> void operator()(std::vector<Ts...> const& v) const {
        _os << "[ ";
        bool first = true;
        for (auto& el : v) {
            if (first) {
                first = false;
            }
            else{
                _os << ", ";
            }
            call(el);
        }
        _os << " ]";
    }

    template <typename T> void operator()(std::map<std::string, T> const& map) const {
        _os << "{ ";
        bool first = true;
        for (auto& [key, value]: map) {
            if (first) {
                first = false;
            } else {
                _os << ", ";
            }
            call(key);
            _os << ": ";
            call(value);
        }
        _os << " }";
    }
};

/*--------------------------------------------------------------------------------------------------------------------*/

int main() {
        //Creates vector of metadata key: value maps.
        std::vector<std::map<std::string, Variant_Type>> meta_Data {};
        meta_Data.emplace_back(Json_Handler());
        Recursive_Print_T print {std::cout};

        for (auto& map : meta_Data) {
            for(auto& [key, value]: map) {
                std::cout << key << ": ";
                print(value);
                std::cout << std::endl;
            }
        }

    /*
      Contains the core set of basic Tika metadata properties, which all parsers will attempt to supply.
      This set also includes the Dublin Core properties as well as properties found from custom fields that
      match a core field.

      Removed "FORMAT", "TYPE".

      Included "AUTHOR",
     */
    std::vector<std::string> core_keys {
        "AUTHOR", "ALTITUDE", "COMMENTS", "CONTRIBUTOR", "COVERAGE", "CREATED", "CREATOR", "CREATOR_TOOL", "DATE",
        "DC:", "DC.", "DC_", "DCTERMS:", "DCTM:", "DESCRIPTION", "EMBEDDED_RELATIONSHIP_ID", "EMBEDDED_RESOURCE_PATH",
        "EMBEDDED_RESOURCE_TYPE", "HAS_SIGNATURE", "IDENTIFIER", "LANGUAGE", "LATITUDE", "LONGITUDE", "METADATA_DATE",
        "MODIFIED", "MODIFIER", "ORIGINAL_RESOURCE_NAME", "PRINT_DATE", "PROTECTED", "PUBLISHER", "RATING", "RELATION",
        "RESOURCE_NAME_KEY", "REVISION", "RIGHTS", "SOURCE", "SOURCE_PATH", "SUBJECT", "TITLE"
    };
    return 0;
}

