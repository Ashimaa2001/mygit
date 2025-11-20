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

static void collect_files_recursive(const filesystem::path &path, const filesystem::path &base, vector<string> &out) {
    for (auto &p: filesystem::directory_iterator(path)) {
        string filename = p.path().filename().string();
        if (filename[0] == '.') continue;
        
        if (filesystem::is_directory(p.path())) {
            collect_files_recursive(p.path(), base, out);
        } else if (filesystem::is_regular_file(p.path())) {
            filesystem::path rel = filesystem::relative(p.path(), base);
            out.push_back(rel.string());
        }
    }
}

int cmd_add(const vector<string> &args) {
    if (!repo_exists()) {
        cerr << "fatal: not a mygit repository\n";
        return 1;
    }
    vector<string> files;
    if (args.size() == 0) {
        cerr << "usage: mygit add . | <paths...>\n";
        return 1;
    }
    filesystem::path cwd = filesystem::current_path();
    
    if (args.size() == 1 && args[0] == ".") {
        collect_files_recursive(cwd, cwd, files);
    } else {
        for (auto &a: args) {
            filesystem::path p(a);
            if (!filesystem::exists(p)) {
                cerr << "warning: path does not exist: " << a << "\n";
                continue;
            }
            if (filesystem::is_directory(p)) {
                collect_files_recursive(p, cwd, files);
            } else if (filesystem::is_regular_file(p)) {
                filesystem::path rel = filesystem::relative(p, cwd);
                files.push_back(rel.string());
            }
        }
    }
    if (files.empty()) return 0;
    if (!add_files_to_index(files)) {
        cerr << "error: failed to add files to index\n";
        return 1;
    }
    return 0;
}

int cmd_commit(const vector<string> &args) {
    if (!repo_exists()) {
        cerr << "fatal: not a mygit repository\n";
        return 1;
    }
    string message;

    if (args.size() >= 2 && args[0] == "-m") {
        message = args[1];
    } else if (args.size() == 0) {

        cerr << "reading commit message from stdin (ctrl-D to end):\n";
        string line;
        while (getline(cin, line)) {
            message += line + "\n";
        }
    } else {
        cerr << "usage: mygit commit [-m <message>]\n";
        return 1;
    }
    if (message.empty()) {
        cerr << "error: commit message cannot be empty\n";
        return 1;
    }

    string tree_sha = build_tree_from_index();
    if (tree_sha.empty()) {
        cerr << "error: failed to write tree\n";
        return 1;
    }

    string head_ref = read_head();
    string parent_sha;
    if (!head_ref.empty() && head_ref.find("refs/") != string::npos) {
        parent_sha = read_ref(head_ref);
    }

    string commit_sha = create_commit_object(tree_sha, message, parent_sha);

    if (!write_ref(head_ref, commit_sha)) {
        cerr << "error: failed to write ref\n";
        return 1;
    }
    cout << commit_sha << "\n";
    return 0;
}

int cmd_log(const vector<string> &args) {
    if (!repo_exists()) {
        cerr << "fatal: not a mygit repository\n";
        return 1;
    }

    string head_ref = read_head();
    if (head_ref.empty()) {
        cerr << "fatal: no HEAD\n";
        return 1;
    }

    string current_sha = read_ref(head_ref);
    if (current_sha.empty()) {
        cerr << "fatal: no commits on this branch\n";
        return 1;
    }

    int count = 0;
    while (!current_sha.empty() && count < 100) {  
        auto p = read_object(current_sha);
        if (p.first.empty() || p.first != "commit") {
            break;
        }
        CommitInfo info = parse_commit(p.second);
        cout << "commit " << current_sha << "\n";
        cout << "Author: " << info.author << "\n";
        cout << "Message: " << info.message << "\n";
        cout << "\n";
        current_sha = info.parent;
        count++;
    }
    return 0;
}

int cmd_checkout(const vector<string> &args) {
    if (!repo_exists()) {
        cerr << "fatal: not a mygit repository\n";
        return 1;
    }
    if (args.size() < 1) {
        cerr << "usage: mygit checkout [--dry-run] <commit_sha>\n";
        return 1;
    }
    
    bool dry_run = false;
    string target_commit_sha;
    
    if (args.size() >= 2 && args[0] == "--dry-run") {
        dry_run = true;
        target_commit_sha = args[1];
    } else {
        target_commit_sha = args[0];
    }
    
    auto target_commit_obj = read_object(target_commit_sha);
    if (target_commit_obj.first.empty()) {
        cerr << "error: commit not found: " << target_commit_sha << "\n";
        return 1;
    }
    if (target_commit_obj.first != "commit") {
        cerr << "error: object is not a commit: " << target_commit_sha << "\n";
        return 1;
    }
    
    istringstream ss(target_commit_obj.second);
    string line;
    string target_tree_sha;
    while (getline(ss, line)) {
        if (line.find("tree ") == 0) {
            target_tree_sha = line.substr(5);
            break;
        }
    }
    
    if (target_tree_sha.empty()) {
        cerr << "error: no tree found in commit\n";
        return 1;
    }
    
    string head_ref = read_head();
    string current_commit_sha;
    if (!head_ref.empty() && head_ref.find("refs/") != string::npos) {
        current_commit_sha = read_ref(head_ref);
    }
    
    vector<CheckoutChange> changes = plan_checkout(target_tree_sha, current_commit_sha);
    
    // DEBUG: Show what's in each tree
    cerr << "\n[DEBUG] Tree contents:\n";
    unordered_map<string, string> target_files, current_files;
    collect_tree_files(target_tree_sha, "", target_files);
    if (!current_commit_sha.empty()) {
        string current_tree_sha;
        auto curr_commit = read_object(current_commit_sha);
        if (!curr_commit.first.empty()) {
            istringstream css(curr_commit.second);
            string cline;
            while (getline(css, cline)) {
                if (cline.find("tree ") == 0) {
                    current_tree_sha = cline.substr(5);
                    break;
                }
            }
            if (!current_tree_sha.empty()) {
                collect_tree_files(current_tree_sha, "", current_files);
            }
        }
    }
    cerr << "  Target tree files (" << target_files.size() << "): ";
    for (auto &kv : target_files) cerr << kv.first << " ";
    cerr << "\n";
    cerr << "  Current tree files (" << current_files.size() << "): ";
    for (auto &kv : current_files) cerr << kv.first << " ";
    cerr << "\n";
    cerr << "\n";
    
    cout << "Checkout plan for commit " << target_commit_sha.substr(0, 7) << ":\n";
    cout << "Target tree SHA: " << target_tree_sha.substr(0, 7) << "\n";
    cout << "Current commit SHA: " << (current_commit_sha.empty() ? "none" : current_commit_sha.substr(0, 7)) << "\n";
    cout << "\n";
    
    int restore_count = 0, delete_count = 0;
    for (auto &change: changes) {
        if (change.action == "restore") {
            cout << "  restore: " << change.path << "\n";
            restore_count++;
        } else if (change.action == "delete") {
            cout << "  delete:  " << change.path << "\n";
            delete_count++;
        }
    }
    
    cout << "\n";
    cout << "Summary: " << restore_count << " files to restore, " << delete_count << " files to delete\n";
    cout << "\n";
    
    if (dry_run) {
        cout << "(dry-run: no files modified)\n";
        return 0;
    }
    
    cout << "Applying changes...\n";
    
    unordered_map<string, string> target_tree_files;
    collect_tree_files(target_tree_sha, "", target_tree_files);
    
    for (auto &change: changes) {
        if (change.action == "restore") {
            const string &path = change.path;
            string blob_sha = target_tree_files[path];
            
            // Create parent directories
            size_t last_slash = path.rfind('/');
            if (last_slash != string::npos) {
                string dir = path.substr(0, last_slash);
                ensure_dir(dir);
            }
            
            auto blob_obj = read_object(blob_sha);
            if (blob_obj.first.empty()) {
                cerr << "error: failed to read blob: " << blob_sha << "\n";
                continue;
            }
            write_file(path, blob_obj.second);
        } else if (change.action == "delete") {
            const string &path = change.path;
            
            if (filesystem::exists(path)) {
                try {
                    filesystem::remove(path);
                } catch (...) {
                    cerr << "warning: failed to delete: " << path << "\n";
                }
            }
        }
    }
    
    // Update HEAD to point to the new commit
    if (!write_ref(head_ref, target_commit_sha)) {
        cerr << "error: failed to update HEAD\n";
        return 1;
    }
    
    // Update index to match target tree
    vector<tuple<string,string,string>> index_entries;
    for (auto &kv: target_tree_files) {
        index_entries.emplace_back(kv.first, "100644", kv.second);
    }
    write_index(index_entries);
    
    cout << "Checkout complete! HEAD detached at " << target_commit_sha.substr(0, 7) << "\n";
    return 0;
}
