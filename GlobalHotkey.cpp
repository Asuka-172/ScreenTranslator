#include "GlobalHotkey.h"

#include <windows.h>

namespace {

// 将 QKeySequence 的首个组合键转换为 RegisterHotKey 的 modifiers + vk
bool toHotkey(const QKeySequence& seq, UINT& modifiers, UINT& vk)
{
    if (seq.isEmpty())
        return false;

    const QKeyCombination combo = seq[0];
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    const int key = combo.key();

    modifiers = 0;
    if (mods & Qt::ControlModifier) modifiers |= MOD_CONTROL;
    if (mods & Qt::AltModifier)     modifiers |= MOD_ALT;
    if (mods & Qt::ShiftModifier)   modifiers |= MOD_SHIFT;
    if (mods & Qt::MetaModifier)    modifiers |= MOD_WIN;

    // 可打印 ASCII 字符，Qt::Key 值与 Windows VK 一致
    if (key >= 0x20 && key <= 0x7E) {
        vk = static_cast<UINT>(key);
        return true;
    }

    // F1-F24
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        vk = VK_F1 + static_cast<UINT>(key - Qt::Key_F1);
        return true;
    }

    switch (key) {
    case Qt::Key_Space:     vk = VK_SPACE;     return true;
    case Qt::Key_Tab:       vk = VK_TAB;       return true;
    case Qt::Key_Backspace: vk = VK_BACK;      return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:     vk = VK_RETURN;    return true;
    case Qt::Key_Escape:    vk = VK_ESCAPE;    return true;
    case Qt::Key_Left:      vk = VK_LEFT;      return true;
    case Qt::Key_Right:     vk = VK_RIGHT;     return true;
    case Qt::Key_Up:        vk = VK_UP;        return true;
    case Qt::Key_Down:      vk = VK_DOWN;      return true;
    case Qt::Key_Home:      vk = VK_HOME;      return true;
    case Qt::Key_End:       vk = VK_END;       return true;
    case Qt::Key_PageUp:    vk = VK_PRIOR;     return true;
    case Qt::Key_PageDown:  vk = VK_NEXT;      return true;
    case Qt::Key_Insert:    vk = VK_INSERT;    return true;
    case Qt::Key_Delete:    vk = VK_DELETE;    return true;
    default: return false;
    }
}

} // namespace

GlobalHotkey::GlobalHotkey(QObject* parent)
    : QObject(parent)
{
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterAll();
}

bool GlobalHotkey::registerHotkey(int id, const QKeySequence& seq)
{
    unregisterHotkey(id);
    if (seq.isEmpty())
        return false;

    UINT modifiers = 0;
    UINT vk = 0;
    if (!toHotkey(seq, modifiers, vk))
        return false;

    if (RegisterHotKey(nullptr, id, modifiers, vk)) {
        m_registered.insert(id);
        return true;
    }
    return false;
}

void GlobalHotkey::unregisterHotkey(int id)
{
    if (m_registered.contains(id)) {
        UnregisterHotKey(nullptr, id);
        m_registered.remove(id);
    }
}

void GlobalHotkey::unregisterAll()
{
    for (int id : m_registered)
        UnregisterHotKey(nullptr, id);
    m_registered.clear();
}

bool GlobalHotkey::isRegistered(int id) const
{
    return m_registered.contains(id);
}

bool GlobalHotkey::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);

    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        const int id = static_cast<int>(msg->wParam);
        if (m_registered.contains(id)) {
            emit activated(id);
            return true;
        }
    }
    return false;
}
