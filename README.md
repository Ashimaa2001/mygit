# mygit
A simple version control system inspired by Git. Designed to help manage code versions, track changes, and collaborate on projects with a minimal footprint


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
```