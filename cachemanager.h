#ifndef IMAGECACHEMANAGER_H
#define IMAGECACHEMANAGER_H
#include <QString>
#include <QImage>
#include <QFileInfo>
#include <map>
#include <memory>
#include <QFileSystemWatcher>
#include <QTimer>
#include <vector>
#include <QSvgRenderer>

enum FileLoadStatus{
    ok,
    failed
};

template <class T>
struct DataEntry{
    std::shared_ptr<T> data;
    FileLoadStatus status;
};

template <class T>
class CacheManager
{
public:    
    DataEntry<T> getData(QString path);
    void setData(QString path, std::shared_ptr<T> data);
    void setCallback(std::function<void(QString)> dataChangedCallback);
    static CacheManager<T>& instance ();

private:
    CacheManager();
    CacheManager(CacheManager const&) = delete;
    void updateFile(QString const &path);
    void deleteFile(QString const &path);
    void removeFailed(QString path);
    void addFailedToWatcher(QFileInfo file);
    void deleteAllResources();

    std::map<QString, DataEntry<T>> mCachedData;
    QFileSystemWatcher *mWatcher;
    QString mLastPath;
    QString mLastPathDir;
    std::function<void(QString)> mDataChangedCallback;
    QTimer mFileTimer;
    QTimer mDirTimer;
};

#endif // IMAGECACHEMANAGER_H

