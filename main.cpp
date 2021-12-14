#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <variant>
#include <vector>

#include <cpr/cpr.h>

#include <rapidjson/document.h>
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

namespace  fs = std::filesystem;
namespace  rj = rapidjson;

std::vector<std::string> file_Content;

std::map<std::string, std::variant<nullptr_t, int, unsigned, int64_t, uint64_t, double, std::string>> core_Map;

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
}

void create_Maps (rj::GenericMember<rj::UTF8<char>, rj::MemoryPoolAllocator<rj::CrtAllocator>> &key) {
    std::string key_Name = key.name.GetString();

    /*
    enum Type {
        kNullType = 0,      //!< null
        kFalseType = 1,     //!< false
        kTrueType = 2,      //!< true
        kObjectType = 3,    //!< object
        kArrayType = 4,     //!< array
        kStringType = 5,    //!< string
        kNumberType = 6     //!< number
    };
    */

    std::variant<nullptr_t, int, unsigned, int64_t, uint64_t, double, std::string> value {};

    switch(key.value.GetType()) {
        case rj::kNullType :
            value = nullptr;
            break;
        case rj::kFalseType :
            break;
        case rj::kTrueType  :
            //TODO;
            break;
        case rj::kObjectType :
            //TODO;
            break;
        case rj::kArrayType  :
            //TODO;
            break;
        case rj::kStringType :
            //TODO;
            break;
        case rj::kNumberType :
            if (key.value.IsUint())
            break;
        default :
            std::cout << rj::kValidateErrorType << '\n';
            break;
    }

    //Make case-insensitive for comparison.
    std::string upper_Name = key_Name;
    std::transform(upper_Name.begin(), upper_Name.end(), upper_Name.begin(), ::toupper);

    /*  Contains the core set of basic Tika metadata properties, which all parsers will attempt to supply.
        This set also includes the Dublin Core properties.
        Removed properties that can be stored in the flat file for later. */
    std::vector<std::string> core_Keys {
        "ALTITUDE", "COMMENTS", "CONTRIBUTOR", "COVERAGE", "CREATED", "CREATOR", "CREATOR_TOOL", "DATE", "DC:",
        "DC.", "DC_", "DCTERMS:", "DCTM:", "DESCRIPTION", "EMBEDDED_RELATIONSHIP_ID", "EMBEDDED_RESOURCE_PATH",
        "EMBEDDED_RESOURCE_TYPE", "HAS_SIGNATURE", "IDENTIFIER", "LANGUAGE", "LATITUDE", "LONGITUDE",
        "METADATA_DATE", "MODIFIED", "MODIFIER", "ORIGINAL_RESOURCE_NAME", "PRINT_DATE", "PROTECTED", "PUBLISHER",
        "RATING", "RELATION", "RESOURCE_NAME_KEY", "REVISION", "RIGHTS", "SOURCE", "SOURCE_PATH", "SUBJECT", "TITLE",
        // Matched many keys with these substrings. Should return with the "dc" prefix. Removed "FORMAT", "TYPE".
    };

    std::vector<std::string> ignore {"UNKNOWN"};

    if (subStr_Found(ignore, upper_Name)) {
        //Do nothing.
    } else if (subStr_Found(core_Keys, upper_Name)) {
        core_Map.emplace(key_Name, );
    } else {
        remaining_Keys.emplace(key_Name);
    }
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
        create_Maps(key);

        std::string key_Name = key.name.GetString();

//        if (key_Name == "X-TIKA:content")
//            continue;

//        prettier_Printer(key);
    }
}

void parse_Array(rj::Document& doc) {
    for (auto &member: doc.GetArray()) {
        if (member.IsObject())
            parse_Object(member);
    }
}

void parse_Document (rj::Document& document, const char* json) {
    document.Parse(json);

    if(document.IsArray()) {
        parse_Array(document);
    } else {
        parse_Object(document);
    }
}

void parse_Json(cpr::Response& response) {
    //Validate json schema.
    rj::Document document;
    if (document.HasParseError()) {
        std::cout << "File is not valid JSON. Error: " << GetParseError_En(document.GetParseError());
    }

    std::string json {response.text};
    const char* c_Json(json.c_str());

    parse_Document(document, c_Json);
}

void parse_Helper(fs::path& path) {
    std::vector<char> buff {file_Buffer(path)};
    cpr::Response response {put_Request(buff, path)};

    parse_Json(response);
}

void parse_Dir_Entry(const fs::directory_entry& dir_Entry) {
    if (!dir_Entry.is_regular_file()) {
        std::cout << "Not a file. Moving on\n";
    }
    else {
        parse_Helper((fs::path &) dir_Entry.path());
    }
}

void output_File(std::set<std::string>& set) {
    std::string path {"/home/xavier/Desktop/unique_keys.txt"};
    std::ofstream file(path);

    for (auto& i : set) {
        file << i << "\n";
    }
}

void edit_File() {
    std::ifstream key_File ("/home/xavier/Desktop/unique_keys.txt");
    std::ofstream new_File("/home/xavier/Desktop/unique_keys_edit.txt");
    std::string line;

    if (!key_File.is_open()){
        std::cout << "Failed to open file.\n";
        std::exit (1);
    }

    while (getline(key_File, line)){
        std::string ignore = "Unknown";
        std::size_t found = line.find(ignore);
        if (found != std::string::npos) {
            continue;
        }
        else {
            new_File << line << '\n';
        }
    }

    key_File.close();
    new_File.close();
}


int main(int argc,char* argv[]) {
    //If a path is not passed via CLI.
    if (argc != 2) {
        std::cout << "Please enter a valid path." << "\n";
        return 1;
    }
    else {
        fs::path path {argv[1]};
        try {
            fs::exists(path);
        }
        catch (fs::filesystem_error const& fs_error) {
            std::cout
                    << "what(): "           << fs_error.what()                      << "\n"
                    << "path1():"           << fs_error.path1()                     << "\n"
                    << "path2():"           << fs_error.path2()                     << "\n"
                    << "code().value():   " << fs_error.code().value()              << "\n"
                    << "code().message(): " << fs_error.code().message()            << "\n"
                    << "code().category():" << fs_error.code().category().name()    << "\n";
        }

        if (!is_directory(path)) {
            parse_Helper(path);
        }
        else {
            for (auto &dir_Entry: fs::recursive_directory_iterator(path)) {
                parse_Dir_Entry(dir_Entry);
            }
        }
    }


    std::cout << "\nCore Keys\n\n";

    for (auto& dc : core_Set) {
        std::cout << dc << '\n';
    }

    std::cout << "\nRemaining\n\n";

    for (auto& dc : remaining_Keys) {
        std::cout << dc << '\n';
    }
//    output_File(remaining_Keys);
//
//    edit_File();

    return 0;
}

