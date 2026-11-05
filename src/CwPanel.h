#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QToolButton;

// Row under the entry line: CQ / S&P mode, CW speed, F1..F12 message keys.
class CwPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CwPanel(QWidget *parent = nullptr);

    void setMessages(const QStringList &messages);   // raw (unexpanded) texts, F1..F12 used
    void setSpeed(int wpm, bool available);
    void setCqMode(bool cq);
    void setAutoCqActive(bool on);
    void flashKey(int index);

protected:
    void resizeEvent(QResizeEvent *e) override;

signals:
    void keyClicked(int index);                       // 0..11
    void modifyRequested(int index);                  // right-click > Modify message
    void autoCqDelayRequested();                      // right-click on F12
    void modeToggled();
    void speedStep(int delta);                        // +1 / -1

private:
    void updateKeyTexts();

    QStringList m_messages;
    QPushButton *m_mode;
    QToolButton *m_up, *m_down;
    QLabel *m_speed;
    QList<QPushButton *> m_keys;
};
