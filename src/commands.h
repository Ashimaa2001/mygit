#ifndef COMMANDS_H
#define COMMANDS_H

#include <vector>
#include <string>

int cmd_init();
int cmd_hash_object(const std::vector<std::string> &args);
int cmd_cat_file(const std::vector<std::string> &args);
int cmd_write_tree();

#endif // COMMANDS_H
