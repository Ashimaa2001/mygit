#include "commands.h"

#include <bits/stdc++.h>

int main(int argc, char **argv) {
    
    if (argc < 2) {
        std::cerr << "usage: mygit <command> [args...]\n";
        return 1;
    }
    std::string cmd = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);

    if (cmd == "init") {
        return cmd_init();
    } else if (cmd == "hash-object") {
        return cmd_hash_object(args);
    }  else if (cmd == "cat-file") {
        return cmd_cat_file(args);
    } else if (cmd == "write-tree") {
        return cmd_write_tree();
    }
}
