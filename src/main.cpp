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
#include "rapidjson/prettywriter.h"



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
        std::map<std::string, boost::recursive_variant_>>::type variant_Value_Type;

/*--------------------------------------------------------------------------------------------------------------------*/

struct Type_Handler : public rj::BaseReaderHandler<rj::UTF8<>, Type_Handler> {
private:
    variant_Value_Type _props;
public:
    // && is normally only used to declare a parameter of a function. And it only takes an r-value expression.
    variant_Value_Type&& Get_Value() {
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

class Get_String_Visitor
    : public boost::static_visitor<std::string>
{
public:
    std::string operator() (rj::Value& value) {
        std::ostringstream s;
        switch (value.GetType()) {
            case rj::kNullType :
                s << "Null";
                break;
            case rj::kFalseType :
            case rj::kTrueType :
                s << value.GetBool();
                break;
            case rj::kNumberType :
                if (value.IsUint64()) {
                    s << value.GetUint64();
                } 
                else if (value.IsInt64()) {
                    s << value.GetInt64();
                } 
                else if (value.IsDouble()) {
                    s << value.GetDouble();
                }
                break;
            case rj::kStringType :
                s << value.GetString();
                break;
            case rj::kObjectType :
            case rj::kArrayType :
                break;
        }
        std::string ss {s.str()};
        boost::trim(ss);
        return ss;
    }
};

/*--------------------------------------------------------------------------------------------------------------------*/

//Value_Handler is a recursive function. Checks all values including child objects and arrays.
variant_Value_Type Value_Handler(rj::Value& value) {
    Get_String_Visitor print_String;

    if (value.IsObject()) {
        std::map<std::string, variant_Value_Type> object_Map;


        for (auto& [object_key, object_value] : value.GetObject()) {
            object_Map.emplace(object_key.GetString(), Value_Handler(object_value));

        }

        return object_Map;
    }
    else if (value.IsArray()) {
        // To remove comma from printing after last value in array, subtract 1.
        unsigned int array_Size = value.Size() - 1;
        std::vector<variant_Value_Type> value_Array;

        for (auto& array_value: value.GetArray()) {
            value_Array.push_back(Value_Handler(array_value));
        }

        return value_Array;
    }
    else {
        Type_Handler value_Handler;
        value.Accept(value_Handler);
        return value_Handler.Get_Value();
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/

std::map<std::string ,variant_Value_Type> Process_Document (rj::Document& document) {
    std::map<std::string ,variant_Value_Type> meta_Data;
    Get_String_Visitor print_String;

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

std::vector<char> File_Buffer (fs::path& path) {
    //Open the file.
    std::ifstream stream {path.string()};

    if (!stream.is_open()) {
        std::cerr << "Could not open file for reading!" << "\n";
        std::exit(1);
    }

    std::vector<char> buffer {
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()
    };

    return buffer;
}

/*--------------------------------------------------------------------------------------------------------------------*/

cpr::Response Post_Request(std::vector<char>& buffer, fs::path& path) {
    fs::path file_Name = path.filename();
    cpr::Url url {cpr::Url{"http://localhost:9998/rmeta/form/text"}};
    cpr::Buffer buffer_Size {cpr::Buffer{buffer.begin(), buffer.end(), file_Name}};

    // Set maxEmbeddedResources to 0 for parent object metadata only.
    auto header {cpr::Header{ {"maxEmbeddedResources", "0"},
                              {"X-Tika-OCRskipOcr", "true"},
                              {"accept", "application/json"}}};

    cpr::Response response {cpr::Post(url, header, cpr::Multipart{{file_Name, buffer_Size}})};

    if (response.status_code != 200) {
        std::cout << "Failed to processes request. Error code: "
                  << response.status_code << ". \n"
                  << "Failed Path: " << path.relative_path() << "\n";
    }

    return response;
}

/*--------------------------------------------------------------------------------------------------------------------*/


std::map<std::string, variant_Value_Type> Path_Handler(fs::path& path) {
    std::map<std::string, variant_Value_Type> map {};
    if (is_directory(path)) {
        for (auto &directory_entry: fs::recursive_directory_iterator(path)) {
            Path_Handler((fs::path &) directory_entry);
        }
    }
    else if (is_regular_file(path)) {
        std::vector<char> buffer{File_Buffer(path)};
        std::string response { Post_Request(buffer, path).text };
        rj::Document document;

        document.Parse(response.c_str());
        map = Process_Document(document);

        rj::StringBuffer rj_Buffer;
        rj::PrettyWriter<rj::StringBuffer> writer (rj_Buffer);
        document.Accept(writer.SetFormatOptions(rapidjson::kFormatSingleLineArray));
        const char* output = rj_Buffer.GetString();
        std::cout << output << std::endl;
    }
    return map;
}

/*--------------------------------------------------------------------------------------------------------------------*/

void Create_File (std::string& path, std::set<std::string>& master_set) {
    std::set<std::string> core_Set;

    for (auto& key : master_set) {
        core_Set.emplace(key);
    }
    std::ofstream file(path);

    for (auto& key: core_Set) {
        file << key << std::endl;
    }

    file.close();

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
        std::vector<std::map<std::string, variant_Value_Type>> meta_Data {};
        meta_Data.emplace_back(Path_Handler(path));

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

