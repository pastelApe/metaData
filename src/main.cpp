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
#include "boost/variant.hpp"
#include "boost/algorithm/string.hpp"

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
        std::map<std::string, boost::recursive_variant_>>::type variant_Value_Type;

/*--------------------------------------------------------------------------------------------------------------------*/

struct Type_Handler : public rj::BaseReaderHandler<rj::UTF8<>, Type_Handler> {
private:
    variant_Value_Type _props;
public:
    // && is normally only used to declare a parameter of a function. And it only takes an r-value expression.
    variant_Value_Type&& Get_Value() {
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
variant_Value_Type Value_Handler(rj::Value& value) {
    Get_String_Visitor print_String;

    if (value.IsObject()) {
        std::map<std::string, variant_Value_Type> map;

        std::cout << "{ ";
        for (auto& [obj_key, obj_val] : value.GetObject()) {
            std::cout << obj_key.GetString() << ": ";
            map.emplace(obj_key.GetString(), Value_Handler(obj_val));

        }
        std::cout << " }";

        return map;
    }
    else if (value.IsArray()) {
        // To remove comma from printing after last value in array, subtract 1.
        unsigned int array_Size = value.Size() - 1;
        std::vector<variant_Value_Type> vec;

        std::cout << "[ ";
        for (auto& arr_value: value.GetArray()) {
            vec.push_back(Value_Handler(arr_value));

            if (array_Size > 0) {
                --array_Size;
                std::cout << ", ";
            }
        }
        std::cout << " ]";

        return vec;
    }
    else {
        Type_Handler val_type;
        value.Accept(val_type);
        std::cout << print_String(value);
        return val_type.Get_Value();
    }
}
/*--------------------------------------------------------------------------------------------------------------------*/

std::map<std::string ,variant_Value_Type> Process_Document (rj::Document& document) {
    std::map<std::string ,variant_Value_Type> meta_Data;
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
                    std::cout << key.GetString() << ": ";
                    meta_Data.emplace(key.GetString(), Value_Handler(val));
                    std::cout << std::endl;
                }
            }
        }
    } else if (document.IsObject()) {
        for (auto& [key, val] : document.GetObject()) {
            std::cout << key.GetString() << ": ";
            meta_Data.emplace(key.GetString(), Value_Handler(val));
            std::cout << std::endl;
        }
    }
    return meta_Data;
}

/*--------------------------------------------------------------------------------------------------------------------*/

std::map<std::string, variant_Value_Type> Json_Handler() {
    std::map<std::string, variant_Value_Type> map {};

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

int main() {
        //Creates vector of metadata key: value maps.
        Get_String_Visitor print_Value;
        std::vector<std::map<std::string, variant_Value_Type>> meta_Data {};
        meta_Data.emplace_back(Json_Handler());


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

