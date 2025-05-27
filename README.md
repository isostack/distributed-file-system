
# Distributed File System (DS3)

This project implements a fully functional distributed file system server with RESTful API endpoints, custom on-disk storage structures, concurrency support, and in-memory caching. Built from the ground up in C++, this project demonstrates low-level systems programming, file system design, and performance-aware architecture.

## 🔧 Features

- **On-Disk File System**
  - Implements superblock, inodes, block bitmaps, and directory structures.
  - Fixed 4KB block size; consistent on-disk layout for crash resilience.

- **RESTful API**
  - `PUT`: Write or overwrite file contents.
  - `GET`: Retrieve file contents or list directory entries.
  - `DELETE`: Remove files and empty directories.
  - `MOVE`: Relocate files/directories using custom HTTP headers.

- **LRU Block Cache**
  - Improves I/O performance by caching recently accessed blocks.
  - Evicts least recently used blocks when full.

- **Thread-Safe Server**
  - Supports concurrent read operations.
  - Enforces atomicity and correctness for writes, deletes, and moves.
  - Uses a thread pool and reader-writer locking.

## 🧪 Utilities

- `ds3cat`: Print file contents by inode number.
- `ds3ls`: Recursively list directory structure.
- `ds3bits`: Inspect superblock and allocation bitmaps.

## 🧠 Skills Demonstrated

- Systems Programming in C++
- POSIX I/O and bit-level memory management
- RESTful service design and implementation
- Reader-writer synchronization and thread pooling
- File system layout and crash consistency

## 💻 How to Run

1. Create a disk image:
   ```bash
   ./mkfs disk.img
   ```

2. Run the server:
   ```bash
   ./server disk.img <cache-size>
   ```

3. Use `curl` or browser to interact via HTTP.

## 📁 Example API Usage

```http
PUT /ds3/docs/file.txt
Body: Hello world!

GET /ds3/docs/
→ type=1&name=file.txt

MOVE /ds3/docs/file.txt
Header: x-ds3-destination: /ds3/archive/file.txt

DELETE /ds3/archive/file.txt
```
