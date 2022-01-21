//STL
#include <filesystem>
#include <iostream>
#include <vector>
#include <map>

//Boost
#include "boost/variant.hpp"

//C++ Requests: Curl for People
#include <cpr/cpr.h>
#include <set>

//RapidJson
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"


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
std::map<std::string, boost::recursive_variant_>>::type variant_type;

/*--------------------------------------------------------------------------------------------------------------------*/

struct Type_Handler : public rj::BaseReaderHandler<rj::UTF8<>, Type_Handler> {
private:
    variant_type _props;
public:
    variant_type&& get_value() {return std::move(_props); }


    bool Null() { return true; }
    bool Bool(bool b) { _props = b; return true; }
    bool Int(int i) { _props = static_cast<int64_t> (i); return true; }
    bool Uint(unsigned u) { _props = static_cast<uint64_t> (u);  return true; }
    bool Int64(int64_t i64) { _props = static_cast<int64_t> (i64); return true; }
    bool Uint64(uint64_t u64) { _props = static_cast<uint64_t> (u64) ; return true; }
    bool Double(double d) { _props = d; return true; }
    bool String(const char* str, rj::SizeType length, bool copy) { _props = std::string(str, length); return true;
    }
    bool StartObject() { return true; }
    bool Key(const char* str, rj::SizeType length, bool copy) { _props = std::string(str, length); return true;
    }
    bool EndObject(rj::SizeType memberCount) { return true; }
    bool StartArray() { return true; }
    bool EndArray(rj::SizeType elementCount) { return true; }
};

/*--------------------------------------------------------------------------------------------------------------------*/

bool key_found (std::vector<std::string>& keys, const std::string& to_find) {
    //Make case-insensitive for comparison.
    std::string u_key {to_find};
    std::transform(u_key.begin(), u_key.end(), u_key.begin(), ::toupper);

    bool found = std::any_of(keys.begin(), keys.end(), [&](auto key) {
        return (u_key.find(key) != std::string::npos);
    });

    return found;
}
std::string print_value(const rj::Value& val) {
    Type_Handler val_type;
    val.Accept(val_type);

    std::string value{};

    switch (val.GetType()) {
        case rj::kNullType :
            return value = "Null";
            break;
        case rj::kFalseType :
            return value = "False";
            break;
        case rj::kTrueType :
            return value = "True";
            break;
        case rj::kNumberType :
            return value = "Number";
            break;
        case rj::kStringType :
            return value = val.GetString();
        case rj::kArrayType :
            return value = "Array";
            break;
        case rj::kObjectType :
            return value = "Object";
            break;
    }
    return value;
}

/*--------------------------------------------------------------------------------------------------------------------*/

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

//value_handler is a recursive function. Checks all values for child objects or arrays.
variant_type value_handler(const rj::Value& val) {
    if (val.IsObject()) {
        std::map<std::string, variant_type> map;

        for (const auto& [obj_key, obj_val] : val.GetObject()) {
            map.emplace(obj_key.GetString(), value_handler(obj_val));
        }

        return map;
    } else if (val.IsArray()) {
        std::vector<variant_type> vec;

        for (const auto& arr_value: val.GetArray()) {
            vec.push_back(value_handler(arr_value));
        }

        return vec;
    } else {
        Type_Handler val_type;
        val.Accept(val_type);

        return val_type.get_value();
    }
}

auto process_doc (rj::Document& doc, const char* json) {
    doc.Parse(json);
    std::map<std::string ,variant_type> meta_data;

    if (doc.HasParseError()) {
        fprintf(stderr, "Error (offset %u): %s",
                (unsigned)doc.GetErrorOffset(),
                rj::GetParseError_En(doc.GetParseError()));
        std::cout << std::endl;
    }

    if (doc.IsArray()) {
        for (const auto& prop : doc.GetArray()) {
            if (prop.IsObject()) {
                for (const auto& [key, val] : prop.GetObject()) {
                    meta_data.emplace(key.GetString(), value_handler(val));
                }
            }
        }
    } else if (doc.IsObject()) {
        for (auto& [key, val] : doc.GetObject()) {
            meta_data.emplace(key.GetString(), value_handler(val));
        }
    }
    return meta_data;
}

/*--------------------------------------------------------------------------------------------------------------------*/

std::vector<char> file_Buffer (fs::path& path) {
    //Open the file.
    std::ifstream stream {path.string()};

    if (!stream.is_open()) {
        std::cerr << "Could not open file for reading!" << "\n";
        std::exit(1);
    }

    std::vector<char> buff {
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()
    };

    return buff;
}

/*--------------------------------------------------------------------------------------------------------------------*/

cpr::Response post_request(std::vector<char>& buff, fs::path& path) {
    fs::path file_Name = path.filename();
    auto url {cpr::Url{"http://localhost:9998/rmeta/form/text"}};
    auto buff_Size {cpr::Buffer{buff.begin(), buff.end(), file_Name}};

    // Set maxEmbeddedResources to 0 for parent object metadata only.
    auto header {cpr::Header{ {"maxEmbeddedResources", "0"},
                              {"X-Tika-OCRskipOcr", "true"},
                              {"accept", "application/json"}}};

    cpr::Response response {cpr::Post(url, header, cpr::Multipart{{file_Name, buff_Size}})};

    if (response.status_code != 200) {
        std::cout << "Failed to processes request. Error code: "
                  << response.status_code << ". \n"
                  << "Failed Path: " << path.relative_path() << "\n";
    }

    return response;
}
/*--------------------------------------------------------------------------------------------------------------------*/

std::set<std::string> keys {};
std::set<std::string> path_handler(fs::path& path) {
    if (is_regular_file(path)) {
        std::vector<char> buff{file_Buffer(path)};
        std::string response { post_request(buff, path).text };
        rj::Document doc;

        for (auto& key : process_doc(doc, response.c_str())) {
            keys.emplace(key.first);
        }

    } else if (is_directory(path)) {
        for (auto &dir_entry: fs::recursive_directory_iterator(path)) {
            path_handler((fs::path &) dir_entry);
        }
    }
    return keys;
}

/*--------------------------------------------------------------------------------------------------------------------*/

void output_File(std::set<std::string>& set) {
    std::string path{"/home/xavier/Workspace/metaData/results/unique_keys.txt"};
    std::ofstream file(path);

    for (auto& key : set) {
        std::string ignore = "Unknown";
        std::size_t found = key.find(ignore);
        if (found != std::string::npos) {
            continue;
        } else {
            file << key << std::endl;
        }
    }

    file.close();
}

/*--------------------------------------------------------------------------------------------------------------------*/

int main(int argc,char* argv[]) {
    std::set<std::string> meta_data {};
    //If a path is not passed via CLI.
    if (argc != 2) {
        std::cout << "Please enter a valid path." << std::endl;
        return 1;
    } else {
        fs::path path { argv[1] };
        try {
            fs::exists(path);
        }
        catch (fs::filesystem_error const &fs_error) {
            std::cout
                    << "what(): " << fs_error.what() << "\n"
                    << "path1():" << fs_error.path1() << "\n"
                    << "path2():" << fs_error.path2() << "\n"
                    << "code().value():   " << fs_error.code().value() << "\n"
                    << "code().message(): " << fs_error.code().message() << "\n"
                    << "code().category():" << fs_error.code().category().name() << "\n";
        }

//          Contains the core set of basic Tika metadata properties, which all parsers will attempt to supply.
//          This set also includes the Dublin Core properties.
//          Removed properties that can be stored in the flat file for later.
        std::vector<std::string> core_Keys{
                "ALTITUDE", "COMMENTS", "CONTRIBUTOR", "COVERAGE", "CREATED", "CREATOR", "CREATOR_TOOL", "DATE", "DC:",
                "DC.", "DC_", "DCTERMS:", "DCTM:", "DESCRIPTION", "EMBEDDED_RELATIONSHIP_ID", "EMBEDDED_RESOURCE_PATH",
                "EMBEDDED_RESOURCE_TYPE", "HAS_SIGNATURE", "IDENTIFIER", "LANGUAGE", "LATITUDE", "LONGITUDE",
                "METADATA_DATE", "MODIFIED", "MODIFIER", "ORIGINAL_RESOURCE_NAME", "PRINT_DATE", "PROTECTED",
                "PUBLISHER",
                "RATING", "RELATION", "RESOURCE_NAME_KEY", "REVISION", "RIGHTS", "SOURCE", "SOURCE_PATH", "SUBJECT",
                "TITLE",
                // Matched many keys with these substrings. Should return with the "dc" prefix. Removed "FORMAT", "TYPE".
        };

        meta_data = path_handler(path);
    }

    output_File(meta_data);
    return 0;
}
