#ifndef DICTIONARYSERVICE_H
#define DICTIONARYSERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// 免费词典查询（Free Dictionary API）。
class DictionaryService : public QObject
{
    Q_OBJECT
public:
    explicit DictionaryService(QObject* parent = nullptr);

    void lookup(const QString& word);

signals:
    void resultReady(const QString& word, const QString& formatted);
    void error(const QString& message);

private:
    QNetworkAccessManager* m_manager;
};

#endif
