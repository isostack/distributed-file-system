/*
* ds3bits utility prints metadata for the file system on a disk image. 
* It takes a single command line argument: the name of a disk image file.
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
// LocalFileSystem.readInodeBitmap
// LocalFileSystem.readDataBitmap

// BITMAP PRINTER HELPER
void printBitmap(const unsigned char *bitmap, int size) {
    //cout << label << endl;
    for (int i = 0; i < size; ++i) {
        cout << (unsigned int) bitmap[i] << " ";
    }
}

int main(int argc, char *argv[]) {
  // DEFAULT ERROR
  if (argc != 2) {
    cout << argv[0] << ": diskImageFile" << endl;
    return 1;
  }

  // PASSED ERROR CHECK
  // Parse arguments
  string diskImageFileName(argv[1]);

  // Initialize Disk, LocalFileSystem
  Disk disk(diskImageFileName, UFS_BLOCK_SIZE);
  LocalFileSystem localFileSystem(&disk);

  // Read super block
  super_t superBlock;
  localFileSystem.readSuperBlock(&superBlock);

  // PRINT SUPERBLOCK
  cout << "Super" << endl
        << "inode_region_addr " << superBlock.inode_region_addr << endl
        << "data_region_addr " << superBlock.data_region_addr << endl
        << endl;

  // INODE BITMAP read/print ================
  int inodeBitmapSize = superBlock.inode_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char inodeBitmap[inodeBitmapSize];
  localFileSystem.readInodeBitmap(&superBlock, inodeBitmap);
  cout << "Inode bitmap" << endl;
  printBitmap(inodeBitmap, inodeBitmapSize);
  cout << endl << endl;

  // DATA BITMAP read/print ================
  int dataBitmapSize = superBlock.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char dataBitmap[dataBitmapSize];
  localFileSystem.readDataBitmap(&superBlock, dataBitmap);
  cout << "Data bitmap" << endl;
  printBitmap(dataBitmap, dataBitmapSize);
  cout << endl;

  return 0;
}
