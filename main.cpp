#include <iostream>
#include <vector>
#include <filesystem>

#include <rapidjson/document.h>
#include <cpr/cpr.h>

namespace  fs = std::filesystem;
namespace  rj = rapidjson;

std::vector<std::string> meta_keys;

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
        meta_keys.emplace_back(key);
//        std::cout << key << "\n";
        return true;
    }
    bool EndObject(rj::SizeType memberCount) {return true;}
    bool StartArray() {return true;}
    bool EndArray(rj::SizeType element_count) {return true;}

};

cpr::Response put_request(std::vector<char>& buff, fs::path& path)
{
    fs::path fileName = path.filename();
    auto url {cpr::Url{"http://localhost:9998/rmeta/form/text"}};
    auto buff_size {cpr::Buffer{buff.begin(), buff.end(), fileName}};
    // Set maxEmbeddedResources to 0 to Return metadata for the parent file/container (e.g. zip) only.
    auto header {cpr::Header {{"maxEmbeddedResources", "0",},
                                      {"X-Tika-OCRskipOcr", "true"},
                                      {"accept", "application/json"}}};

    cpr::Response response {cpr::Post(url, header, cpr::Multipart{{fileName, buff_size}})};

   if (response.status_code != 200)
   {
        std::cout << "Failed to processes request. Error code: " << response.status_code << " . Check Tika Server.\n"
        << "Path: " << path.relative_path() << "\n";
   }
    return response;
}

void parse_json(cpr::Response& response)
{
    std::string text {response.text};
    rj::StringStream str_stream(text.c_str());
    rj::Reader reader;
    type_handler handler;

    reader.Parse(str_stream, handler);
}

std::vector<char> file_buffer (fs::path& path)
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

void constructor(fs::path& path)
{
    std::vector<char> buff {file_buffer(path)};
    cpr::Response response {put_request(buff, path)};

    parse_json(response);
}

void unique_vector(std::vector<std::string>& vector)
{
    std::sort(vector.begin(), vector.end());
    vector.erase(std::unique(vector.begin(), vector.end()), vector.end());
}

void writer(std::vector<std::string>& vector)
{
    std::sort(vector.begin(), vector.end());

    std::string path {"/Users/xawatso/Desktop/unique_keys_test.json"};
    std::ofstream file(path);

    for (const auto& i : vector)
    {
        file << i << "\n";
    }
}


int main(int argc,char* argv[]) {

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
            constructor(file);
        }
        else
        {
            for (const auto& dir_entry: fs::recursive_directory_iterator(path))
            {
                if (!dir_entry.is_regular_file() || dir_entry.is_symlink())
                {
                    continue;
                }
                constructor((fs::path& )dir_entry.path());
            }
        }
    }

    unique_vector(meta_keys);
    std::cout << "Items are now unique." << "\n";

    writer(meta_keys);
    std::cout << "File has been written." << "\n";
    return 0;
}
