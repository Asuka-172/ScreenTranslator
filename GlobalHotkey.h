#ifndef GLOBALHOTKEY_H
#define GLOBALHOTKEY_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QSet>

// 系统级全局热键封装（Windows RegisterHotKey），窗口失焦时仍可触发。
class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;

    bool registerHotkey(int id, const QKeySequence& seq);
    void unregisterHotkey(int id);
    void unregisterAll();
    bool isRegistered(int id) const;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void activated(int id);

private:
    QSet<int> m_registered;
};

#endif
