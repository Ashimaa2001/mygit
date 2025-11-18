#include "commands.h"
#include "git_utils.h"

#include <bits/stdc++.h>
#include <unistd.h>
#include <filesystem>

using namespace std;

int cmd_init() {
    if (repo_exists()) {
        cerr << "Repository already exists in " << REPO_DIR << "\n";
        return 1;
    }
    if (!ensure_dir(REPO_DIR)) {
        cerr << "failed to create " << REPO_DIR << "\n";
        return 1;
    }
    ensure_dir(REPO_DIR + "/objects");
    ensure_dir(REPO_DIR + "/refs/heads");
    write_file(REPO_DIR + "/HEAD", string("ref: refs/heads/master\n"));
    cout << "Initialized empty mygit repository in " << REPO_DIR << "\n";
    return 0;
}

int cmd_hash_object(const vector<string> &args) {
    bool write = false;
    string file;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-w") { write = true; }
        else file = args[i];
    }
    if (file.empty()) {
        cerr << "usage: mygit hash-object [-w] <file>\n";
        return 1;
    }
    string data = read_file(file);
    if (data.empty() && access(file.c_str(), F_OK) != 0) {
        cerr << "error: cannot read file: " << file << "\n";
        return 1;
    }
    string sha = hash_object_from_data("blob", data, write);
    cout << sha << "\n";
    return 0;
}