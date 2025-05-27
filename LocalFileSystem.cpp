#include <iostream>
#include <string>
#include <vector>
#include <assert.h>

#include "LocalFileSystem.h"
#include "ufs.h"

// Other Libraries
#include <cstring>
#include <cmath>

using namespace std;

const int ENTRIES_IN_BLOCK = UFS_BLOCK_SIZE / sizeof(dir_ent_t);
const int INODES_IN_BLOCK = UFS_BLOCK_SIZE / sizeof(inode_t);

// HELPERS

// ==================================
inline bool checkInode(super_t & super, int num) {
  if (num < super.num_inodes && num >= 0) {
    return true;
  }
  return false;
}

inline int block2Bit(const super_t &super, const int num) {
  return num - super.data_region_addr;
}

inline int bit2Block(const super_t &super, const int bit) {
  return bit + super.data_region_addr;
}

bool findBit(const unsigned char *bitmap, const int bit) {
  int byteIdx = bit / 8;
  int bitOffset = bit % 8;
  unsigned char byte = bitmap[byteIdx];

  return byte & (1 << bitOffset);
}

void createBit(unsigned char *bitmap, const int bit) {
  int byteIdx = bit / 8;
  int bitOffset = bit % 8;
  unsigned char byte = bitmap[byteIdx];

  bitmap[byteIdx] = byte | (1 << bitOffset);
}

void delBit(unsigned char *bitmap, const int bit) {
  int byteIdx = bit / 8;
  int bitOffset = bit % 8;
  unsigned char byte = bitmap[byteIdx];

  bitmap[byteIdx] = byte & ~(1 << bitOffset);
}

int countBits(const unsigned char *bitmap, const int size) {
  int availableBits = 0;
  
  for (int i = 0; i < size; ++i)
    if (!findBit(bitmap, i))
      ++availableBits;

  return availableBits;
}

int divide(const int a, const int b) {
  return (a+b-1) / b;
}

int findFirstBit(const unsigned char *bitmap, const int len) {
  for (int i = 0; i < len * 8; ++i) {
    if (!findBit(bitmap, i)) {
      return i;
    }
  }

  return -1;
}

// ==================================
// !!
// LOOK AT FILESYSTEM.H FOR FUNCTIONS
// !!
// ==================================

// done
LocalFileSystem::LocalFileSystem(Disk *disk) {
  this->disk = disk;
}

// done
void LocalFileSystem::readSuperBlock(super_t *super) {
  char buffer[UFS_BLOCK_SIZE];
  disk->readBlock(0, buffer);
  memcpy(super, buffer, sizeof(super_t));
}

// done
int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  // Read super block
  super_t superBlock;
  readSuperBlock(&superBlock);

  // CHECK if valid inode number
  if (checkInode(superBlock, parentInodeNumber) == false) {
    return -EINVALIDINODE;
  }

  // Get inode information
  inode_t parentInode;
  stat(parentInodeNumber, &parentInode);

  // CHECK if valid inode
  if (parentInode.type != UFS_DIRECTORY) {
    return -EINVALIDINODE;
  }

  // Go through all direct pointers
  for (int i = 0; i < DIRECT_PTRS; ++i) {
    int blockNumber = parentInode.direct[i];

    // ERROR
    if (blockNumber == -1) {
      return -ENOTFOUND;
    }

    // Read directory entries from block
    dir_ent_t directoryEntries[ENTRIES_IN_BLOCK];
    disk->readBlock(parentInode.direct[i], directoryEntries);

    // Go through each entry
    for (dir_ent_t entry : directoryEntries) {
      // ERROR
      if (entry.inum == -1) {
        return -ENOTFOUND;
      }

      // RETURN inode number
      if (string(entry.name) == name) {
        return entry.inum;
      }
    }
  }

  // ERROR, name not found
  return -ENOTFOUND;
}

// done
int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  // Read super block
  super_t superBlock;
  readSuperBlock(&superBlock);

  // CHECK if valid inode number
  if (checkInode(superBlock, inodeNumber) == false) {
    return -EINVALIDINODE;
  }

  // Find block number and offset
  int inodePosition = inodeNumber * sizeof(inode_t);
  int blockNumber = superBlock.inode_region_addr + inodePosition / UFS_BLOCK_SIZE;
  int inodeOffset = inodePosition % UFS_BLOCK_SIZE;

  // Buffer
  char blockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(blockNumber, blockBuffer);

  // COPY
  memcpy(inode, blockBuffer + inodeOffset, sizeof(inode_t));

  return 0;
}

// done
int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  // Read super block
  super_t superBlock;
  readSuperBlock(&superBlock);
  
  // CHECK if valid inode number
  if (checkInode(superBlock, inodeNumber) == false) {
    return -EINVALIDINODE;
  }

  // CHECK if valid file size
  if (size > MAX_FILE_SIZE || size<0) {
    return -EINVALIDSIZE;
  }

  // Get inode information
  inode_t inode;
  stat(inodeNumber, &inode);

  // CHECK inode type
  if (inode.type != UFS_REGULAR_FILE && inode.type != UFS_DIRECTORY) {
    return -EINVALIDTYPE; // ERROR: invalid inode type
  }
  
  // Find size to read and the number of blocks
  int readSize = min(size, inode.size);
  int numBlocks = (readSize + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  int lastBlockSize = readSize % UFS_BLOCK_SIZE;
  if (lastBlockSize == 0) {
    lastBlockSize = UFS_BLOCK_SIZE;
  }

  // Read data from the blocks
  for (int idx = 0; idx < numBlocks; ++idx) {
    int currentBlock = inode.direct[idx];
    unsigned char blockBuffer[UFS_BLOCK_SIZE];
    disk->readBlock(currentBlock, blockBuffer);

    int bufferOffset = idx * UFS_BLOCK_SIZE;
    int bytes = UFS_BLOCK_SIZE;

    // Checking if the current block is the last one
    if (idx == numBlocks - 1) {
      bytes = lastBlockSize;
    }

    // Copy the bytes
    memcpy((unsigned char*)(buffer) + bufferOffset, blockBuffer, bytes);
  }

  return readSize;
}

// done
int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  super_t superBlock;
  readSuperBlock(&superBlock); // Read the superblock from the filesystem

  if (!checkInode(superBlock, parentInodeNumber)) // Validate the parent inode number
    return -EINVALIDINODE;

  if ((name.size() < DIR_ENT_NAME_SIZE) == false) // Validate the name size
    return -EINVALIDNAME;

  if (type != UFS_DIRECTORY && type != UFS_REGULAR_FILE) // Validate the file type
    return -EINVALIDTYPE;

  const int inodeBitmapSize = superBlock.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char inodeBitmap[inodeBitmapSize];
  this->readInodeBitmap(&superBlock, inodeBitmap); // Read the inode bitmap

  const int numInodes = superBlock.num_inodes;
  inode_t inodes[numInodes];
  readInodeRegion(&superBlock, inodes); // Read the inode region

  const int dataBitmapSize = superBlock.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char dataBitmap[dataBitmapSize];
  this->readDataBitmap(&superBlock, dataBitmap); // Read the data bitmap

  inode_t &parentInode = inodes[parentInodeNumber];
  if (parentInode.type != UFS_DIRECTORY) // Ensure the parent inode is a directory
    return -EINVALIDINODE;

  if (parentInode.size == UFS_BLOCK_SIZE * DIRECT_PTRS) // Check for available space
    return -ENOTENOUGHSPACE;

  const int numParentEntries = parentInode.size / sizeof(dir_ent_t);
  dir_ent_t parentEntries[numParentEntries];
  read(parentInodeNumber, parentEntries, numParentEntries * sizeof(dir_ent_t)); // Read parent directory entries

  for (int i = 0; i < numParentEntries; ++i) {
    dir_ent_t entry = parentEntries[i];
    string entryName(entry.name);

    if (entryName == name) { // Check if the name already exists
      inode_t inode = inodes[entry.inum];
      if (inode.type == type) {
        return 0;
      } else {
        return -EINVALIDTYPE;
      }
    }
  }

  const int availableInode = findFirstBit(inodeBitmap, inodeBitmapSize); // Find an available inode
  if (availableInode < 0 || availableInode >= superBlock.num_inodes)
    return -ENOTENOUGHSPACE;

  createBit(inodeBitmap, availableInode); // Mark the inode as used

  int newBlock = -1;
  dir_ent_t newEntries[ENTRIES_IN_BLOCK];
  inodes[availableInode].type = type; // Set the inode type

  if (type == UFS_DIRECTORY) {
    const int availableDataBit = findFirstBit(dataBitmap, dataBitmapSize); // Find an available data block
    if (availableDataBit < 0 || availableDataBit >= superBlock.num_data)
      return -ENOTENOUGHSPACE;

    createBit(dataBitmap, availableDataBit); // Mark the data block as used
    newBlock = bit2Block(superBlock, availableDataBit);
    
    newEntries[0].inum = availableInode;
    strcpy(newEntries[0].name, ".");
    newEntries[1].inum = parentInodeNumber;
    strcpy(newEntries[1].name, "..");

    for (int i = 2; i < ENTRIES_IN_BLOCK; ++i)
      newEntries[i].inum = -1;

    inodes[availableInode].size = 2 * sizeof(dir_ent_t); // Set the size of the new directory
    inodes[availableInode].direct[0] = newBlock; // Assign the data block to the inode
  } else {
    inodes[availableInode].size = 0; // Set the size for a regular file
  }

  int entryBlock = -1;
  dir_ent_t entries[ENTRIES_IN_BLOCK];

  if (parentInode.size % UFS_BLOCK_SIZE == 0) {
    entries[0].inum = availableInode;
    strcpy(entries[0].name, name.c_str());

    for (int i = 1; i < ENTRIES_IN_BLOCK; ++i)
      entries[i].inum = -1;

    const int entryBlockBit = findFirstBit(dataBitmap, dataBitmapSize); // Find an available entry block
    if (entryBlockBit < -1 || entryBlockBit >= superBlock.num_data)
      return -ENOTENOUGHSPACE;
    
    createBit(dataBitmap, entryBlockBit); // Mark the entry block as used
    entryBlock = bit2Block(superBlock, entryBlockBit);

    const int parentBlockIndex = parentInode.size / UFS_BLOCK_SIZE;
    parentInode.direct[parentBlockIndex] = entryBlock; // Assign the entry block to the parent inode
  } else {
    const int directSize = divide(parentInode.size, UFS_BLOCK_SIZE);
    const int lastBlockIndex = directSize - 1;
    entryBlock = parentInode.direct[lastBlockIndex];
    
    disk->readBlock(entryBlock, entries); // Read the last block

    const int entryIndex = parentInode.size % UFS_BLOCK_SIZE / sizeof(dir_ent_t);
    entries[entryIndex].inum = availableInode;
    strcpy(entries[entryIndex].name, name.c_str()); // Add the new entry
  }

  parentInode.size += sizeof(dir_ent_t); // Update the size of the parent inode

  this->disk->beginTransaction();
  this->writeInodeBitmap(&superBlock, inodeBitmap); // Write the updated inode bitmap
  this->writeDataBitmap(&superBlock, dataBitmap); // Write the updated data bitmap
  this->writeInodeRegion(&superBlock, inodes); // Write the updated inode region
  this->disk->writeBlock(entryBlock, entries); // Write the updated entries
  
  if (newBlock != -1)
    this->disk->writeBlock(newBlock, newEntries); // Write the new entries if needed

  this->disk->commit();

  return availableInode;
}


// done
int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  super_t superBlock;
  readSuperBlock(&superBlock);

  // Validate input
  if (checkInode(superBlock, inodeNumber) == false)
    return -EINVALIDINODE;

  if (size < 0 || size > MAX_FILE_SIZE)
    return -EINVALIDSIZE;

  // Get the inode region
  inode_t inodeRegion[superBlock.num_inodes];
  readInodeRegion(&superBlock, inodeRegion);

  // Get the specific inode to write
  inode_t &inodeWrite = inodeRegion[inodeNumber];
  if (inodeWrite.type != UFS_REGULAR_FILE)
    return -EINVALIDTYPE;

  // Read the data bitmap
  const int dataBitmapSize = superBlock.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char dataBitmap[dataBitmapSize];
  readDataBitmap(&superBlock, dataBitmap);

  // Determine the number of blocks to allocate or deallocate
  int curBlocks = divide(inodeWrite.size, UFS_BLOCK_SIZE);
  int requiredBlocks = divide(size, UFS_BLOCK_SIZE);
  int blocksToAllocate = max(0, requiredBlocks - curBlocks);
  int blocksToDeallocate = max(0, curBlocks - requiredBlocks);

  if (requiredBlocks > DIRECT_PTRS)
    return -ENOTENOUGHSPACE;

  // Allocate new blocks if needed
  for (int i = 0; i < blocksToAllocate; ++i) {
    int idx = curBlocks + i; // Append at the end
    
    // Get the first available data block number, and set that bit
    const int availableBit = findFirstBit(dataBitmap, dataBitmapSize);
    if (availableBit < 0 || availableBit >= superBlock.num_data)
      return -ENOTENOUGHSPACE;
    createBit(dataBitmap, availableBit);

    // Set the available block number in inode
    inodeWrite.direct[idx] = bit2Block(superBlock, availableBit);
  }

  // Deallocate blocks if needed
  for (int i = 1; i <= blocksToDeallocate; ++i) {
    const int idx = curBlocks - i;
    const int blockToFree = inodeWrite.direct[idx];
    const int bitToFree = block2Bit(superBlock, blockToFree);

    // Clear the bit to deallocate
    delBit(dataBitmap, bitToFree);
  }

  // Update inode size
  inodeWrite.size = size;

  // Get the size of the last block
  const int lastBlockSize = (inodeWrite.size - 1) % UFS_BLOCK_SIZE + 1;

  // Perform the write operation
  disk->beginTransaction();
  
  writeInodeRegion(&superBlock, inodeRegion);
  writeDataBitmap(&superBlock, dataBitmap);

  // Write to all the blocks
  for (int i = 0; i < requiredBlocks; ++i) {
    const bool isLastBlock = i == requiredBlocks - 1;
    const int bytesToCopy = isLastBlock ? lastBlockSize : UFS_BLOCK_SIZE;
    
    const int bufferOffset = i * UFS_BLOCK_SIZE;
    const int blockNumber = inodeWrite.direct[i];

    unsigned char blockContent[UFS_BLOCK_SIZE];
    memcpy(blockContent, (unsigned char *)buffer + bufferOffset, bytesToCopy);

    disk->writeBlock(blockNumber, blockContent);
  }

  disk->commit();

  return size;
}

// done
int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  super_t superBlock;
  readSuperBlock(&superBlock);

  // Validate inputs
  if (checkInode(superBlock, parentInodeNumber) == false)
    return -EINVALIDINODE;

  // CHECK
  if ((name.size() < DIR_ENT_NAME_SIZE) == false)
    return -EINVALIDNAME;

  // CHECK
  if ((name != "." && name != "..") == false) {
    return -EUNLINKNOTALLOWED;
  }

  // Read inodes
  const int numInodes = superBlock.num_inodes;
  inode_t inodes[numInodes];
  readInodeRegion(&superBlock, inodes);

  // Get parent inode
  inode_t &parentInode = inodes[parentInodeNumber];
  if (parentInode.type != UFS_DIRECTORY)
    return -EINVALIDINODE;

  // Read data bitmap
  const int dataBitmapSize = superBlock.data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char dataBitmap[dataBitmapSize];
  readDataBitmap(&superBlock, dataBitmap);

  // Read inode bitmap
  const int inodeBitmapSize = superBlock.inode_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char inodeBitmap[inodeBitmapSize];
  readInodeBitmap(&superBlock, inodeBitmap);

  // Load directory entries
  const int numEntries = parentInode.size / sizeof(dir_ent_t);
  vector<dir_ent_t> entries(numEntries);
  read(parentInodeNumber, entries.data(), parentInode.size);

  // Find the entry to delete
  int entryIndex = -1;
  for (int i = 0; i < numEntries; ++i) {
    if (string(entries[i].name) == name) {
      entryIndex = i;
      break;
    }
  }
  if (entryIndex < 0)
    return 0;

  // Delete inode contents
  const int inodeToDelete = entries[entryIndex].inum;
  inode_t inode = inodes[inodeToDelete];
  if (inode.type == UFS_DIRECTORY && inode.size > (int)sizeof(dir_ent_t) * 2)
    return -EDIRNOTEMPTY;

  int blocksToDelete = divide(inode.size, UFS_BLOCK_SIZE);
  for (int i = 0; i < blocksToDelete; ++i) {
    const int blockNum = inode.direct[i];
    const int bitToClear = block2Bit(superBlock, blockNum);
    delBit(dataBitmap, bitToClear);
  }

  // Clear inode bit
  delBit(inodeBitmap, inodeToDelete);

  // Remove directory entry
  entries.erase(entries.begin() + entryIndex);
  parentInode.size -= sizeof(dir_ent_t);

  // Delete last block if not needed
  if (parentInode.size % UFS_BLOCK_SIZE == 0) {
    const int blocksNeeded = parentInode.size / UFS_BLOCK_SIZE;
    if (blocksNeeded < DIRECT_PTRS) {
      const int blockNum = parentInode.direct[blocksNeeded];
      const int bitToClear = block2Bit(superBlock, blockNum);
      delBit(dataBitmap, bitToClear);
      parentInode.direct[blocksNeeded] = -1;
    }
  }

  // Pad directory entries to block size
  while (entries.size() * sizeof(dir_ent_t) % UFS_BLOCK_SIZE != 0) {
    dir_ent_t emptyEntry;
    emptyEntry.inum = -1;
    entries.push_back(emptyEntry);
  }

  // Write changes
  disk->beginTransaction();
  writeInodeRegion(&superBlock, inodes);
  writeDataBitmap(&superBlock, dataBitmap);
  writeInodeBitmap(&superBlock, inodeBitmap);

  for (int i = 0; i < divide(parentInode.size, UFS_BLOCK_SIZE); ++i) {
    const int offset = i * ENTRIES_IN_BLOCK;
    disk->writeBlock(parentInode.direct[i], entries.data() + offset);
  }

  disk->commit();

  return 0;
}


// ======================================================================
// Helper functions, you should read/write the entire inode and bitmap regions
// ======================================================================

// DISK HAS SPACE

bool LocalFileSystem::diskHasSpace(super_t *super, int numInodesNeeded, int numDataBytesNeeded, int numDataBlocksNeeded) {
  // CHECK inode space available
  int inodeBitmapSize = super->inode_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char inodeBitmap[inodeBitmapSize];
  this->readInodeBitmap(super, inodeBitmap);

  int availableInodes = countBits(inodeBitmap, inodeBitmapSize);

  // CHECK available inodes
  if (availableInodes < numInodesNeeded) {
    return false;
  }

  // CHECK data space available
  int dataBitmapSize = super->data_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char dataBitmap[dataBitmapSize];
  this->readDataBitmap(super, dataBitmap);

  int availableDataBlocks = countBits(dataBitmap, dataBitmapSize);

  return availableDataBlocks >= numDataBlocksNeeded + ((numDataBytesNeeded + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE);
}

// READ FUNCTIONS

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) {    
  for (int i = 0; i < super->inode_bitmap_len; ++i) {
    int block = super->inode_bitmap_addr + i;
    auto buff = inodeBitmap + i * UFS_BLOCK_SIZE;
    disk->readBlock(block, buff);
  }
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) {
  for (int i = 0; i < super->data_bitmap_len; ++i) {
    int block = super->data_bitmap_addr + i;
    auto buff = dataBitmap + i * UFS_BLOCK_SIZE;
    disk->readBlock(block, buff);
  }
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
  for (int i = 0; i < super->inode_region_len; ++i) {
    int block = super->inode_region_addr + i;
    auto buff = inodes + i * INODES_IN_BLOCK;
    disk->readBlock(block, buff);
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap) {  
  for (int i = 0; i < super->inode_bitmap_len; ++i) {
    int block = super->inode_bitmap_addr + i;
    auto buff = inodeBitmap + i * UFS_BLOCK_SIZE;
    disk->writeBlock(block, buff);
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {
  for (int i = 0; i < super->data_bitmap_len; ++i) {
    int block = super->data_bitmap_addr + i;
    auto buff = dataBitmap + i * UFS_BLOCK_SIZE;
    disk->writeBlock(block, buff);
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {
  for (int i = 0; i < super->inode_region_len; ++i) {
    int block = super->inode_region_addr + i;
    auto buff = inodes + i * INODES_IN_BLOCK;
    disk->writeBlock(block, buff);
  }
}
