/*
* The ds3ls utility prints the names for all of the files and directories in a disk image. 
* This utility takes a single command line argument: the name of the disk image file to use. 
* This program will start at the root of the file system, 
* print the contents of that directory in full, 
* and then traverse to each directory contained within. 
* This process repeats in a depth-first fashion until all file and directory names have been printed.
*/

#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

//#include <vector>
using namespace std;

// Uses:
// LocalFileSystem.readSuperBlock
// LocalFileSystem.stat
// LocalFileSystem.read

bool compareEntries(dir_ent_t &lhs, dir_ent_t &rhs) {
  return strcmp(lhs.name, rhs.name) < 0;
}

// 
void printDir(int num, LocalFileSystem &fileSystem, string path) {
  inode_t inode;
  cout << "Directory " << path << endl;
  
  // ERROR CHECK
  if (fileSystem.stat(num, &inode)) {
    return;
  }
  
  int numDirectoryEntries = inode.size / sizeof(dir_ent_t);
  dir_ent_t entries[numDirectoryEntries];

  // ERROR CHECK
  if (fileSystem.read(num, entries, inode.size) < 0) {
    return;
  }

  // Sort entries by name
  sort(entries, entries + numDirectoryEntries, compareEntries);

  // PRINT each directory entry
  for (const dir_ent_t &entry : entries) {
    cout << entry.inum << "\t" << entry.name << endl;
  }
  cout << endl;

  // After printing,
  // traverse into each subdirectory recursively
  for (const dir_ent_t entry : entries) {
    inode_t nextInode;
    string entryName(entry.name);

    // CHECK
    if (entryName != ".." && entryName != ".") {
      // CHECK
      if (!fileSystem.stat(entry.inum, &nextInode)) {
        // CHECK
        if (nextInode.type == UFS_DIRECTORY) {
          // Print the next directory
          string nextPath = path + entryName + "/";
          printDir(entry.inum, fileSystem, nextPath);
        }
      }
    } // end check
  }
}

int main(int argc, char *argv[]) {
  // DEFAULT ERROR
  if (argc != 2) {
    cout << argv[0] << ": diskImageFile" << endl;
    return 1;
  }

  // Parse arguments
  Disk disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem localFileSystem(&disk);

  // Read super block
  super_t superBlock;
  localFileSystem.readSuperBlock(&superBlock);

  // 0 = root number
  printDir(0, localFileSystem, "/");
}
