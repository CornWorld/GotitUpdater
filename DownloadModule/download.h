//
// Created by CornWorld on 2022/3/16.
//

#ifndef GOTITUPDATER_DOWNLOAD_H
#define GOTITUPDATER_DOWNLOAD_H

#include <curl/curl.h>

#include <cstdlib>
#include <string>
#include <functional>
#include <thread>
#include <mutex>

namespace GotitUpdater {
    inline namespace DownloadModule {
        using std::string;
        using std::pair;
        using std::mutex;

        typedef unsigned long long ull;

        typedef size_t (*WriteFunc)(void *ptr, ull size, void **customData) ;

        struct DownloadBuffer {
        public:
            virtual bool allocate(ull allocate_size)=0;
            virtual size_t write(void *origin, ull write_size, ull pos)=0;
            virtual char *read(ull pos, ull read_size) const=0;
        };

        struct DownloadBufferString : DownloadBuffer {
        public:
            char *buf;
            mutex mtx;
            ull allocated,size;

            explicit DownloadBufferString();
            void setDefault();

            bool allocate(ull allocate_size) override;
            size_t write(void *origin, ull write_size, ull pos) override;
            char *read(ull pos, ull read_size) const override;
        };

        struct DownloadBufferFile : DownloadBuffer {
            FILE *buf;
            mutex mtx;
            ull size;
            char const *path, *mode;

            explicit DownloadBufferFile();
            void setDefault();

            void setFile(const char *filepath, const char *filemode);
            bool allocate(ull size) override;
            size_t write(void *origin, ull write_size, ull pos) override;
            char* read(ull pos, ull read_size) const override;
        };


        struct DownloadSubtask {
            CURL *curl;
            pair<ull,ull> range;
            std::thread *thread;
        };

        struct DownloadTask {
        public:
            void setDefault(bool init=false);

            explicit DownloadTask();
            explicit DownloadTask(const string & url, DownloadBuffer *buf);

            void setUrl(const string &origin);
            void setThreadNum(int threadNum);
            void setBuffer(DownloadBuffer *buf);

            string url;
            ull size;
            int status;
#define DOWNLOAD_STATUS_INIT 0
#define DOWNLOAD_STATUS_READY 1
#define DOWNLOAD_STATUS_PROCESS 2
#define DOWNLOAD_STATUS_FINISHED 3
#define DOWNLOAD_STATUS_ERROR (-1)

            int threadNum;
            int workingNum;

            CURL* shareHandle;

            DownloadSubtask *subtasks;

            DownloadBuffer* buffer;
        };


        class Download {
        public:

            bool runDownloadTask(DownloadTask *task);

            static CURL* getHeaderFromUrl(const string &url);

        private:


            static void workThread(DownloadTask *task, int subtaskId);

        };
    }
}




#endif //GOTITUPDATER_DOWNLOAD_H
