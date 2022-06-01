#ifndef MODULES_DROPBOX_H
#define MODULES_DROPBOX_H

#include <string>
#include <vector>
#include <map>

#include "../utils/curl.h"

typedef struct ListResult {
    std::string name;
    std::string path_display;
} ListResult;

class Dropbox{
    public:
        Dropbox(std::string token);
        ~Dropbox(){};
        void upload(std::map<std::pair<std::string, std::string>, std::vector<std::string>> paths);
        std::vector<ListResult> list(std::string path);
        void download(std::string path, std::string destPath);
    private:
        std::string _token;
        Curl _curl;
};


#endif
