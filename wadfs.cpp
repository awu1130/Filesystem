#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../libWad/Wad.h"

static int getattr_callback(const char* path, struct stat* stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    Wad* wad = (Wad*)fuse_get_context()->private_data;
    if (wad->isContent(path)) {
        stbuf->st_mode = S_IFREG | 0777;
        stbuf->st_nlink = 1;
        stbuf->st_size = wad->getSize(path);
        return 0;
    } else if (wad->isDirectory(path)) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        return 0;
    }
    return -ENOENT;
}

static int mknod_callback(const char* path, mode_t mode, dev_t dev) {
    Wad* wad = (Wad*)fuse_get_context()->private_data;
    wad->createFile(path);
    if (!wad->isContent(path)) {
        return -ENOENT;
    }
    return 0;
}

static int mkdir_callback(const char* path, mode_t mode) {
    Wad* wad = (Wad*)fuse_get_context()->private_data;
    wad->createDirectory(path);
    if (!wad->isDirectory(path)) {
        return -ENOENT;
    }
    return 0;
}

static int read_callback(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    Wad* wad = (Wad*)fuse_get_context()->private_data;
    if (!wad->isContent(path)) {
        return -ENOENT;
    }
    int bytesRead = wad->getContents(path, buf, size, offset);
    if (bytesRead < 0) {
        return -EIO;
    }
    return bytesRead;
}

static int write_callback(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    Wad* wad = (Wad*)fuse_get_context()->private_data;
    if (!wad->isContent(path)) {
        return -ENOENT;
    }
    int bytesWritten = wad->writeToFile(path, buf, size, offset);
    if (bytesWritten < 0) {
        return -EIO;
    }
    return bytesWritten;
}

static int readdir_callback(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    Wad* wad = (Wad*)fuse_get_context()->private_data;
    if (!wad->isDirectory(path)) {
        return -ENOENT;
    }
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    std::vector<std::string> entries;
    wad->getDirectory(path, &entries);
    for (const std::string& entry : entries) {
        filler(buf, entry.c_str(), NULL, 0);
    }
    return 0;
}

static struct fuse_operations operations = {
    .getattr = getattr_callback,
    .mknod = mknod_callback,
    .mkdir = mkdir_callback,
    .read = read_callback,
    .write = write_callback,
    .readdir = readdir_callback,
};

int main(int argc, char* argv[]) {
    // from ernesto vid

    if (argc < 3) {
        std::cout << "Not enough arguments." << std::endl;
        exit(EXIT_SUCCESS);
    }

    std::string wadPath = argv[argc - 2];

    if (wadPath.at(0) != '/') {
        wadPath = std::string(get_current_dir_name()) + "/" + wadPath;
    }
    Wad* myWad = Wad::loadWad(wadPath);

    argv[argc - 2] = argv[argc - 1];
    argc--;

    return fuse_main(argc, argv, &operations, myWad);
}
