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
        std::map<std::string, boost::recursive_variant_>>::type Variant_Data;

/*--------------------------------------------------------------------------------------------------------------------*/

struct Type_Handler : public rj::BaseReaderHandler<rj::UTF8<>, Type_Handler> {
private:
    Variant_Data _props;
public:
    // && is normally only used to declare a parameter of a function. And it only takes an r-value expression.
    Variant_Data&& Get_Value() {
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

class Get_String_Visitor
    : public boost::static_visitor<std::string>
{
public:
    std::string operator() (rj::Value& value) {
        std::ostringstream s;
        switch (value.GetType()) {
            case rj::kNullType :
                s << "Null";
                break;
            case rj::kFalseType :
            case rj::kTrueType :
                s << std::boolalpha << value.GetBool();
                break;
            case rj::kNumberType :
                if (value.IsUint64()) {
                    s << value.GetUint64();
                } 
                else if (value.IsInt64()) {
                    s << value.GetInt64();
                } 
                else if (value.IsDouble()) {
                    s << value.GetDouble();
                }
                break;
            case rj::kStringType :
                s << value.GetString();
                break;
            case rj::kObjectType :
            case rj::kArrayType :
                break;
        }
        std::string ss {s.str()};
        boost::trim(ss);
        return ss;
    }
};

/*--------------------------------------------------------------------------------------------------------------------*/

//Value_Handler is a recursive function. Checks all values including child objects and arrays.
Variant_Data Value_Handler(rj::Value& value) {
    Get_String_Visitor print_String;

    if (value.IsObject()) {
        std::map<std::string, Variant_Data> map;
        for (auto& [obj_key, obj_val] : value.GetObject()) {
            map.emplace(obj_key.GetString(), Value_Handler(obj_val));

        }
        return map;
    }
    else if (value.IsArray()) {
        // To remove comma from printing after last value in array, subtract 1.
        std::vector<Variant_Data> vec;
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

std::map<std::string ,Variant_Data> Process_Document (rj::Document& document) {
    std::map<std::string ,Variant_Data> meta_Data;
    Get_String_Visitor print_String;

    if (document.HasParseError()) {
        fprintf(stderr, "Error (offset %u): %s",
                (unsigned)document.GetErrorOffset(),
                rj::GetParseError_En(document.GetParseError()));
        std::cout << std::endl;
    }

    if (document.IsArray()) {
        for (auto& prop : document.GetArray()) {
            if (prop.IsObject()) {
                for (auto& [key, val]: prop.GetObject()) {
                    meta_Data.emplace(key.GetString(), Value_Handler(val));
                }
            }
        }
    } else if (document.IsObject()) {
        for (auto& [key, val] : document.GetObject()) {
            meta_Data.emplace(key.GetString(), Value_Handler(val));
        }
    }
    return meta_Data;
}

/*--------------------------------------------------------------------------------------------------------------------*/

std::map<std::string, Variant_Data> Json_Handler() {
    std::map<std::string, Variant_Data> map {};

        rj::Document document;
        const char* json = "[\n{\"author\": \"Xavier\","
                           "\"Author\": \"Casey\","
                           "\"creator\": \"Xavier\","
                           "\"Created-By\": [\"Xavier\", \"Casey\"],"
                           "\"hello\": \"world\","
                           "\"true\": true,"
                           "\"false\": false,"
                           "\"null\": null,"
                           "\"int64\": 123,"
                           "\"uint64\": 123456,"
                           "\"pi\": 3.1416,"
                           "\"array\": [1, 2, 3, 4],"
                           "\"nested::array\": [\"This\", \"is\", \"level\", 1, \".\","
                           "[\"This\", \"is\", \"level\", 2, \".\"]],"
                           "\"nested::object\":{\"level2\": {\"level3\": \"sub-value\"}}}\n]";
        document.Parse(json);
        map = Process_Document(document);

    return map;
}

/*--------------------------------------------------------------------------------------------------------------------*/

struct Recursive_Printer {
    using result_type = void;
    std::ostream& _os;

    // forwards for `operator()`
    template <typename Ta> void call(Ta const& value) const {
        return operator()(value);
    }
    // dispatch for variants
    template <typename Tb> void operator()(boost::variant<Tb> const& value) const {
        return boost::apply_visitor(*this, value);
    }

    void operator()(int64_t i64) const { _os << i64; }
    void operator()(u_int64_t u64) const { _os << u64; }
    void operator()(double iD) const { _os << iD; }
    void operator()(std::string const& string) const { _os << std::quoted(string); }
    void operator()(bool _bool) const { _os << std::boolalpha << _bool; }

    template <typename Tb> void operator()(std::vector<Tb> const& array) const {
        _os << "[ ";
        bool first = true;
        for (auto &entry: array) {
            if (first) {
                first = false;
            } else {
                _os << ", ";
                call(entry);
            }
            _os << " ]";
        }
    }

    template <typename Tb> void operator()(std::map<std::string, Tb> const& map) const {
        _os << "{ ";
        bool first = true;
        for (auto& [key, value]: map) {
            if (first) {
                first = false;
            } else {
                _os << ", ";
                call(key);
                _os << ": ";
                call(value);
            }
            _os << " }";
        }
    }
};

/*--------------------------------------------------------------------------------------------------------------------*/

int main() {
        //Creates vector of metadata key: value maps.
        Get_String_Visitor print_Value;
        std::vector<std::map<std::string, Variant_Data>> meta_Data {};
        meta_Data.emplace_back(Json_Handler());

        for (auto& map : meta_Data) {
            for(auto& [key, value]: map) {

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

