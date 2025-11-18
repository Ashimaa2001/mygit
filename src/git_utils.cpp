#include "git_utils.h"

#include <bits/stdc++.h>
#include <openssl/sha.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <filesystem>
#include <ctime>

const std::string REPO_DIR = ".mygit";

using namespace std;

string to_hex(const unsigned char *hash, size_t len) {
    static const char *hex = "0123456789abcdef";
    string s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = hash[i];
        s.push_back(hex[c >> 4]);
        s.push_back(hex[c & 0xf]);
    }
    return s;
}

string sha1_hex(const string &data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char*)data.data(), data.size(), hash);
    return to_hex(hash, SHA_DIGEST_LENGTH);
}

string compress_data(const string &data) {
    uLongf compressed_size = compressBound(data.size());
    unsigned char *compressed = new unsigned char[compressed_size];
    int ret = compress2(compressed, &compressed_size, (const unsigned char *)data.data(), data.size(), Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
        delete[] compressed;
        return string();  // compression failed
    }
    string result((char *)compressed, compressed_size);
    delete[] compressed;
    return result;
}

string decompress_data(const string &data) {

    uLongf uncompressed_size = data.size() * 10; 
    for (int attempt = 0; attempt < 5; ++attempt) {
        unsigned char *uncompressed = new unsigned char[uncompressed_size];
        int ret = uncompress(uncompressed, &uncompressed_size, (const unsigned char *)data.data(), data.size());
        if (ret == Z_OK) {
            string result((char *)uncompressed, uncompressed_size);
            delete[] uncompressed;
            return result;
        }
        delete[] uncompressed;
        if (ret != Z_BUF_ERROR) return string();  
        uncompressed_size *= 2; 
    }
    return string(); 
}

bool ensure_dir(const string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) return true;
        return false;
    }
    
    size_t pos = 0;
    while (true) {
        pos = path.find('/', pos + 1);
        string sub = path.substr(0, pos == string::npos ? path.size() : pos);
        if (sub.size() == 0) continue;
        if (stat(sub.c_str(), &st) != 0) {
            if (mkdir(sub.c_str(), 0755) != 0) {
                return false;
            }
        }
        if (pos == string::npos) break;
    }
    return true;
}

string read_file(const string &path) {
    ifstream ifs(path, ios::binary);
    if (!ifs) return string();
    ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

bool write_file(const string &path, const string &data) {
    size_t p = path.rfind('/');
    if (p != string::npos) {
        string parent = path.substr(0, p);
        ensure_dir(parent);
    }
    ofstream ofs(path, ios::binary);
    if (!ofs) return false;
    ofs.write(data.data(), data.size());
    return true;
}

string object_path_for_sha(const string &sha) {
    string dir = REPO_DIR + "/objects/" + sha.substr(0, 2);
    string file = sha.substr(2);
    return dir + "/" + file;
}

string build_object_buffer(const string &type, const string &data) {
    string hdr = type + " " + to_string(data.size()) + '\0';
    return hdr + data;
}

string hash_object_from_data(const string &type, const string &data, bool write) {
    string buf = build_object_buffer(type, data);
    string sha = sha1_hex(buf);
    if (write) {
        string dir = REPO_DIR + "/objects/" + sha.substr(0,2);
        ensure_dir(dir);
        string path = object_path_for_sha(sha);
    
        string compressed = compress_data(buf);
        if (compressed.empty()) {
            cerr << "error: failed to compress object\n";
            return string();
        }
        write_file(path, compressed);
    }
    return sha;
}

bool repo_exists() {
    struct stat st;
    return stat(REPO_DIR.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

string write_tree_recursive(const string &path) {
    vector<tuple<string,string,string>> entries;
    DIR *dp = opendir(path.c_str());
    if (!dp) return string();
    struct dirent *de;
    while ((de = readdir(dp)) != nullptr) {
        string name = de->d_name;
        if (name == "." || name == "..") continue;
        // skip hidden directories (starting with .)
        if (name[0] == '.') continue;
        string full = path == "." ? name : path + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            string child_sha = write_tree_recursive(full);
            if (child_sha.empty()) continue;
            entries.emplace_back(name, string("40000"), child_sha);
        } else if (S_ISREG(st.st_mode)) {
            string data = read_file(full);
            string sha = hash_object_from_data("blob", data, true);
            entries.emplace_back(name, string("100644"), sha);
        }
    }
    closedir(dp);
    sort(entries.begin(), entries.end(), [](const auto &a, const auto &b){ return get<0>(a) < get<0>(b); });
    ostringstream ss;
    for (auto &e: entries) {
        ss << get<1>(e) << " " << get<0>(e) << '\t' << get<2>(e) << '\n';
    }
    string tree_data = ss.str();
    string tree_sha = hash_object_from_data("tree", tree_data, true);
    return tree_sha;
}
