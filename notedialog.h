#ifndef NOTEDIALOG_H
#define NOTEDIALOG_H

#include <QtWidgets>
#include "noteedit.h"
class Note;

class NoteDialog : public QWidget
{
    Q_OBJECT

    public:
        NoteDialog(Note *note = 0);
        QString content() const;
        QString title() const;
        Note * note();
        void setNote(Note* note);
        void setFocus();
        void update();

    protected:
        void changeEvent(QEvent *event);
        void closeEvent(QCloseEvent *event);
        void resizeEvent(QResizeEvent* event);
        virtual void attachToNote();

    signals:
        void newNote(Note *);
        void deletionRequested(Note *);

    protected slots:
        void infos();
        void deleteMe();
        bool save();

    protected:
        NoteEdit *m_noteEdit;
        QSettings *m_settings;
        Note *m_note;
        QAction *m_actionDelete;
};

#endif // NOTEDIALOG_H
