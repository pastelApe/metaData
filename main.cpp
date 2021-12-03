#include <filesystem>
#include <iostream>
#include <vector>
#include <set>

#include <rapidjson/document.h>
#include "rapidjson/error/en.h"
#include <cpr/cpr.h>

namespace  fs = std::filesystem;
namespace  rj = rapidjson;

std::set<std::string> remaining_Keys;
std::set<std::string> DublinCore_Keys;
std::set<std::string> TikaCore_Keys;
std::set<std::string> FlatFile_Keys;

std::vector<std::string> file_Content;

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
    auto header {cpr::Header {{"maxEmbeddedResources", "0",},
                              {"X-Tika-OCRskipOcr", "true"},
                              {"accept", "application/json"}}};

    cpr::Response response {cpr::Post(url, header, cpr::Multipart{{file_Name, buff_Size}})};

    if (response.status_code != 200) 
    {
        std::cout << "Failed to processes request. Error code: " 
                  << response.status_code << ". \n"
                  << "Path: " << path.relative_path() << "\n";
    }
    return response;
}

void create_Sets(rj::GenericMember<rj::UTF8<char>, rj::MemoryPoolAllocator<rj::CrtAllocator>> &key) {
    std::string key_Name = key.name.GetString();
    //Create unique set of keys.
    remaining_Keys.emplace(key_Name);
    //Make case-insensitive for comparison.
    std::transform(key_Name.begin(), key_Name.end(), key_Name.begin(), ::tolower);
    //Add Dublin Core metadata fields
    std::vector<std::string> dc_Keys {"dc:", "dc.", "dc_", "dcterms", "dctm"};
    for (auto& term: dc_Keys) {
        std::cout << "\n\n DC terms to search for: " << term << "\n\n";
        std::size_t found = key_Name.find(term);
        if (found != std::string::npos) {
            DublinCore_Keys.emplace(key_Name);
        }
    }
}

void parse_Object(rj::GenericValue<rj::UTF8<char>, rj::MemoryPoolAllocator<rj::CrtAllocator>>& member) {
    if (!member.IsObject()) {
        std::cout << "JSON is not an object. Failed to parse. Type is: " << strerror(member.GetType());
    }
    
    for (auto& key : member.GetObject())
    {
        create_Sets(key);

        //Logic to print JSON as Key : Value pairs with proper formatting for values that are arrays.
        std::cout << key.name.GetString() << ": ";

        if (key.value.IsArray())
        {
            // Subtract 1 to remove comma from printing after last value in array.
            unsigned int array_Size = key.value.Size() - 1;

            std::cout  << "[ ";

            for (auto& value : key.value.GetArray())
            {
                std::cout << value.GetString();

                if (array_Size > 0)
                {
                    --array_Size;
                    std::cout << ", ";
                }
            }
            std::cout  << " ]\n";
        }
        else
        {
            std::cout << key.value.GetString() << '\n';
        }
    }
}

void parse_Array(rj::Document& doc) {
    for (auto &member: doc.GetArray()) {
        if (member.IsObject()) {
            parse_Object(member);
        }
    }
}

void json_Parser(cpr::Response& response) 
{
    //Validate json schema.
    rj::Document document;
    if (document.HasParseError()){
        std::cout << "File is not valid JSON. Error: " << GetParseError_En(document.GetParseError());
    }
    
    std::string json {response.text};
    const char* c_Json(json.c_str());

    if(!document.HasParseError()) {
        document.Parse(c_Json);
    }

    if(document.IsArray()) {
        parse_Array(document);
    } else if (document.IsObject()) {
        parse_Object(document);
    }
}


void parse_Helper(fs::path& path) {
    std::vector<char> buff {file_Buffer(path)};
    cpr::Response response {put_Request(buff, path)};

    json_Parser(response);
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
    std::string path {"/home/xavier/Desktop/unique_keys.json"};
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


    std::cout << "\nFrom DC keys\n\n";
    
    for (auto& dc : DublinCore_Keys) {
        std::cout << dc << '\n';
    }

    std::cout << "\nFrom all keys\n\n";

    for (auto& dc : remaining_Keys) {
        std::cout << dc << '\n';
    }
//    output_File(remaining_Keys);
//
//    edit_File();

    return 0;
}

