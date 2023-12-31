#include <stdio.h>
#include <malloc.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>

#include <sys/stat.h>
#include <openssl/sha.h>

#include "dropbox.h"

std::string getUpdateTimestampForCitraSave(std::string dropboxToken, std::string fullPath) {
    Dropbox dropbox(dropboxToken);
    auto results = dropbox.list_folder(fullPath + "/data/00000001");
    for (auto lr : results) {
        if (lr.name == "user1") {
            return lr.server_modified;
        }
    }
    return "";
}

std::string checkpointDirToCitraGameCode(std::string checkpointGameSaveDir) {
    if (checkpointGameSaveDir.rfind("0x", 0) == 0) {
        std::string gameCode = "0" + checkpointGameSaveDir.substr(2,5) + "00";
        for (auto &elem : gameCode) {
            elem = std::tolower(elem);
        }
        return gameCode;
    }
    return "";
}

std::map<std::string, std::string> findCheckpointSaves(std::string checkpointPath) {
    std::map<std::string, std::string> pathmap;

    std::string path(checkpointPath + "/saves");
    DIR *dir;
    struct dirent *ent;
    if((dir = opendir(path.c_str())) != NULL){
        while((ent = readdir(dir)) != NULL){
            std::string dirname = ent->d_name;

            if (dirname == "." || dirname == "..") {
                continue;
            }

            std::string readpath(path + "/" + dirname);

            std::string gameCode = checkpointDirToCitraGameCode(dirname);
            if (gameCode != "") {
                pathmap[gameCode] = dirname;
            } else {
                std::cout << "Invalid game directory, skipping: " << dirname << std::endl;
            }
        }
    }

    return pathmap;
}

void downloadCitraSaveToCheckpoint(std::string dropboxToken, std::string timestamp, std::string checkpointPath, std::map<std::string, std::string> pathmap, std::string gameCode, std::string fullPath) {
    Dropbox dropbox(dropboxToken);

    std::string baseSaveDir = checkpointPath + "/saves";

    if (!pathmap.count(gameCode)) {
        std::cout << "Game code " << gameCode << " not found in local Checkpoint saves, skipping";
        return;
    }

    std::string gameDirname = pathmap[gameCode];
    std::string gameSaveDir = baseSaveDir + "/" + gameDirname;
    // TODO: Currently only the latest Citra save is retained per game. 
    //       Could append timestamp if wanted, but adding raw timestamp caused segfaults on 3DS.
    //       Investigate further if want to keep older saves.
    // std::string destPath = gameSaveDir + "/Citra_" + timestamp;
    std::string destPath = gameSaveDir + "/Citra";

    std::cout << "Citra save found for " << gameDirname << " with timestamp: " << timestamp << std::endl;

    struct stat info;

    if (stat(destPath.c_str(), &info) != 0) {
        std::cout << "Creating dir: " << destPath << std::endl;
        int status = mkdir(destPath.c_str(), 0777);
        if (status != 0) {
            std::cout << "Failed to create Checkpoint save dir " << destPath << ", skipping" << std::endl;
            return;
        }
    } else if (info.st_mode & !S_IFDIR) {
        std::cout << "File already exists at Checkpoint save dir, delete the file and try again:\n" << destPath << std::endl;
    } else {
        std::cout << "Checkpoint save dir already exists: " << destPath << std::endl;
    }

    dropbox.download(fullPath + "/data/00000001/system",  destPath + "/system");
    dropbox.download(fullPath + "/data/00000001/user1", destPath + "/user1");
}

void downloadCitraSaves(std::string dropboxToken, std::string checkpointPath) {
    auto pathmap = findCheckpointSaves(checkpointPath);

    Dropbox dropbox(dropboxToken);
    auto folder = dropbox.list_folder("/sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000/title/00040000");
    for (auto lr : folder) {
        std::string timestamp = getUpdateTimestampForCitraSave(dropboxToken, lr.path_display);
        if (timestamp == "") {
            std::cout << lr.name << ": Could not find timestamp, skipping" << std::endl; 
            continue;
        }

        downloadCitraSaveToCheckpoint(
            dropboxToken,
            timestamp,
            checkpointPath,
            pathmap,
            lr.name,
            lr.path_display
        );
    }
}

// Chat-GPT code

std::string calculateSHA1(const std::string &filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    SHA_CTX shaContext;
    SHA1_Init(&shaContext);

    char buffer[BUFSIZ];
    while (file.read(buffer, sizeof(buffer)).gcount() > 0) {
        SHA1_Update(&shaContext, buffer, file.gcount());
    }

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final(hash, &shaContext);

    std::string result;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        result += (char)(hash[i]);
    }

    file.close();
    return result;
}

std::map<std::string, std::string> findROMsAndHashes(const std::string& romsFolderPath) {
    std::map<std::string, std::string> romsAndHashes;

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(romsFolderPath.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string filename = ent->d_name;
            if (filename.length() >= 4 && filename.substr(filename.length() - 4) == ".nds") {
                std::string filepath = romsFolderPath + "/" + filename;

                std::string hash = calculateSHA1(filepath);
                if (!hash.empty()) {
                    romsAndHashes[filename] = hash;
                } else {
                    std::cout << "Failed to calculate hash for: " << filename << std::endl;
                }
            }
        }
        closedir(dir);
    } else {
        std::cout << "Could not open directory: " << romsFolderPath << std::endl;
    }

    return romsAndHashes;
}

void downloadDeltaSaves(std::string dropboxToken, std::string romsPath) {
    auto pathmap = findROMSaves(romsPath);

    Dropbox dropbox(dropboxToken);
    auto folder = dropbox.list_folder("/Delta Emulator");
    for (auto lr : folder) {
        if (!lr.is_file) {
            continue; // Skip directories
        }

        std::string filename = lr.name;
        // Check if the file follows the expected naming convention
        if (filename.substr(0, 9) != "GameSave-" || filename.substr(filename.length() - 9) != "-gameSave") {
            //std::cout << lr.name << ": Invalid file name, skipping" << std::endl;
            continue;
        }

        // Extract the SHA-1 hash from the filename
        std::string hash = filename.substr(9, filename.length() - 18);

        // Assuming the hash corresponds to a game in your ROMs path
        auto it = pathmap.find(hash);
        if (it == pathmap.end()) {
            std::cout << lr.name << ": No corresponding ROM found, skipping" << std::endl;
            continue;
        }

        std::string romName = it->second; // ROM name based on hash
        std::string savePath = romsPath + "/" + romName + ".sav"; // Path to save in ROMs directory

        // Download the save file to the ROMs directory
        bool downloadSuccess = dropbox.download_file(lr.path_display, savePath);

        if (downloadSuccess) {
            std::cout << lr.name << ": Downloaded to " << savePath << std::endl;
        } else {
            std::cout << lr.name << ": Download failed" << std::endl;
        }
    }
}
