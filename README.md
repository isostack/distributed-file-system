
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

## Build Instructions

To build the project, navigate to the root directory of the repository and use the provided `Makefile`. The build process utilizes `g++` for C++ files and `gcc` for C files.

You can build all the primary targets by running:
```bash
make all
```
Alternatively, you can build all targets by simply running:
```bash
make
```

This will compile the following executables:
- `server_web`: The main distributed file system server.
- `mkfs`: Utility to create and format a new disk image.
- `ds3cat`: Utility to print file contents from the disk image.
- `ds3ls`: Utility to list directory structures from the disk image.
- `ds3bits`: Utility to inspect superblock and allocation bitmaps on the disk image.

If you need to build a specific target, you can do so by specifying its name:
```bash
make <target_name>
```
For example, to build only the server:
```bash
make server_web
```

To clean up all compiled files and executables, you can run:
```bash
make clean
```

## Dependencies

The project requires the following external libraries and tools to be installed on your system:

- **C++ Compiler**: A modern C++ compiler that supports C++11 or later (e.g., g++, clang). The `Makefile` is configured to use `g++` and `gcc`.
- **OpenSSL**: Used for cryptographic functions and secure communication. The `Makefile` includes paths for OpenSSL headers and libraries. You may need to install the development package for OpenSSL (e.g., `libssl-dev` on Debian/Ubuntu, or `openssl-devel` on Fedora/CentOS).
- **pthreads**: The POSIX threads library, used for concurrency. This is typically available by default on most Linux and macOS systems.

Ensure these dependencies are met before attempting to build the project.

## 🚀 API Endpoints

The server exposes RESTful API endpoints to interact with the distributed file system. Replace `<port_number>` with the actual port the server is running on.

### `PUT /ds3/{filePath}`
*   **Description**: Writes or overwrites the content of a file at the specified path. If the file does not exist, it is created. If it exists, its content is overwritten.
*   **Request Body**: Raw binary content of the file.
*   **Example**:
    ```bash
    curl -X PUT --data-binary "Hello from DS3!" http://localhost:<port_number>/ds3/path/to/your/file.txt
    ```
*   **Success Response**: `200 OK` (on successful write/overwrite). `201 Created` might be used if the file was newly created (standard REST practice, though the exact implementation might vary).

### `GET /ds3/{filePathOrDirPath}`
*   **Description**: Retrieves the content of a file or lists the entries within a directory.
*   **If `{filePathOrDirPath}` is a file**:
    *   **Response Body**: Raw binary content of the file.
    *   **Example**:
        ```bash
        curl http://localhost:<port_number>/ds3/path/to/your/file.txt
        ```
*   **If `{filePathOrDirPath}` is a directory**:
    *   **Response Body**: A plain text list of entries, one per line. Each line is formatted as `type=<type_id>&name=<entry_name>`.
        *   `type=0`: Indicates a directory.
        *   `type=1`: Indicates a file.
    *   **Example**:
        ```bash
        curl http://localhost:<port_number>/ds3/path/to/your/directory/
        ```
        Example Output:
        ```
        type=1&name=file1.txt
        type=0&name=subdir
        type=1&name=another_file.log
        ```
*   **Success Response**: `200 OK`.

### `DELETE /ds3/{filePathOrDirPath}`
*   **Description**: Removes a file or an empty directory from the file system.
*   **Example**:
    ```bash
    curl -X DELETE http://localhost:<port_number>/ds3/path/to/your/file.txt
    ```
    Or to delete an empty directory:
    ```bash
    curl -X DELETE http://localhost:<port_number>/ds3/path/to/your/empty_directory/
    ```
*   **Success Response**: `200 OK` (or `204 No Content` as is common for DELETE operations).

### `MOVE /ds3/{sourcePath}`
*   **Description**: Moves or renames a file or a directory from a source path to a destination path.
*   **Required Header**: `x-ds3-destination: /ds3/{destinationPath}` (specifies the new path for the file/directory).
*   **Example**:
    ```bash
    curl -X MOVE -H "x-ds3-destination: /ds3/new/path/to/file.txt" http://localhost:<port_number>/ds3/old/path/to/file.txt
    ```
*   **Success Response**: `200 OK`.

## 🚨 Error Handling

The API uses standard HTTP status codes to indicate the success or failure of a request. In case of an error, the response body may also contain a plain text message providing more details about the issue.

Here are some of the common error codes you might encounter:

*   **`400 Bad Request`**: This error is returned if the request is malformed, such as having invalid syntax or missing required parameters.
*   **`401 Unauthorized`**: Returned if the request requires authentication and the provided credentials are missing or invalid. (Note: Based on `ClientError.h`, this is a possible error, though the current system may not have explicit user authentication features implemented).
*   **`403 Forbidden`**: Indicates that the server understood the request but refuses to authorize it. This could be due to insufficient permissions for the requested operation, even if authentication is not explicitly in place.
*   **`404 Not Found`**: Returned when the server cannot find the requested resource, such as a non-existent file or directory path.
*   **`405 Method Not Allowed`**: This error occurs if the HTTP method used in the request (e.g., `POST`, `PATCH`) is not supported for the requested resource/endpoint.
*   **`409 Conflict`**: Indicates that the request could not be processed because of a conflict with the current state of the target resource. For example, this might occur if trying to delete a non-empty directory or if a resource modification conflicts with another state.
*   **`507 Insufficient Storage`**: This error is returned if the server is unable to store the representation needed to complete the request, for instance, if the disk image runs out of space.

It's recommended to check both the HTTP status code and the response body for comprehensive error information.

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
