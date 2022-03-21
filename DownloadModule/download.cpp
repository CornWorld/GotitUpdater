//
// Created by CornWorld on 2022/3/16.
//

#include "download.h"

#include <curl/curl.h>

#include <functional>
#include <thread>
#include <cstdio>
#include <cstring>


GotitUpdater::DownloadModule::DownloadTask::DownloadTask() {
    setDefault(true);
}

GotitUpdater::DownloadModule::DownloadTask::DownloadTask(const std::string &url, GotitUpdater::DownloadBuffer *buf) {
    setDefault(true);
    setUrl(url);
}

void GotitUpdater::DownloadModule::DownloadTask::setUrl(const std::string &origin) {
    url=origin;
}

void GotitUpdater::DownloadModule::DownloadTask::setThreadNum(int num) {
    threadNum=num;
}

void GotitUpdater::DownloadModule::DownloadTask::setBuffer(GotitUpdater::DownloadBuffer *buf) {
    buffer=buf;
}

void GotitUpdater::DownloadModule::DownloadTask::setDefault(bool init) {
    if(!init) {
        // TODO
    }
    buffer=nullptr;
    shareHandle=nullptr;
    status=DOWNLOAD_STATUS_INIT;
    threadNum=1;
    size=0;
    workingNum=0;
}

CURL *GotitUpdater::DownloadModule::Download::getHeaderFromUrl(const std::string &url) {
    CURL *handle=curl_easy_init();

    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_NOBODY, true);
    curl_easy_setopt(handle, CURLOPT_HEADER, true);
    curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "GET");

    if(curl_easy_perform(handle)==CURLE_OK) {
        return handle;
    }
    return nullptr;
}

bool GotitUpdater::DownloadModule::Download::runDownloadTask(GotitUpdater::DownloadTask *task) {
    using std::make_pair;
    bool ret=false;

    if(task->status==DOWNLOAD_STATUS_INIT) {
        if(!task->url.empty() && task->buffer!=nullptr) task->status=DOWNLOAD_STATUS_READY;
    }
    if(task->status==DOWNLOAD_STATUS_READY) {
        task->status=DOWNLOAD_STATUS_PROCESS;

        CURL* header=getHeaderFromUrl(task->url);
        if(!task->size) {
            curl_off_t filesize;
            curl_easy_getinfo(header, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &filesize);
            if(filesize!=-1) task->size=(ull)filesize;
        }
        task->buffer->allocate(task->size);

        task->shareHandle=curl_share_init();
        curl_share_setopt(task->shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);

        task->subtasks=new DownloadSubtask[task->threadNum];
        DownloadSubtask *subtasks=task->subtasks;

        unsigned subtaskSize=task->size/task->threadNum;
        unsigned surplus=task->size%task->threadNum;

        for(int i=0;i<task->threadNum;i++) {
            subtasks[i].range=make_pair(subtaskSize*i,subtaskSize*(i+1)-1);
        }
        subtasks[task->threadNum-1].range.second+=surplus;
////      DEBUG: print the range of all subtasks
//        for(int i=0;i<task->threadNum;i++) {
//            ull x=task->subtasks[i].range.first,y=task->subtasks[i].range.second;
//            printf("i:%d f:%llu s:%llu sz:%llu\n",i,x,y,y-x+1);
//        }

        for(int i=0;i<task->threadNum;i++) {
            subtasks[i].thread=new std::thread(workThread, task, i);
        }

        for(int i=0;i<task->threadNum;i++) {
            subtasks[i].thread->join();
        }

        delete[] subtasks;
        curl_share_cleanup(task->shareHandle);
        task->status=DOWNLOAD_STATUS_FINISHED;
    }


    return ret;
}

void GotitUpdater::DownloadModule::Download::workThread(GotitUpdater::DownloadTask *task, int subtaskId) {
    using std::make_pair;
    using std::pair;

    task->workingNum+=1;

    DownloadSubtask subtask=task->subtasks[subtaskId];

    subtask.curl=curl_easy_init();
    curl_easy_setopt(subtask.curl, CURLOPT_DNS_CACHE_TIMEOUT, 60*5);
    curl_easy_setopt(subtask.curl, CURLOPT_SHARE, task->shareHandle);
    curl_easy_setopt(subtask.curl, CURLOPT_URL, task->url.c_str());

    curl_easy_setopt(subtask.curl, CURLOPT_NOSIGNAL, true);

    pair<DownloadTask*, int> write_arg(task, subtaskId);
    curl_easy_setopt(subtask.curl, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&write_arg));

    curl_write_callback write_callback=[](char *ptr, size_t size, size_t nmemb, void *userdata) ->size_t {
        auto *arg=reinterpret_cast<pair<DownloadTask*, int>* >(userdata);
        DownloadTask *task=arg->first;
        ull &left_range=task->subtasks[arg->second].range.first;
        ull offset=left_range;
        left_range+=size*nmemb;

        return task->buffer->write(ptr, size*nmemb, offset);
    };

    curl_easy_setopt(subtask.curl, CURLOPT_WRITEFUNCTION, write_callback);

    char *range=new char[19*2+3];
    sprintf(range,"%llu-%llu", subtask.range.first, subtask.range.second);
    curl_easy_setopt(subtask.curl, CURLOPT_RANGE, range);

    if(curl_easy_perform(subtask.curl)!=CURLE_OK) {
        fprintf(stderr, "subtask %d: Download failed.\n", subtaskId);
    }

    delete[] range;
    task->workingNum-=1;
}


GotitUpdater::DownloadModule::DownloadBufferString::DownloadBufferString() {
    setDefault();
}

void GotitUpdater::DownloadModule::DownloadBufferString::setDefault() {
    buf=nullptr;
    allocated=0;
    size=0;
}

bool GotitUpdater::DownloadModule::DownloadBufferString::allocate(GotitUpdater::ull allocate_size) {
    if(allocated) {
        allocated=0;
        delete[] buf;
    }
    buf=new char[allocate_size];
    if(buf==nullptr) return false;
    else {
        allocated=allocate_size;
        return true;
    }
}

size_t GotitUpdater::DownloadModule::DownloadBufferString::write(void *origin, GotitUpdater::ull write_size, GotitUpdater::ull pos) {
    if(allocated<size+write_size) return 0;
    else {
        while(mtx.try_lock());
        if(pos==0) {
            memcpy(buf+size, origin, write_size);
        }
        else {
            memcpy(buf+pos, origin, write_size);
        }
        size+=write_size;
        mtx.unlock();
        return write_size;
    }
}

char *GotitUpdater::DownloadModule::DownloadBufferString::read(GotitUpdater::ull pos, GotitUpdater::ull read_size) const {
    if(size<pos+read_size) return nullptr;
    else {
        return (buf+pos);
    }
}


GotitUpdater::DownloadModule::DownloadBufferFile::DownloadBufferFile() {
    setDefault();
}

void GotitUpdater::DownloadModule::DownloadBufferFile::setDefault() {
    buf=nullptr;
    size=0;
}

void GotitUpdater::DownloadModule::DownloadBufferFile::setFile(const char *filepath, const char *filemode) {
    path=filepath, mode=filemode;
}

bool GotitUpdater::DownloadModule::DownloadBufferFile::allocate(GotitUpdater::ull allocate_size) {
    if(size) {
        fclose(buf);
        buf=nullptr;
    }
    buf=fopen(path, mode);
    return !fseek(buf, (long)allocate_size, SEEK_SET);
}

size_t GotitUpdater::DownloadModule::DownloadBufferFile::write(void *origin, GotitUpdater::ull write_size, ull pos) {
    while(mtx.try_lock());
    fseek(buf, (long)pos, SEEK_SET);
    fwrite(origin, write_size, 1, buf);
    size+=write_size;
    mtx.unlock();
    return write_size;
}

char* GotitUpdater::DownloadModule::DownloadBufferFile::read(GotitUpdater::ull pos, GotitUpdater::ull read_size) const {
    if(size<pos+read_size) return nullptr;
    else {
        char *ret=new char[read_size];
        fseek(buf, (long)pos, SEEK_SET);
        fread(ret, read_size, 1, buf);
        return ret;
    }
}

