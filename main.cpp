#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>

#include <rapidjson/document.h>
#include <cpr/cpr.h>

namespace  fs = std::filesystem;
namespace  rj = rapidjson;

std::set<std::string> meta_Keys;

std::vector<char> file_Buffer(fs::path& path) {
    //Open the file.
    std::ifstream stream {path.string()};

    if (!stream.is_open()) {
        std::cerr << "Could not open file for reading!" << "\n";
        std::exit(1);
    }

    std::vector<char> buff {
        std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>()};
    return buff;
}


cpr::Response put_Request(std::vector<char>& buff, fs::path& path) {
    fs::path file_Name = path.filename();
    auto url {cpr::Url{"http://localhost:9998/rmeta/form/text"}};
    auto buff_Size {cpr::Buffer{buff.begin(), buff.end(), file_Name}};
    // Set maxEmbeddedResources to 0 to Return metadata for the parent file/container (e.g. zip) only.
    auto header {cpr::Header {{"maxEmbeddedResources", "0",},
                              {"X-Tika-OCRskipOcr", "true"},
                              {"accept", "application/json"}}};

    cpr::Response response {cpr::Post(url, header, cpr::Multipart{{file_Name, buff_Size}})};

    if (response.status_code != 200) {
        std::cout << "Failed to processes request. Error code: " << response.status_code << " . Check Tika Server.\n"
                  << "Path: " << path.relative_path() << "\n";
    }
    return response;
}


void json_Parser(cpr::Response& response) {
    rj::Document document;
    std::string json {response.text};
    const char* str_JSON(json.c_str());
    document.Parse(str_JSON);

    if (document.IsArray()) {
        for (auto& member : document[0].GetObject()) {
            std::string key = member.name.GetString();
            meta_Keys.emplace(key);
        }
    }
    else if (document.IsObject()) {
        for (auto& member : document.GetObject()) {
            std::string key = member.name.GetString();;
            meta_Keys.emplace(key);
        }
    }
}


void parse_Helper(fs::path& path) {
    std::vector<char> buff {file_Buffer(path)};
    cpr::Response response {put_Request(buff, path)};

    json_Parser(response);
}


void parse_Dir_Entry(const fs::directory_entry& dir_Entry) {
    if (!dir_Entry.is_regular_file()) {
        ;
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
        return 1;
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

    output_File(meta_Keys);

    edit_File();

    return 0;
}

