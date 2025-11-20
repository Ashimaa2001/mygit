#ifndef GIT_UTILS_H
#define GIT_UTILS_H

#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <map>


extern const std::string REPO_DIR;

using namespace std;

string to_hex(const unsigned char *hash, size_t len);
string sha1_hex(const string &data);

bool ensure_dir(const string &path);
string read_file(const string &path);
bool write_file(const string &path, const string &data);

string compress_data(const string &data);  

string object_path_for_sha(const string &sha);
string build_object_buffer(const string &type, const string &data);
string hash_object_from_data(const string &type, const string &data, bool write);

pair<string, string> read_object(const string &sha);
bool repo_exists();

string write_tree_recursive(const string &path);
vector<tuple<string,string,string>> read_index();
string build_tree_from_index_entries(const vector<tuple<string,string,string>> &entries);
bool write_index(const vector<tuple<string,string,string>> &entries);

string build_tree_from_index();  
bool add_files_to_index(const vector<string> &files);

string read_head();  
string read_ref(const string &ref);
bool write_ref(const string &ref, const string &sha);
string create_commit_object(const string &tree_sha, const string &message, const string &parent_sha);

struct CommitInfo {
    string parent;
    string author;
    string message;
};

CommitInfo parse_commit(const string &commit_data);


void collect_tree_files(const string &tree_sha, const string &prefix, unordered_map<string, string> &tree_files);

struct CheckoutChange {
    string action;  // "restore", "delete"
    string path;
};
vector<CheckoutChange> plan_checkout(const string &target_tree_sha, const string &current_commit_sha);

#endif // GIT_UTILS_H
