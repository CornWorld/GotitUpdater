#include "DownloadModule/download.h"

#include <cstdio>
#include <iostream>
#include <cstring>

int main() {
    GotitUpdater::DownloadTask task;
    GotitUpdater::DownloadBufferFile bs;

    task.setBuffer(&bs);
    task.setUrl("https://df719767a4c60685098e5e29c62d14d8.dlied1.cdntips.net/dl.softmgr.qq.com/original/Development/VSCodeUserSetup-x64-1.62.3.exe?mkey=62381c1b70eae88d&f=0000&cip=112.234.206.120&proto=https");
    task.setThreadNum(16);

    bs.setFile("VSCodeUserSetup-x64-1.62.3.exe","w+");

    GotitUpdater::Download handle;

    handle.runDownloadTask(&task);

//    printf("%llu %s\n",bs.size,bs.read(0,bs.size));

    printf("\nDone");
    return 0;
}
