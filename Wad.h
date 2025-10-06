#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <cctype>

struct Node {
    // filename, offset, length, way to store other files if given descriptor is directory
    std::string filename;
    std::string fullPath;
    unsigned int offset;
    unsigned int length;
    bool isFile;
    Node* parent;
    std::vector<Node*> children;

    Node(const std::string& x, bool w, unsigned int y = 0, unsigned int z = 0, Node* t = nullptr) : filename(x), isFile(w), offset(y), length(z), parent(t) {
        std::string pathName = x;
        if (!isFile) {
            if (x.size() >= 6 && x.substr(x.size() - 6) == "_START") {
                pathName = x.substr(0, x.size() - 6);
            } else if (x.size() >= 4 && x.substr(x.size() - 4) == "_END") {
                pathName = x.substr(0, x.size() - 4);
            }
        }
        if (parent) {
            fullPath = parent->fullPath + "/" + pathName;
        } else {
            fullPath = pathName;
        }
    }
};

class Wad {
    std::string filePath;
    std::string magic;
    unsigned int numDescriptor;
    unsigned int directoryOffset;
    std::fstream fileStream;
    Node* root;

    private:
        // constructor
        Wad(const std::string &x);

    public:
        std::vector<std::string> split(const std::string &path);
        Node* dfs(Node* current, const std::vector<std::string>& pathParts, size_t index);
        static Wad* loadWad(const std::string &path);
        std::string getMagic();
        bool isContent(const std::string &path);
        bool isDirectory(const std::string &path);
        int getSize(const std::string &path);
        int getContents(const std::string &path, char *buffer, int length, int offset = 0);
        int getDirectory(const std::string &path, std::vector<std::string> *directory);
        void rebuildDescriptorList();
        void createDirectory(const std::string &path);
        void createFile(const std::string &path);
        int writeToFile(const std::string &path, const char *buffer, int length, int offset = 0);
};