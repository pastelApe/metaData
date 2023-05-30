#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "UnusedParameter"
#pragma ide diagnostic ignored "UnusedLocalVariable"
#pragma ide diagnostic ignored "HidingNonVirtualFunction"
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma ide diagnostic ignored "readability-convert-member-functions-to-static"

//STL
#include <filesystem>
#include <map>
#include <ranges>
#include <vector>
#include <iostream>
#include <fstream>

//Boost
#include "boost/variant.hpp"
#include "boost/algorithm/string.hpp"

//FMT
#include "fmt/core.h"
#include "fmt/color.h"

//RapidJson
#include "rapidjson/include/rapidjson/document.h"
#include "rapidjson/include/rapidjson/error/en.h"
#include "rapidjson/include/rapidjson/filereadstream.h"
#include "rapidjson/include/rapidjson/writer.h"
#include "rapidjson/include/rapidjson/stringbuffer.h"

namespace  fs = std::filesystem;
namespace  rj = rapidjson;

/**
 * Multimap chosen to allow for duplicate key values.
 */
typedef boost::make_recursive_variant<
        bool,
        int64_t,
        uint64_t,
        double,
        std::string,
        std::vector<boost::recursive_variant_>,
        std::multimap<std::string, boost::recursive_variant_>>::type VariantType;

/**
 * Reproduced from the Rapidjson "simplereader" tutorial.
 */
struct Type_Handler : public rj::BaseReaderHandler<rj::UTF8<>, Type_Handler> {
private:
    VariantType _type;
public:
    // && is normally only used to declare a parameter of a function. And it only takes an r-value expression.
    VariantType&& Get_Value() {
        return std::move(_type);
    }

    bool Null() { return true; }
    bool Bool(bool b) { _type = b; return true; }
    bool Int(int i) { _type = static_cast<int64_t> (i); return true; }
    bool Uint(unsigned u) { _type = static_cast<uint64_t> (u);  return true; }
    bool Int64(int64_t i64) { _type = static_cast<int64_t> (i64); return true; }
    bool Uint64(uint64_t u64) { _type = static_cast<uint64_t> (u64) ; return true; }
    bool Double(double d) { _type = d; return true; }
    bool String(const char* str, rj::SizeType length, bool copy) { _type = std::string(str, length); return true; }
    bool StartObject() { return true; }
    bool Key(const char* str, rj::SizeType length, bool copy) { _type = std::string(str, length); return true; }
    bool EndObject(rj::SizeType memberCount) { return true; }
    bool StartArray() { return true; }
    bool EndArray(rj::SizeType elementCount) { return true; }
};

/**
 * Recursive function to parse the value to include child objects and arrays.
 * @param value of the parent object found in the Document.
 * @return VariantType of the value.
 */
VariantType Value_Handler(rj::Value& value) {
    if (value.IsObject()) {
        auto map { std::multimap<std::string, VariantType>() };

        for (auto& [objKey, objValue] : value.GetObject()) {
            map.emplace(objKey.GetString(), Value_Handler(objValue));
        }
        return map;
    }

    else if (value.IsArray()) {
        auto variantVector { std::vector<VariantType>() };

        for (auto& arrayValue: value.GetArray()) {
            variantVector.push_back(Value_Handler(arrayValue));
        }
        return variantVector;
    }

    // objValue is not an object or array.
    else {
        auto valueType { Type_Handler() };
        value.Accept(valueType);
        return valueType.Get_Value();
    }
}

/**
 * Process document.
 * @param document is an in-memory representation of JSON for query and manipulation.
 * @return multimap with metadata in document.
 */
std::multimap<std::string ,VariantType> DocumentHandler (rj::Document& document) {
    auto metaData {std::multimap<std::string ,VariantType>() };

    if (document.HasParseError()) {
        throw std::runtime_error(fmt::format("Error (offset {}): {}\n",
                                             (unsigned)document.GetErrorOffset(),
                                             rj::GetParseError_En(document.GetParseError())));
    }

    if (document.IsArray()) {
        for (auto& prop : document.GetArray()) {
            if (prop.IsObject()) {
                for (auto& [key, value]: prop.GetObject()) {
                    metaData.emplace(key.GetString(), Value_Handler(value));
                }
            }
        }
    } else if (document.IsObject()) {
        for (auto& [key, value] : document.GetObject()) {
            metaData.emplace(key.GetString(), Value_Handler(value));
        }
    }
    return metaData;
}

/**
 * Parse file into json document and process the document into a map.
 * @return JSON Document
 */
std::multimap<std::string, VariantType> ParseDocument(const std::filesystem::path& path) {
    FILE* file = fopen(path.c_str(), "r");

    char readBuffer [65536] ;
    auto stream { rj::FileReadStream(file, readBuffer, sizeof(readBuffer)) };
    auto document { rj::Document() };
    document.ParseStream(stream);

    fclose(file);
    return std::multimap<std::string, VariantType>(DocumentHandler(document));

}

/**
 * Custom Value printer for pretty printing.
 */
struct PrintValue {
    // forwards for type `operator()`s.
    template <typename T1> void call(T1 const& value) const { return operator()(value); }

    // dispatch for variants
    template <typename... T2> void operator()(boost::variant<T2...> const& value) const {
        return boost::apply_visitor(*this, value);
    }

    void operator()(bool b)               const { fmt::print("{}", b); }
    void operator()(int64_t i64)          const { fmt::print("{}", i64); }
    void operator()(u_int64_t u64)        const { fmt::print("{}", u64); }
    void operator()(double d)             const { fmt::print("{}", d); }
    void operator()(const std::string& s) const { fmt::print("{:?}", s); } // :? for quoted strings

    template <typename... Ts> void operator()(std::vector<Ts...> const& array) const {

        for (auto& value : array) {
            if(!array.empty()) {
                if (value == array.front()) {
                    fmt::print("[");
                    call(value);
                } else if (value != array.back()){
                    call(value);
                    fmt::print(", ");
                } else {
                    call(value);
                    fmt::print("]");
                }
            }
        }
    }

    template <typename T> void operator()(std::multimap<std::string, T> const& map) const {
        fmt::print("{}", "{");
        bool first = true;
        for (auto& [key, value]: map) {
            if (first) {
                first = false;
            } else {
                fmt::print(", ");
            }
            call(key);
            fmt::print(": ");
            call(value);
        }
        fmt::print("{}", "}");
    }
};


/**
  * Contains the core set of basic Tika metadata properties, which all parsers will attempt to supply.
  * This set also includes the Dublin Core properties as well as properties found from custom fields that
  * match a core field.
  * Removed "FORMAT", "TYPE". Included "AUTHOR".
  * Any metadata keys that do not match will be disregarded.
  */
std::vector<std::string> CORE_KEYS {
        "AUTHOR", "ALTITUDE", "COMMENTS", "CONTRIBUTOR", "COVERAGE", "CREATED", "CREATOR", "CREATOR_TOOL", "DATE",
        "DC:", "DC.", "DC_", "DCTERMS:", "DCTM:", "DESCRIPTION", "EMBEDDED_RELATIONSHIP_ID", "EMBEDDED_RESOURCE_PATH",
        "EMBEDDED_RESOURCE_TYPE", "HAS_SIGNATURE", "IDENTIFIER", "LANGUAGE", "LATITUDE", "LONGITUDE", "METADATA_DATE",
        "MODIFIED", "MODIFIER", "ORIGINAL_RESOURCE_NAME", "PRINT_DATE", "PROTECTED", "PUBLISHER", "RATING", "RELATION",
        "RESOURCE_NAME_KEY", "REVISION", "RIGHTS", "SOURCE", "SOURCE_PATH", "SUBJECT", "TITLE"
};


void CorePrinter(std::vector<std::multimap<std::string, VariantType>>& coreData) {
    std::sort(coreData.begin(), coreData.end());

    fmt::print(fmt::emphasis::bold | fmt::emphasis::underline,"\t\t\tUnique Core MetaData\t\t\t\n\n");
    PrintValue print;

    fmt::print("[ \n");
    auto count = 1;
    for (auto& core : coreData) {
        auto size = coreData.size();
        if (!core.empty()) {
            for(auto& [key, value]: core) {
                if (std::ranges::find(CORE_KEYS, key) != CORE_KEYS.end()) {
                    fmt::print(" {:?}: ", key);
                    print(value);

                    if (count != size) {
                        fmt::print(",");
                    }
                    count++;
                    fmt::print("\n");
                }
            }
        }
    }
    fmt::print("]");
}

int main(int argc, char *argv[]) {
    //Store test data in map.
    auto metaData {std::vector<std::multimap<std::string, VariantType>>() };
    metaData.emplace_back(ParseDocument(std::filesystem::path(argv[1])));

    auto uniqueCoreData {std::vector<std::multimap<std::string, VariantType>>() };

    for (auto& map : metaData) {
        auto lastPair {std::multimap<std::string, VariantType>() };

        for (auto& [key, value]: map) {
            if (lastPair.empty()) {
                lastPair.emplace(boost::to_upper_copy(key), value);
                continue;
            }

            auto currentPair {std::multimap<std::string, VariantType>() };
            currentPair.emplace(boost::to_upper_copy(key), value);
            //Check for duplicates and test against CORE_KEYS. Save unique objects.
            if (lastPair != currentPair) {
                uniqueCoreData.emplace_back(currentPair);

                lastPair = currentPair;
            }
            else {
                continue;
            }
        }
    }

    CorePrinter(uniqueCoreData);

    return 0;
}

#pragma clang diagnostic pop
