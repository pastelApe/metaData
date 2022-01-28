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
    // && is normally only used to declare a parameter of a function. And it only takes an r-value expression.
    variant_type&& get_value() {
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
void print_value(rj::Value& val) {
    Type_Handler val_type;
    val.Accept(val_type);

    std::string value{};

    switch (val.GetType()) {
        case rj::kNullType :
            std::cout << "Null";
            break;
        case rj::kFalseType :
        case rj::kTrueType :
            std::cout << std::boolalpha << val.GetBool();
            break;
        case rj::kNumberType :

            if (val.IsUint64()) {
                std::cout << val.GetUint64();
            } else if (val.IsInt64()) {
                std::cout << val.GetInt64();
            } else {
                std::cout << val.GetDouble();
            }
            break;
        case rj::kStringType :
            std::cout << val.GetString();
        case rj::kArrayType :
            std::cout << "[ ";
            for (auto& v : val.GetArray()) {
                print_value(v);
                std::cout << ", ";
            }
            std::cout << " ]";
            break;
        case rj::kObjectType :
            for (auto& obj_v : val.GetObject()) {
                std::cout << "{ ";
                print_value((rj::Value &)obj_v);
            }
            break;
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/

//value_handler is a recursive function. Checks all values including child objects and arrays.
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

/*--------------------------------------------------------------------------------------------------------------------*/

std::map<std::string ,variant_type> process_doc (rj::Document& doc, const char* json) {
    doc.Parse(json);
    std::map<std::string ,variant_type> meta_data;

    if (doc.HasParseError()) {
        fprintf(stderr, "Error (offset %u): %s",
                (unsigned)doc.GetErrorOffset(),
                rj::GetParseError_En(doc.GetParseError()));
        std::cout << std::endl;
    }

    if (doc.IsArray()) {
        for (auto& prop : doc.GetArray()) {
            if (prop.IsObject()) {
                for (auto& [key, val]: prop.GetObject()) {
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
    cpr::Url url {cpr::Url{"http://localhost:9998/rmeta/form/text"}};
    cpr::Buffer buff_Size {cpr::Buffer{buff.begin(), buff.end(), file_Name}};

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


std::map<std::string, variant_type> path_handler(fs::path& path) {
    if (is_regular_file(path)) {
        std::vector<char> buff{file_Buffer(path)};
        std::string response { post_request(buff, path).text };
        rj::Document doc;

        process_doc(doc, response.c_str());

    } else if (is_directory(path)) {
        for (auto &dir_entry: fs::recursive_directory_iterator(path)) {
            path_handler((fs::path &) dir_entry);
        }
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/

void create_file (std::string& core_path, std::set<std::string>& key_list) {
    std::set<std::string> core_set;

    for (auto& key : key_list) {
        core_set.emplace(key);
    }
    std::ofstream core_file(core_path);

    for (auto& key: core_set) {
        core_file << key << std::endl;
    }

    core_file.close();

}

/*--------------------------------------------------------------------------------------------------------------------*/

int main(int argc,char* argv[]) {
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
        //Creates vector of metadata key: value maps.
        std::vector<std::map<std::string, variant_type>> meta_data {};
        meta_data.emplace_back(path_handler(path));

        //Access each map in the vector
        for (const auto& map : meta_data) {
            for (const auto& [key, value] : map) {
                std::cout << key << ": ";
                print_value((rapidjson::Value &) value);
                std::cout << std::endl;
            }
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

