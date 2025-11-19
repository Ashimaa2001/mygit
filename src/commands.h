#ifndef COMMANDS_H
#define COMMANDS_H

#include <vector>
#include <string>

int cmd_init();
int cmd_hash_object(const std::vector<std::string> &args);
int cmd_cat_file(const std::vector<std::string> &args);
int cmd_write_tree();
int cmd_ls_tree(const std::vector<std::string> &args);
int cmd_add(const std::vector<std::string> &args);
int cmd_commit(const std::vector<std::string> &args);

#endif // COMMANDS_H
