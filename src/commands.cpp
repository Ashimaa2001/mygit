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

int cmd_cat_file(const vector<string> &args) {
    if (!repo_exists()) {
        cerr << "fatal: not a mygit repository (or any parent up to mount point)\n";
        return 1;
    }
    if (args.size() < 2) {
        cerr << "usage: mygit cat-file <flag> <object_sha>\n";
        cerr << "  -p : print contents\n";
        cerr << "  -t : print type\n";
        cerr << "  -s : print size in bytes\n";
        return 1;
    }
    string flag = args[0];
    string sha = args[1];
    auto p = read_object(sha);
    if (p.first.empty()) {
        cerr << "error: object not found: " << sha << "\n";
        return 1;
    }
    if (flag == "-p") {
        cout << p.second;
        if (p.second.empty() || p.second.back() != '\n') cout << '\n';
    } else if (flag == "-t") {
        cout << p.first << "\n";
    } else if (flag == "-s") {
        cout << p.second.size() << "\n";
    } else {
        cerr << "unknown flag: " << flag << "\n";
        return 1;
    }
    return 0;
}

int cmd_write_tree() {
    if (!repo_exists()) {
        cerr << "fatal: not a mygit repository\n";
        return 1;
    }
    string sha = write_tree_recursive(".");
    if (sha.empty()) {
        cerr << "error: failed to write tree\n";
        return 1;
    }
    cout << sha << "\n";
    return 0;
}

int cmd_ls_tree(const vector<string> &args) {
    if (!repo_exists()) {
        cerr << "fatal: not a mygit repository\n";
        return 1;
    }
    bool name_only = false;
    string sha;
    if (args.size() == 1) {
        sha = args[0];
    } else if (args.size() == 2) {
        if (args[0] == "--name-only") {
            name_only = true;
            sha = args[1];
        } else {
            cerr << "usage: mygit ls-tree [--name-only] <tree_sha>\n";
            return 1;
        }
    } else {
        cerr << "usage: mygit ls-tree [--name-only] <tree_sha>\n";
        return 1;
    }
    auto p = read_object(sha);
    if (p.first.empty()) {
        cerr << "error: object not found: " << sha << "\n";
        return 1;
    }
    if (p.first != "tree") {
        cerr << "error: object is not a tree: " << sha << "\n";
        return 1;
    }

    istringstream ss(p.second);
    string line;
    while (getline(ss, line)) {
        if (line.empty()) continue;
        size_t tab = line.find('\t');
        string left = (tab == string::npos) ? line : line.substr(0, tab);
        string entry_sha = (tab == string::npos) ? string() : line.substr(tab + 1);

        size_t sp = left.find(' ');
        string mode, name;
        if (sp != string::npos) {
            mode = left.substr(0, sp);
            name = left.substr(sp + 1);
        } else {
            name = left;
        }
        if (name_only) cout << name << "\n";
        else cout << mode << " " << name << "\t" << entry_sha << "\n";
    }
    return 0;
}