//STL
#include <filesystem>
#include <iostream>
#include <vector>
#include <map>

//Boost
#include "boost/variant.hpp"

//C++ Requests: Curl for People
#include <cpr/cpr.h>

//RapidJson
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"

namespace  fs = std::filesystem;
namespace  rj = rapidjson;

typedef boost::make_recursive_variant<
        bool,
        int64_t,
        uint64_t,
        double,
        std::string,
        std::vector<boost::recursive_variant_>,
                std::map<std::string, boost::recursive_variant_>
                        >::type variant_type;


class meta_value {
private:
    std::vector<variant_type> _props;
public:
    std::vector<variant_type> get_value(variant_type& v) {_props.emplace_back(std::move(v)); return _props;}
};


struct MyHandler : public rj::BaseReaderHandler<rj::UTF8<>, MyHandler> {
    variant_type prop {};
    
    bool Null() { return true; }
    bool Bool(bool b) { prop = b; return true; }
    bool Int(int i) { prop = static_cast<int64_t> (i); return true; }
    bool Uint(unsigned u) { prop = static_cast<uint64_t> (u);  return true; }
    bool Int64(int64_t i64) { prop = static_cast<int64_t> (i64) return true; }
    bool Uint64(uint64_t u64) { prop = static_cast<unt64_t> (u64) ; return true; }
    bool Double(double d) { prop = d; return true; }
    bool String(const char* str, rj::SizeType length, bool copy) { prop = std::string(str, length) ; return true;
    }
    bool StartObject() { return true; }
    bool Key(const char* str, rj::SizeType length, bool copy) { prop = std::string(str, length); return true;
    }
    bool EndObject(rj::SizeType memberCount) { return true; }
    bool StartArray() { return true; }
    bool EndArray(rj::SizeType elementCount) { return true; }
};


variant_type type_handler(const auto& value ) {
    std::map<std:string, variant_type> meta {};
    if (value.IsObject()) {
        for (const auto& [key, val] : value) {

    }
}

/*--------------------------------------

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

cpr::Response put_Request(std::vector<char>& buff, fs::path& path) {
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

// Look for any string included in the vector.
bool subStr_Found (std::vector<std::string>& keys, std::string& to_Find) {
    bool found = std::any_of(keys.begin(), keys.end(), [&](auto key) {
        return (to_Find.find(key) != std::string::npos);
    });

    return found;

    /*  Contains the core set of basic Tika metadata properties, which all parsers will attempt to supply.
        This set also includes the Dublin Core properties.
        Removed properties that can be stored in the flat file for later. */
    std::vector<std::string> core_Keys{
            "ALTITUDE", "COMMENTS", "CONTRIBUTOR", "COVERAGE", "CREATED", "CREATOR", "CREATOR_TOOL", "DATE", "DC:",
            "DC.", "DC_", "DCTERMS:", "DCTM:", "DESCRIPTION", "EMBEDDED_RELATIONSHIP_ID", "EMBEDDED_RESOURCE_PATH",
            "EMBEDDED_RESOURCE_TYPE", "HAS_SIGNATURE", "IDENTIFIER", "LANGUAGE", "LATITUDE", "LONGITUDE",
            "METADATA_DATE", "MODIFIED", "MODIFIER", "ORIGINAL_RESOURCE_NAME", "PRINT_DATE", "PROTECTED", "PUBLISHER",
            "RATING", "RELATION", "RESOURCE_NAME_KEY", "REVISION", "RIGHTS", "SOURCE", "SOURCE_PATH", "SUBJECT",
            "TITLE",
            // Matched many keys with these substrings. Should return with the "dc" prefix. Removed "FORMAT", "TYPE".
    };

    std::vector<std::string> ignore{"UNKNOWN"};
}
    

void prettier_Printer(rj::GenericMember<rj::UTF8<char>, rj::MemoryPoolAllocator<rj::CrtAllocator>> &key) {
    std::cout << key.name.GetString() << ": ";

    if (key.value.IsArray()) {
        // Subtract 1 to remove comma from printing after last value in array.
        unsigned int array_Size = key.value.Size() - 1;
        std::cout  << "[ ";

        for (auto& value : key.value.GetArray()) {
            std::cout << value.GetString();

            if (array_Size > 0) {
                --array_Size;
                std::cout << ", ";
            }
        }
        std::cout  << " ]\n";
    } else {
        std::cout << key.value.GetString() << '\n';
    }
}

void parse_Object(rj::GenericValue<rj::UTF8<char>, rj::MemoryPoolAllocator<rj::CrtAllocator>>& member) {
    if (!member.IsObject()) {
        std::cout << "JSON is not an object. Failed to parse. Type is: " << strerror(member.GetType());
    }

    for (auto &key: member.GetObject()) {

        std::string key_Name = key.name.GetString();

//        if (key_Name == "X-TIKA:content")
//            continue;

//        prettier_Printer(key);
    }
}

void parse_Document (rj::Document& document, const char* json) {
    document.Parse(json);

    if(!document.IsArray())
        parse_Object(document);

    for (auto &member: document.GetArray()) {
        if (member.IsObject())
            parse_Object(member);
    }
}

void ParseMessage(cpr::Response& response) {
    MyHandler handler;
    rj::Reader reader;
    std::string json {response.text};
    rj::StringStream stream(json.c_str());

    reader.IterativeParseInit();
    while (!reader.IterativeParseComplete()) {
        reader.IterativeParseNext<rj::kParseDefaultFlags>(stream, handler);
    }

}

void parse_Helper(fs::path& path) {
    std::vector<char> buff {file_Buffer(path)};
    cpr::Response response {put_Request(buff, path)};

    ParseMessage(response);
}

int main(int argc,char* argv[]) {
    //If a path is not passed via CLI.
    if (argc != 2) {
        std::cout << "Please enter a valid path." << "\n";
        return 1;
    }
    else {
        fs::path path{argv[1]};
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
        parse_Helper(path);
    }
    return 0;
}
