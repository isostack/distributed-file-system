/* 
* The ds3cat utility prints the contents of a file to standard output. 
* It takes the name of the disk image file and an inode number as the only arguments. 
* It prints the contents of the file that is specified by the inode number.
*/

#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

// Uses:
// LocalFileSystem.readSuperBlock
// LocalFileSystem.stat
// LocalFileSystem.read

int main(int argc, char *argv[]) {
  // DEFAULT ERROR
  if (argc != 3) {
    cout << argv[0] << ": diskImageFile inodeNumber" << endl;
    return 1;
  }

  // PASSED ERROR CHECK
  // Parse arguments
  Disk disk(argv[1], UFS_BLOCK_SIZE);
  int inodeNumber = atoi(argv[2]);

  // Initialize Disk, LocalFileSystem
  LocalFileSystem localFileSystem(&disk);

  // Read super block
  super_t superBlock;
  localFileSystem.readSuperBlock(&superBlock);
  
  // Get inode
  inode_t inode;
  if (localFileSystem.stat(inodeNumber, &inode) != 0) {
      return -1; // ERROR: invalid
  }

  // FIND number of blocks
  int numBlocks = (inode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

  // PRINT the File blocks
  cout << "File blocks" << endl;
  for (int blockIndex = 0; blockIndex < numBlocks; ++blockIndex) {
      cout << inode.direct[blockIndex] << endl;
  }
  cout << endl;

  // FIND bytes read
  char dataFile[inode.size + 1];
  int bytesRead = localFileSystem.read(inodeNumber, dataFile, inode.size);

  // PRINT the File data
  cout << "File data" << endl;
  if (bytesRead < 0) {
      return -1; // ERROR: fail read
  }
  dataFile[inode.size] = '\0';
  cout << dataFile;

  return 0;
}
