#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sstream>
#include <iostream>
#include <map>
#include <string>
#include <algorithm>

#include "DistributedFileSystemService.h"
#include "ClientError.h"
#include "ufs.h"
#include "WwwFormEncodedDict.h"

#include <string.h>

using namespace std;

const int ROOT_INODE = 0;

bool cmp(const dir_ent_t a, const dir_ent_t b) {
  return strcmp(a.name, b.name) < 0;
}

bool parsePath(const string &url, istringstream &paths) {
  const string root = "ds3/";
  const int rootIndex = url.find(root);
  if (rootIndex == static_cast<const int>(string::npos))
    return false;
  
  string allPaths = url.substr(rootIndex + root.size());
  paths = istringstream(allPaths);
  return true;
}

DistributedFileSystemService::DistributedFileSystemService(string diskFile) : HttpService("/ds3/") {
  fileSystem = new LocalFileSystem(new Disk(diskFile, UFS_BLOCK_SIZE));
}

void DistributedFileSystemService::get(HTTPRequest *request, HTTPResponse *response) {
  const string url = request->getUrl();
  istringstream paths;
  if (!parsePath(url, paths)) {
    response->setBody("");
    return;
  }

  int inodeNum = ROOT_INODE;
  string entryName;
  while (getline(paths, entryName, '/')) {
    const int nextInode = fileSystem->lookup(inodeNum, entryName);
    if (nextInode == -ENOTFOUND) {
      response->setStatus(ClientError::notFound().status_code);
      response->setBody(ClientError::notFound().what());
      return;
    } else if (nextInode < 0) {
      response->setStatus(ClientError::badRequest().status_code);
      response->setBody(ClientError::badRequest().what());
      return;
    }
    inodeNum = nextInode;
  }

  inode_t inode;
  fileSystem->stat(inodeNum, &inode);

  if (inode.type == UFS_REGULAR_FILE) {
    char buffer[inode.size + 1];
    if (fileSystem->read(inodeNum, buffer, inode.size) < 0) {
      response->setStatus(ClientError::badRequest().status_code);
      response->setBody(ClientError::badRequest().what());
      return;
    }

    buffer[inode.size] = '\0';
    response->setBody(buffer);
  } else {
    const int numEntries = inode.size / sizeof(dir_ent_t);
    dir_ent_t entries[inode.size / sizeof(dir_ent_t)];
    if (fileSystem->read(inodeNum, entries, inode.size) < 0) {
      response->setStatus(ClientError::badRequest().status_code);
      response->setBody(ClientError::badRequest().what());
      return;
    }

    sort(entries, entries + numEntries, cmp);
    
    string result = "";
    for (dir_ent_t entry : entries) {
      string entryName(entry.name);
      if (entryName == "." || entryName == "..")
        continue;

      inode_t entryInode;
      fileSystem->stat(entry.inum, &entryInode);

      if (entryInode.type == UFS_DIRECTORY)
        entryName.push_back('/');

      result += entryName + "\n";
    }

    response->setBody(result);
  }
}

void DistributedFileSystemService::put(HTTPRequest *request, HTTPResponse *response) {
  const string url = request->getUrl();
  istringstream paths;
  if (!parsePath(url, paths)) {
    response->setBody("");
    return;
  }

  vector<string> pathVec;
  string entryName;
  while (getline(paths, entryName, '/')) {
    pathVec.push_back(entryName);
  }

  int inodeNum = ROOT_INODE;
  for (int i = 0; i < (int)pathVec.size(); ++i) {
    const string nextEntry = pathVec[i];
    int nextInode = fileSystem->lookup(inodeNum, nextEntry);
    if (nextInode == -ENOTFOUND) {
      const bool isLast = i == (int)pathVec.size() - 1;
      const bool type = isLast ? UFS_REGULAR_FILE : UFS_DIRECTORY;
      nextInode = fileSystem->create(inodeNum, type, nextEntry);
      if (nextInode == -ENOTENOUGHSPACE) {
        response->setStatus(ClientError::insufficientStorage().status_code);
        response->setBody(ClientError::insufficientStorage().what());
        return;
      } else if (nextInode == -EINVALIDTYPE) {
        response->setStatus(ClientError::conflict().status_code);
        response->setBody(ClientError::conflict().what());
        return;
      } else if (nextInode < 0) {
        response->setStatus(ClientError::badRequest().status_code);
        response->setBody(ClientError::badRequest().what());
        return;
      }
    } else if (nextInode < 0) {
      response->setStatus(ClientError::badRequest().status_code);
      response->setBody(ClientError::badRequest().what());
      return;
    }
    inodeNum = nextInode;
  }

  const string content = request->getBody();
  const int bytesWritten = fileSystem->write(inodeNum, content.data(), content.size());
  if (bytesWritten == -ENOTENOUGHSPACE || bytesWritten == -EINVALIDSIZE) {
    response->setStatus(ClientError::insufficientStorage().status_code);
    response->setBody(ClientError::insufficientStorage().what());
    return;
  } else if (bytesWritten < 0) {
    response->setStatus(ClientError::badRequest().status_code);
    response->setBody(ClientError::badRequest().what());
    return;
  }

  response->setBody("");
}

void DistributedFileSystemService::del(HTTPRequest *request, HTTPResponse *response) {
  const string url = request->getUrl();
  istringstream paths;
  if (!parsePath(url, paths)) {
    response->setBody("");
    return;
  }

  vector<string> pathVec;
  string entryName;
  while (getline(paths, entryName, '/')) {
    pathVec.push_back(entryName);
  }

  super_t superBlock;
  fileSystem->readSuperBlock(&superBlock);

  int parentInodeNum = -1;
  int inodeNum = ROOT_INODE;
  string currentEntry;
  for (int i = 0; i < (int)pathVec.size(); ++i) {
    currentEntry = pathVec[i];

    inode_t currentInode;
    fileSystem->stat(inodeNum, &currentInode);

    if (currentInode.type != UFS_DIRECTORY) {
      response->setStatus(ClientError::badRequest().status_code);
      response->setBody(ClientError::badRequest().what());
      return;
    }

    parentInodeNum = inodeNum;
    inodeNum = fileSystem->lookup(inodeNum, currentEntry);

    if (inodeNum < 0) {
      response->setBody("");
      return;
    }
  }

  if (parentInodeNum == -1) {
    response->setStatus(ClientError::badRequest().status_code);
    response->setBody(ClientError::badRequest().what());
    return;
  }

  const int unlinkStatus = fileSystem->unlink(parentInodeNum, currentEntry);
  if (unlinkStatus < 0) {
    response->setStatus(ClientError::badRequest().status_code);
    response->setBody(ClientError::badRequest().what());
    return;
  }

  response->setBody("");
}
