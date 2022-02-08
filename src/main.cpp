#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "UnusedParameter"
#pragma ide diagnostic ignored "UnusedLocalVariable"
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

//RapidJson
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"




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
        std::multimap<std::string, boost::recursive_variant_>>::type Variant_Type;


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
        std::multimap<std::string, Variant_Type> map;
        for (auto& [obj_Key, obj_Val] : value.GetObject()) {
            map.emplace(obj_Key.GetString(), Value_Handler(obj_Val));

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

std::multimap<std::string ,Variant_Type> Process_Document (rj::Document& document) {
    std::multimap<std::string ,Variant_Type> meta_Data;

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

std::multimap<std::string, Variant_Type> Process_Json() {
    std::multimap<std::string, Variant_Type> map {};

        rj::Document document;
        const char* json = "[\n{\"author\": \"Xavier\","
                           "\"author\": \"Xavier\","
                           "\"auThor\": \"Xavier\","
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
                           "[\"This\", \"is\", \"level\", 2, \".\","
                           "[\"This\", \"is\", \"level\", 3, \".\"]]],"
                           "\"nested::object\":{\"level2\": {\"level3\": \"Casey\"}},"
                           "\"nested::object\":{\"level2\": {\"level3\": \"Casey\"}},"
                           "\"array::object\":[{\"level2\": {\"level3\": \"Xavier\"}}, [1,2,3,4]]}\n]";
        document.Parse(json);
        map = Process_Document(document);

    return map;
}

/*--------------------------------------------------------------------------------------------------------------------*/

struct Recursive_Print_T {
    // forwards for type `operator()`s.
    template <typename T1> void call(T1 const& value) const { return operator()(value); }

    // dispatch for variants
    template <typename... T2> void operator()(boost::variant<T2...> const& value) const {
        return boost::apply_visitor(*this, value);
    }

    void operator()(bool b) const { std::cout << std::boolalpha << b; }
    void operator()(int64_t i64) const { std::cout << i64; }
    void operator()(u_int64_t u64) const { std::cout << u64; }
    void operator()(double d) const { std::cout << d; }
    void operator()(const std::string& s) const { std::cout << std::quoted(s); }

    template <typename... Ts> void operator()(std::vector<Ts...> const& array) const {
        std::cout << "[ ";
        bool first = true;
        for (auto& value : array) {
            if (first) {
                first = false;
            }
            else {
                std::cout << ", ";
            }
            call(value);
        }
        std::cout << " ]";
    }

    template <typename T> void operator()(std::multimap<std::string, T> const& map) const {
        std::cout << "{ ";
        bool first = true;
        for (auto& [key, value]: map) {
            if (first) {
                first = false;
            } else {
                std::cout << ", ";
            }
            call(key);
            std::cout << ": ";
            call(value);
        }
        std::cout << " }";
    }
};

/*--------------------------------------------------------------------------------------------------------------------*/


int main() {
    //Creates vector of metadata key: value maps.
    std::vector<std::multimap<std::string, Variant_Type>> meta_Data {};
    meta_Data.emplace_back(Process_Json());

    std::vector<std::multimap<std::string, Variant_Type>> unique_Data {};
    for (auto& map : meta_Data) {
        std::multimap<std::string, Variant_Type> last_Pair {};
        for (auto& [key, value]: map) {
            if (last_Pair.empty()) {
                std::string low_key { boost::to_lower_copy(key)};
                last_Pair.emplace(low_key, value);
                continue;
            }

            std::multimap<std::string, Variant_Type> current_Pair {};
            std::string low_key { boost::to_lower_copy(key)};
            current_Pair.emplace(low_key, value);

            if (last_Pair != current_Pair) {
                unique_Data.emplace_back(current_Pair);
                last_Pair = current_Pair;
            }
            else {
                continue;
            }
        }
    }

    std::cout << "#######################################################################################" << std::endl;
    std::cout << "\t\t\tmeta_Data";
    Recursive_Print_T print;

    for (auto& map : meta_Data) {
        for(auto& [key, value]: map) {
            std::cout << key << ": ";
            print(value);
            std::cout << std::endl;
        }
    }

    std::cout << "#######################################################################################" << std::endl;
    std::cout << "\t\t\tunique_Data";

    for (auto& map : unique_Data) {
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

#pragma clang diagnostic pop
