# mygit
A simple version control system inspired by Git. Designed to help manage code versions, track changes, and collaborate on projects with a minimal footprint


## Compilation

```bash
# Build the project
make

# Clean build artifacts
make clean
```

## Usage
```bash
# Initialize a new repository
./mygit init

# Hash a file and store it
./mygit hash-object -w <file_path>

# Display object type
./mygit cat-file -t <sha>

# Display object size
./mygit cat-file -s <sha>

# Display object content
./mygit cat-file -p <sha>

# Stage files for commit
./mygit add .                  # Stage all files
./mygit add <file_path>        # Stage specific file
./mygit add <directory_path>   # Stage directory

# Create tree objects from staged files
./mygit write-tree

# List tree contents
./mygit ls-tree [--name-only] <tree_sha>

# Commit staged files
./mygit commit -m "<message>"  # Commit with message
./mygit commit                 # Commit (reads message from stdin)

# View commit history
./mygit log [<commit_sha>]     # Show commit log from a commit

# Checkout a commit
./mygit checkout <commit_sha>              # Checkout specific commit
./mygit checkout --dry-run <commit_sha>    # Preview changes without applying
```