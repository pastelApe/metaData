#include <iostream>
#include <filesystem>
#include <vector>
#include <set>
#include <cstring>


#include <rapidjson/document.h>
#include <cpr/cpr.h>

namespace  fs = std::filesystem;
namespace  rj = rapidjson;

std::set<std::string> dublin_Core_Keys;

struct type_handler : public rj::BaseReaderHandler<rj::UTF8<>, type_handler>
{

    bool Null  () {std::cout << "Null()" << "\n"; return true;}
    bool Bool  (bool b) {std::cout << "Bool(" << std::boolalpha << b << ")" << "\n"; return true;}
    bool Int   (int i) {std::cout << "Int(" << i << ")" << "\n"; return true;}
    bool Uint  (unsigned u) {std::cout << "Uint(" << u << ")" << "\n"; return true;}
    bool Int64 (int64_t i) {std::cout << "Int64(" << i << ")" << "\n"; return true;}
    bool Uint64(uint64_t u) {std::cout << "Uint64(" << u << ")" << "\n"; return true;}
    bool Double(double d) {std::cout << "Double(" << d << ")" << "\n"; return true;}
    bool String(const char* str, rj::SizeType length, bool copy) {return true;}
    bool StartObject() {return true;}
    bool Key(const char* key, rj::SizeType length, bool copy)
    {
        const char *sub_String {"Unknown"};
        if (strstr(key, sub_String) != nullptr)
        {
            std::cout << "Unknown skipped.\n";
        }
        else
        {
            dublin_Core_Keys.emplace(key);
        }

        return true;
    }
    bool EndObject(rj::SizeType memberCount) {return true;}
    bool StartArray() {return true;}
    bool EndArray(rj::SizeType element_count) {return true;}

};

cpr::Response put_request(std::vector<char>& buff, fs::path& path)
{
    fs::path file_Name = path.filename();
    auto url {cpr::Url{"http://localhost:9998/rmeta/form/text"}};
    auto buff_Size {cpr::Buffer{buff.begin(), buff.end(), file_Name}};
    // Set maxEmbeddedResources to 0 to Return metadata for the parent file/container (e.g. zip) only.
    auto header {cpr::Header {{"maxEmbeddedResources", "0",},
                              {"X-Tika-OCRskipOcr", "true"},
                              {"accept", "application/json"}}};

    cpr::Response response {cpr::Post(url, header, cpr::Multipart{{file_Name, buff_Size}})};

    if (response.status_code != 200)
    {
        std::cout << "Failed to processes request. Error code: " << response.status_code << " . Check Tika Server.\n"
                  << "Path: " << path.relative_path() << "\n";
    }
    return response;
}

void json_parser(cpr::Response& response)
{
    std::string text {response.text};
    rj::StringStream str_Stream(text.c_str());
    rj::Reader reader;
    type_handler handler;

    reader.Parse(str_Stream, handler);
}

std::vector<char> file_Buffer(fs::path& path)
{
    //Open the file.
    std::ifstream stream {path.string()};

    if (!stream.is_open())
        std::cerr << "Could not open file for reading!" << "\n";

    std::vector<char> buff {
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
    return buff;
}

void parse_Json(fs::path& path)
{
    std::vector<char> buff {file_Buffer(path)};
    cpr::Response response {put_request(buff, path)};

    json_parser(response);
}
void writer(std::set<std::string>& set)
{
    std::string path {"/home/xavier/Desktop/unique_keys_test.json"};
    std::ofstream file(path);

    for (auto& i : set)
    {
        file << i << "\n";
    }
}


int main(int argc,char* argv[])
{

    if (argc != 2)
    {
        std::cout << "Please enter a valid path." << "\n";
        return 1;
    }
    else
    {
        fs::path path {argv[1]};
        try
        {
            fs::exists(path);
        }
        catch (fs::filesystem_error const& fs_error)
        {
            std::cout
                    << "what(): "           << fs_error.what()                      << "\n"
                    << "path1():"           << fs_error.path1()                     << "\n"
                    << "path2():"           << fs_error.path2()                     << "\n"
                    << "code().value():   " << fs_error.code().value()              << "\n"
                    << "code().message(): " << fs_error.code().message()            << "\n"
                    << "code().category():" << fs_error.code().category().name()    << "\n";
        }

        if (!is_directory(path))
        {
            fs::path& file {path};
            parse_Json(file);
        }
        else
        {
            for (const auto& dir_entry: fs::recursive_directory_iterator(path))
            {
                if (!dir_entry.is_regular_file() || dir_entry.is_symlink())
                {
                    continue;
                }
                parse_Json((fs::path& )dir_entry.path());
            }
        }
    }

    writer(dublin_Core_Keys);
    return 0;
}