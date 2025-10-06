#include "Wad.h"

Wad::Wad(const std::string &path) : filePath(path), root(nullptr) {
    fileStream.open(filePath, std::ios::in | std::ios::out | std::ios::binary);

    // header
    char fileMagic[4];
    fileStream.read(fileMagic, 4);
    fileStream.read(reinterpret_cast<char*>(&numDescriptor), 4);
    fileStream.read(reinterpret_cast<char*>(&directoryOffset), 4);
    magic = std::string(fileMagic, 4);

    // create n-ary tree from descriptor list
    root = new Node("/", false);
    std::vector<Node*> fileStack;
    fileStack.push_back(root);
    fileStream.seekg(directoryOffset, std::ios::beg);
    Node* E1M0 = nullptr;
    int E1M0files = 0;

    for (uint32_t i = 0; i < numDescriptor; ++i) {
        uint32_t offset, length;
        char name[8];
        fileStream.read(reinterpret_cast<char*>(&offset), 4);
        fileStream.read(reinterpret_cast<char*>(&length), 4);
        fileStream.read(name, 8);
        std::string filename(name, 8);
        Node* newNode = nullptr;
        // remove new line
        size_t nullCharPos = filename.find('\0');
        if (nullCharPos != std::string::npos) {
            filename.erase(nullCharPos);
        }
        // adjust node depending on if directory _START/_END/map or file
        if (filename.size() >= 6 && filename.substr(filename.size() - 6) == "_START") {
            newNode = new Node(filename, false, offset, length, fileStack.back());
            fileStack.back()->children.push_back(newNode);
            fileStack.push_back(newNode);
        } 
        else if (filename.size() >= 4 && filename.substr(filename.size() - 4) == "_END") {
            newNode = new Node(filename, false, offset, length, fileStack.back());
            fileStack.back()->children.push_back(newNode);
            fileStack.pop_back();
        } 
        else if (filename.size() == 4 && (filename[0] == 'E' && isdigit(filename[1]) && filename[2] == 'M' && isdigit(filename[3]))) {
            E1M0 = new Node(filename, false, offset, length, fileStack.back());
            fileStack.back()->children.push_back(E1M0);
            fileStack.push_back(E1M0);
            E1M0files = 0;
        } 
        else {
            newNode = new Node(filename, true, offset, length, fileStack.back());
            fileStack.back()->children.push_back(newNode);
            // acts as E1M0 _END once 10 files are reached so that everything is not under E1M0
            if (E1M0 != nullptr) {
                E1M0files += 1;
                if (E1M0files == 10) {
                    fileStack.pop_back();
                    E1M0 = nullptr;
                }
            }
        }
    }
    fileStream.flush();
    fileStream.close();
}

Wad* Wad::loadWad(const std::string &path) {
    return new Wad(path);
}

std::string Wad::getMagic() {
    return this->magic;
}

Node* Wad::dfs(Node* current, const std::vector<std::string>& pathParts, size_t index) {
    if (!current) {
        return nullptr;
    }

    if (index == pathParts.size()) {
        return current;
    }

    for (Node* child : current->children) {
        std::string childName = child->filename;
        // _START and _END are not part of file names, strip if _START, ignore if _END
        if (!child->isFile && childName.size() >= 6 && childName.substr(childName.size() - 6) == "_START") {
            childName = childName.substr(0, childName.size() - 6);
        }
        if (!child->isFile &&childName.size() >= 4 && childName.substr(childName.size() - 4) == "_END") {
            continue;
        }
        if (childName == pathParts[index]) {
            return dfs(child, pathParts, index + 1);
        }
    }

    return nullptr;
}

std::vector<std::string> Wad::split(const std::string &path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

bool Wad::isContent(const std::string &path) {
    // check if last character is "/", return false if true
    if (path.empty() || path[-1] == '/') {
        return false;
    }
    std::vector<std::string> pathParts = split(path);
    Node* targetNode = dfs(root, pathParts, 0);
    return targetNode && targetNode->isFile;
}

bool Wad::isDirectory(const std::string &path) {
    // check if last character is "/", remove if true
    std::string newPath = path;
    if (newPath.empty()) {
        return false;
    }
    if (newPath[-1] == '/') {
        newPath = newPath.erase(newPath.length() - 1);
    }
    std::vector<std::string> pathParts = split(newPath);
    Node* targetNode = dfs(root, pathParts, 0);
    return targetNode && !targetNode->isFile;
}

int Wad::getSize(const std::string &path) {
    std::vector<std::string> pathParts = split(path);
    Node* targetNode = dfs(root, pathParts, 0);
    if (!targetNode || !targetNode->isFile) {
        return -1;
    }
    return targetNode->length;
}

int Wad::getContents(const std::string &path, char *buffer, int length, int offset) {
    fileStream.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
    std::vector<std::string> pathParts = split(path);
    Node* targetNode = dfs(root, pathParts, 0);
    if (!targetNode || !targetNode->isFile) {
        return -1;
    }
    if (offset > targetNode->length) {
        return 0;
    }
    else {
        fileStream.seekg(targetNode->offset + offset, std::ios::beg);
        fileStream.read(buffer, std::min(length, static_cast<int>(targetNode->length) - offset));
    }
    fileStream.flush();
    fileStream.close();
    return std::min(length, static_cast<int>(targetNode->length) - offset);
}

int Wad::getDirectory(const std::string &path, std::vector<std::string> *directory) {
    std::string newPath = path;
    if (newPath.empty()) {
        return -1;
    }
    if (newPath[-1] == '/') {
        newPath = newPath.erase(newPath.length() - 1);
    }  
    std::vector<std::string> pathParts = split(newPath);
    Node* dirNode = dfs(root, pathParts, 0);

    if (!dirNode || dirNode->isFile) {
        return -1;
    }

    for (Node* child : dirNode->children) {
        if (child->filename.size() >= 4 && child->filename.substr(child->filename.size() - 4) == "_END") {
            continue;
        }
        else if (child->filename.size() >= 6 && child->filename.substr(child->filename.size() - 6) == "_START") {
            directory->push_back(child->filename.substr(0, child->filename.size() - 6));
        }
        else {
            directory->push_back(child->filename);
        }
    }

    return directory->size();
}

void Wad::createDirectory(const std::string &path) {
    // update data structure
    // read path backwards until first "/"
    // dfs tree until parent directory's end ex f1_end
    // insert new directory start and end as children of f1_start
    // dfs tree until f1_start and add new directory as child
    // check if parent dir E#M# or dir name too long 
    std::string newPath = path;
    if (!newPath.empty() && newPath.back() == '/') {
        newPath.erase(newPath.length() - 1);
    }
    size_t pos = newPath.find_last_of('/');
    std::string parentDir;
    std::string newDirName;
    if (pos != std::string::npos) {
        parentDir = newPath.substr(0, pos);
        newDirName = newPath.substr(pos + 1);
    } else {
        // root = parent
        parentDir = "/";
        newDirName = newPath;
    }
    // name too long
    if (newDirName.length() > 2) {
        return;
    }
    std::vector<std::string> pathParts = split(parentDir);
    Node* parentNode = dfs(root, pathParts, 0);
    if (!parentNode || (parentNode->filename[0] == 'E' && isdigit(parentNode->filename[1]) && parentNode->filename[2] == 'M' && isdigit(parentNode->filename[3]))) {
        return;
    }
    Node* newDirStart = new Node(newDirName + "_START", false, 0, 0, parentNode);
    Node* newDirEnd = new Node(newDirName + "_END", false, 0, 0, parentNode);
    Node* endNode;
    if (!parentNode->children.empty() && parentNode->children.back()->filename.substr(parentNode->children.back()->filename.length() - 4, parentNode->children.back()->filename.length()) == "_END") {
        Node* endNode = parentNode->children[parentNode->children.size()-1];
        parentNode->children.pop_back();
        parentNode->children.push_back(newDirStart);
        parentNode->children.push_back(newDirEnd);
        parentNode->children.push_back(endNode);
    }
    else {
        parentNode->children.push_back(newDirStart);
        parentNode->children.push_back(newDirEnd);
    }
    // write to wad
    fileStream.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
    fileStream.seekg(directoryOffset, std::ios::beg);
    uint32_t parentEndOffset = 0;
    char descriptor[16];
    for (uint32_t i = 0; i < numDescriptor; ++i) {
        fileStream.read(descriptor, 16);
        std::string descriptorName(descriptor + 8, 8);
        size_t nullPos = descriptorName.find('\0');
        if (nullPos != std::string::npos) descriptorName.erase(nullPos);
        if (parentNode->filename == "/" || (descriptorName == parentNode->filename.substr(0, parentNode->filename.length() - 6) + "_END")) {
            parentEndOffset = directoryOffset + i * 16;
            break;
        }
    }
    fileStream.seekg(parentEndOffset, std::ios::beg);
    std::vector<char> buffer((numDescriptor - ((parentEndOffset - directoryOffset) / 16)) * 16);
    fileStream.read(buffer.data(), buffer.size());

    uint32_t newOffset = 0;
    uint32_t newLength = 0;
    char startDescriptor[16] = {0};
    std::memcpy(startDescriptor, &newOffset, 4);
    std::memcpy(startDescriptor + 4, &newLength, 4);
    std::memcpy(startDescriptor + 8, (newDirName + "_START").c_str(), (newDirName + "_START").size());
    char endDescriptor[16] = {0};
    std::memcpy(endDescriptor, &newOffset, 4);
    std::memcpy(endDescriptor + 4, &newLength, 4);
    std::memcpy(endDescriptor + 8, (newDirName + "_END").c_str(), (newDirName + "_END").size());

    if (parentNode->filename == "/") {
        fileStream.seekp(parentEndOffset, std::ios::beg);
        fileStream.write(buffer.data(), buffer.size());
        fileStream.write(startDescriptor, 16);
        fileStream.write(endDescriptor, 16);
    }
    else {
        fileStream.seekp(parentEndOffset, std::ios::beg);
        fileStream.write(startDescriptor, 16);
        fileStream.write(endDescriptor, 16);
        fileStream.write(buffer.data(), buffer.size());
    }
    // update header
    numDescriptor += 2;
    fileStream.seekp(4, std::ios::beg);
    fileStream.write(reinterpret_cast<const char*>(&numDescriptor), 4);
    fileStream.flush();
    fileStream.close();
}

void Wad::createFile(const std::string &path) {
    if (path.empty() || (path.length() >= 4 && path.substr(path.length() - 4) == "_END") || (path.length() >= 6 && path.substr(path.length() - 6) == "_START")) {
        return;
    }
    size_t pos = path.find_last_of('/');
    std::string parentDir;
    std::string newFileName;
    if (pos != std::string::npos) {
        parentDir = path.substr(0, pos);
        newFileName = path.substr(pos + 1);
    } else {
        // root = parent
        parentDir = "/";
        newFileName = path;
    }
    // name too long
    if (newFileName.length() > 8 || (newFileName[0] == 'E' && isdigit(newFileName[1]) && newFileName[2] == 'M' && isdigit(newFileName[3])) || (parentDir[0] == 'E' && isdigit(parentDir[1]) && parentDir[2] == 'M' && isdigit(parentDir[3]))) {
        return;
    }
    std::vector<std::string> pathParts = split(parentDir);
    Node* parentNode = dfs(root, pathParts, 0);
    if (!parentNode || (parentNode->filename[0] == 'E' && isdigit(parentNode->filename[1]) && parentNode->filename[2] == 'M' && isdigit(parentNode->filename[3]))) {
        return;
    }
    Node* newFile = new Node(newFileName, true, 0, 0, parentNode);
    Node* endNode;
    if (!parentNode->children.empty() && parentNode->children.back()->filename.substr(parentNode->children.back()->filename.length() - 4, parentNode->children.back()->filename.length()) == "_END") {
        Node* endNode = parentNode->children[parentNode->children.size()-1];
        parentNode->children.pop_back();
        parentNode->children.push_back(newFile);
        parentNode->children.push_back(endNode);
    }
    else {
        parentNode->children.push_back(newFile);
    }
    fileStream.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
    // write to wad
    fileStream.seekg(directoryOffset, std::ios::beg);
    uint32_t parentEndOffset = 0;
    char descriptor[16];
    for (uint32_t i = 0; i < numDescriptor; ++i) {
        fileStream.read(descriptor, 16);
        std::string descriptorName(descriptor + 8, 8);
        size_t nullPos = descriptorName.find('\0');
        if (nullPos != std::string::npos) descriptorName.erase(nullPos);
        if (parentNode->filename == "/" || (descriptorName == parentNode->filename.substr(0, parentNode->filename.length() - 6) + "_END")) {
            parentEndOffset = directoryOffset + i * 16;
            break;
        }
    }
    fileStream.seekg(parentEndOffset, std::ios::beg);
    std::vector<char> buffer((numDescriptor - ((parentEndOffset - directoryOffset) / 16)) * 16);
    fileStream.read(buffer.data(), buffer.size());

    uint32_t newOffset = 0;
    uint32_t newLength = 0;
    char newFileDescriptor[16] = {0};
    std::memcpy(newFileDescriptor, &newOffset, 4);
    std::memcpy(newFileDescriptor + 4, &newLength, 4);
    std::memcpy(newFileDescriptor + 8, (newFileName).c_str(), (newFileName).size());

    if (parentNode->filename == "/") {
        fileStream.seekp(parentEndOffset, std::ios::beg);
        fileStream.write(buffer.data(), buffer.size());
        fileStream.write(newFileDescriptor, 16);
    }
    else {
        fileStream.seekp(parentEndOffset, std::ios::beg);
        fileStream.write(newFileDescriptor, 16);
        fileStream.write(buffer.data(), buffer.size());
    }
    // update header
    numDescriptor += 1;
    fileStream.seekp(4, std::ios::beg);
    fileStream.write(reinterpret_cast<const char*>(&numDescriptor), 4);
    fileStream.flush();
    fileStream.close();
}

int Wad::writeToFile(const std::string &path, const char *buffer, int length, int offset) { 
    if (!isContent(path)) {
        return -1;
    }
    std::vector<std::string> pathParts = split(path);
    Node* targetNode = dfs(root, pathParts, 0);
    if (targetNode->length > 0) {
        return 0;
    }
    // write to end of lump data
    fileStream.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
    fileStream.seekp(0, std::ios::end);
    uint32_t lumpEnd = static_cast<uint32_t>(fileStream.tellp());
    fileStream.write(buffer, length);
    // update node;
    targetNode->offset = lumpEnd;
    targetNode->length = length;
    // shift descriptor list
    uint32_t newDirectoryOffset = static_cast<uint32_t>(fileStream.tellp());
    uint32_t descriptorSize = numDescriptor * 16;
    if (newDirectoryOffset + descriptorSize > directoryOffset) {
        char *buffer = new char[descriptorSize];
        fileStream.seekg(directoryOffset, std::ios::beg);
        fileStream.read(buffer, descriptorSize);
        fileStream.seekp(newDirectoryOffset, std::ios::beg);
        fileStream.write(buffer, descriptorSize);
        delete[] buffer;
        directoryOffset = newDirectoryOffset;
    }
    // update descriptor
    char descriptor[16] = {0};
    fileStream.seekg(directoryOffset, std::ios::beg);
    for (uint32_t i = 0; i < numDescriptor; ++i) {
        fileStream.read(descriptor, 16);
        std::string descriptorName(descriptor + 8, 8);
        size_t nullPos = descriptorName.find('\0');
        if (nullPos != std::string::npos) {
            descriptorName.erase(nullPos);
        }
        if (descriptorName == targetNode->filename) {
            fileStream.seekp(directoryOffset + (i * 16), std::ios::beg);
            fileStream.write(reinterpret_cast<const char*>(&lumpEnd), 4);
            fileStream.write(reinterpret_cast<const char*>(&length), 4);
            fileStream.flush();
            break;
        }
    }
    // update header
    fileStream.seekp(8, std::ios::beg);
    fileStream.write(reinterpret_cast<const char*>(&directoryOffset), 4);
    fileStream.flush();
    fileStream.close();
    return length;
}

