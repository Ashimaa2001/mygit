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

pair<string,string> read_object(const string &sha) {
    string path = object_path_for_sha(sha);
    string compressed = read_file(path);
    if (compressed.empty()) return {"",""};
    
    string buf = decompress_data(compressed);
    if (buf.empty()) return {"",""};
    size_t pos = buf.find('\0');
    if (pos == string::npos) return {"",""};
    string hdr = buf.substr(0, pos);
    string data = buf.substr(pos + 1);
    size_t sp = hdr.find(' ');
    if (sp == string::npos) return {"",""};
    string type = hdr.substr(0, sp);
    return {type, data};
}

bool repo_exists() {
    struct stat st;
    return stat(REPO_DIR.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

vector<tuple<string,string,string>> read_index() {
    vector<tuple<string,string,string>> entries;
    string index_path = REPO_DIR + "/index";
    string content = read_file(index_path);
    if (content.empty()) return entries;
    
    istringstream iss(content);
    string line;
    while (getline(iss, line)) {
        if (line.empty()) continue;
        size_t tab_pos = line.rfind('\t');
        if (tab_pos == string::npos) continue;
        
        string mode_path = line.substr(0, tab_pos);
        string sha = line.substr(tab_pos + 1);
        
        size_t space_pos = mode_path.find(' ');
        if (space_pos == string::npos) continue;
        
        string mode = mode_path.substr(0, space_pos);
        string filepath = mode_path.substr(space_pos + 1);
        
        entries.emplace_back(filepath, mode, sha);
    }
    return entries;
}

string build_tree_from_index_entries(const vector<tuple<string,string,string>> &entries) {
    if (entries.empty()) return string();
    
    map<string, vector<tuple<string,string,string>>> dir_entries;
    
    for (const auto &entry : entries) {
        string filepath = get<0>(entry);
        string mode = get<1>(entry);
        string sha = get<2>(entry);
        
        size_t last_slash = filepath.rfind('/');
        string dir = (last_slash == string::npos) ? "" : filepath.substr(0, last_slash);
        string filename = (last_slash == string::npos) ? filepath : filepath.substr(last_slash + 1);
        
        dir_entries[dir].emplace_back(filename, mode, sha);
    }
    
    map<string, string> dir_tree_shas;
    
    vector<string> dirs;
    for (auto &kv : dir_entries) {
        dirs.push_back(kv.first);
    }
    sort(dirs.begin(), dirs.end(), [](const string &a, const string &b) {
        return count(a.begin(), a.end(), '/') > count(b.begin(), b.end(), '/');
    });
    
    for (const string &dir : dirs) {
        vector<tuple<string,string,string>> tree_entries = dir_entries[dir];
        
        for (auto &entry : tree_entries) {
            string name = get<0>(entry);
            string mode = get<1>(entry);
            string sha = get<2>(entry);
            
            string child_dir = dir.empty() ? name : dir + "/" + name;
            if (dir_tree_shas.find(child_dir) != dir_tree_shas.end()) {
                get<1>(entry) = "40000";
                get<2>(entry) = dir_tree_shas[child_dir];
            }
        }
        
        sort(tree_entries.begin(), tree_entries.end(),
            [](const auto &a, const auto &b) { return get<0>(a) < get<0>(b); });
        
        ostringstream ss;
        for (auto &e : tree_entries) {
            ss << get<1>(e) << " " << get<0>(e) << '\t' << get<2>(e) << '\n';
        }
        string tree_data = ss.str();
        string tree_sha = hash_object_from_data("tree", tree_data, true);
        dir_tree_shas[dir] = tree_sha;
    }
    
    return dir_tree_shas[""];
}

string write_tree_recursive(const string &path) {
    vector<tuple<string,string,string>> entries = read_index();
    return build_tree_from_index_entries(entries);
}

bool write_index(const vector<tuple<string,string,string>> &entries) {
    ostringstream ss;
    for (auto &e: entries) {
        ss << get<1>(e) << " " << get<0>(e) << "\t" << get<2>(e) << "\n";
    }
    return write_file(REPO_DIR + "/index", ss.str());
}

bool add_files_to_index(const vector<string> &files) {
    vector<tuple<string,string,string>> index = read_index();
    
    unordered_map<string,string> m;
    for (auto &e: index) m[get<0>(e)] = get<2>(e);

    for (const string &f: files) {
        string data;
        try {
            data = read_file(f);
        } catch (...) { data = string(); }
        if (data.empty() && access(f.c_str(), F_OK) != 0) {
            continue;
        }
        string sha = hash_object_from_data("blob", data, true);
        m[f] = sha;
    }

    vector<tuple<string,string,string>> out;
    out.reserve(m.size());
    for (auto &kv: m) out.emplace_back(kv.first, "100644", kv.second);
    
    sort(out.begin(), out.end(), [](auto &a, auto &b){ return get<0>(a) < get<0>(b); });
    return write_index(out);
}


string build_tree_from_index() {
    vector<tuple<string,string,string>> entries = read_index();
    return build_tree_from_index_entries(entries);
}


string read_head() {
    string head_file = REPO_DIR + "/HEAD";
    string content = read_file(head_file);
    
    if (content.find("ref:") != string::npos) {
        // extract ref
        size_t pos = content.find("refs/");
        if (pos != string::npos) {
            string ref = content.substr(pos);
            while (!ref.empty() && (ref.back() == '\n' || ref.back() == '\r')) ref.pop_back();
            return ref;
        }
    }
    while (!content.empty() && (content.back() == '\n' || content.back() == '\r')) content.pop_back();
    return content;
}

string read_ref(const string &ref) {
    string path = REPO_DIR + "/" + ref;
    string content = read_file(path);
    while (!content.empty() && (content.back() == '\n' || content.back() == '\r')) content.pop_back();
    return content;
}

bool write_ref(const string &ref, const string &sha) {
    string path = REPO_DIR + "/" + ref;
    return write_file(path, sha + "\n");
}

string create_commit_object(const string &tree_sha, const string &message, const string &parent_sha) {
    time_t now = time(nullptr);
    ostringstream ss;
    ss << "tree " << tree_sha << "\n";
    if (!parent_sha.empty()) {
        ss << "parent " << parent_sha << "\n";
    }
    ss << "author mygit <mygit@example.com> " << now << " +0000\n";
    ss << "committer mygit <mygit@example.com> " << now << " +0000\n";
    ss << "\n" << message;
    if (!message.empty() && message.back() != '\n') {
        ss << "\n";
    }
    
    string commit_data = ss.str();
    return hash_object_from_data("commit", commit_data, true);
}

CommitInfo parse_commit(const string &commit_data) {
    CommitInfo info;
    istringstream ss(commit_data);
    string line;
    bool reading_message = false;
    while (getline(ss, line)) {
        if (reading_message) {
            info.message += line + "\n";
        } else if (line.empty()) {
            reading_message = true;
        } else if (line.find("parent ") == 0) {
            info.parent = line.substr(7);  
        } else if (line.find("author ") == 0) {
            info.author = line.substr(7);
        }
    }
    if (!info.message.empty() && info.message.back() == '\n') {
        info.message.pop_back();
    }
    return info;
}

// Extract tree SHA from a commit object
static string get_tree_sha_from_commit(const string &commit_sha) {
    auto p = read_object(commit_sha);
    if (p.first.empty() || p.first != "commit") return string();
    
    istringstream ss(p.second);
    string line;
    while (getline(ss, line)) {
        if (line.find("tree ") == 0) {
            return line.substr(5);  
        }
    }
    return string();
}

// Helper to recursively collect files from tree
void collect_tree_files(const string &tree_sha, const string &prefix, unordered_map<string, string> &tree_files) {
    auto p = read_object(tree_sha);
    if (p.first.empty() || p.first != "tree") return;
    
    istringstream ss(p.second);
    string line;
    while (getline(ss, line)) {
        if (line.empty()) continue;
        size_t tab = line.find('\t');
        if (tab == string::npos) continue;
        string left = line.substr(0, tab);
        string entry_sha = line.substr(tab + 1);
        
        size_t sp = left.find(' ');
        if (sp == string::npos) continue;
        string mode = left.substr(0, sp);
        string name = left.substr(sp + 1);
        
        string full_path = prefix.empty() ? name : prefix + "/" + name;
        
        if (mode == "40000") {
            collect_tree_files(entry_sha, full_path, tree_files);
        } else {
            tree_files[full_path] = entry_sha;
        }
    }
}

// Helper to collect current working files (excluding hidden)
static void collect_current_files(const string &path, const string &prefix, unordered_set<string> &current_files) {
    filesystem::path fp(path);
    try {
        for (auto &entry: filesystem::directory_iterator(fp)) {
            string filename = entry.path().filename().string();
            if (filename[0] == '.') continue;  
            
            string full_path = prefix.empty() ? filename : prefix + "/" + filename;
            if (filesystem::is_directory(entry.path())) {
                collect_current_files(entry.path().string(), full_path, current_files);
            } else if (filesystem::is_regular_file(entry.path())) {
                current_files.insert(full_path);
            }
        }
    } catch (...) {}
}

vector<CheckoutChange> plan_checkout(const string &target_tree_sha, const string &current_commit_sha) {
    vector<CheckoutChange> changes;
    
    unordered_map<string, string> current_tree_files;
    if (!current_commit_sha.empty()) {
        string current_tree_sha = get_tree_sha_from_commit(current_commit_sha);
        if (!current_tree_sha.empty()) {
            collect_tree_files(current_tree_sha, "", current_tree_files);
        }
    }
    
    unordered_map<string, string> target_tree_files;
    collect_tree_files(target_tree_sha, "", target_tree_files);
    
    for (auto &kv: target_tree_files) {
        const string &path = kv.first;
        const string &target_sha = kv.second;
        
        auto it = current_tree_files.find(path);
        if (it == current_tree_files.end()) {
            changes.push_back({"restore", path});
        } else if (it->second != target_sha) {
            // File exists but with different content (different SHA)
            changes.push_back({"restore", path});
        }
    }
    
    for (auto &kv: current_tree_files) {
        const string &path = kv.first;
        if (target_tree_files.find(path) == target_tree_files.end()) {
            changes.push_back({"delete", path});
        }
    }
    
    return changes;
}

vector<CheckoutChange> plan_checkout(const string &tree_sha, bool dry_run) {
    vector<CheckoutChange> changes;
    
    unordered_map<string, string> tree_files;
    collect_tree_files(tree_sha, "", tree_files);
    
    unordered_set<string> current_files;
    collect_current_files(".", "", current_files);
    
    for (auto &kv: tree_files) {
        changes.push_back({"restore", kv.first});
    }
    
    for (auto &f: current_files) {
        if (tree_files.find(f) == tree_files.end()) {
            changes.push_back({"delete", f});
        }
    }
    
    return changes;
}
