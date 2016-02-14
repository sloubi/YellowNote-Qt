/**
 * Projet créé le 30 décembre 2015
 */

#include <QDebug>
#include "mainwindow.h"
#include "connexiondialog.h"

MainWindow::MainWindow(bool *hotkeyLoop) : QMainWindow()
{
    setWindowTitle("YellowNote");

    m_listWidget = new QListWidget();

    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addWidget(m_listWidget);

    QWidget *central = new QWidget;
    central->setLayout(mainLayout);
    setCentralWidget(central);

    QObject::connect(m_listWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(openEditNoteDialog(QListWidgetItem*)));

    createMenus();

    initialize();
    m_hotkeyLoop = hotkeyLoop;
}

void MainWindow::createMenus()
{
    QAction *actionQuit = new QAction("&Quitter", this);
    actionQuit->setShortcut(QKeySequence("Ctrl+Q"));
    connect(actionQuit, SIGNAL(triggered()), this, SLOT(close()));

    QAction *actionNew = new QAction("&Nouvelle note", this);
    actionNew->setIcon(QIcon(":/note/add"));
    actionNew->setIconText("Nouvelle Note");
    actionNew->setShortcut(QKeySequence("Ctrl+N"));
    connect(actionNew, SIGNAL(triggered()), this, SLOT(openNoteDialog()));

    QAction *actionDelete = new QAction("&Supprimer la note", this);
    actionDelete->setIcon(QIcon(":/note/delete"));
    actionDelete->setIconText("Supprimer la note");
    actionDelete->setShortcut(QKeySequence("Del"));
    connect(actionDelete, SIGNAL(triggered()), this, SLOT(deleteNote()));

    QAction *actionSync = new QAction("&Synchroniser", this);
    actionSync->setIcon(QIcon(":/note/sync"));
    actionSync->setIconText("Synchroniser");
    actionSync->setShortcut(QKeySequence("Ctrl+S"));
    connect(actionSync, SIGNAL(triggered()), this, SLOT(sync()));

    QAction *actionAbout = new QAction("&À propos de YellowNote", this);
    connect(actionAbout, SIGNAL(triggered()), this, SLOT(about()));

    QMenu *menuNotes = menuBar()->addMenu("&Notes");
    menuNotes->addAction(actionNew);
    menuNotes->addAction(actionDelete);
    menuNotes->addAction(actionSync);
    menuNotes->addAction(actionQuit);

    QMenu *menuHelp = menuBar()->addMenu("&Aide");
    menuHelp->addAction(actionAbout);

    QToolBar *toolBar = addToolBar("Toolbar");
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar->addAction(actionNew);
    toolBar->addAction(actionDelete);
    toolBar->addAction(actionSync);
}

void MainWindow::initialize()
{
    // Initialise la connexion à la base de données
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("notes.db");

    QList<Note> notes = Note::loadFromDb();
    if ( ! notes.empty())
    {
        for (int i = 0; i < notes.size(); ++i)
        {
            Note *note = new Note();
            *note = notes.at(i);
            addNoteLabel(note);
        }
    }

    // Récupération de la configuration du serveur oAuth2
    m_o2InternalSettings = new QSettings(":/config/oauth.ini", QSettings::IniFormat);

    // Paramétrage pour les appels à l'API
    m_oauth2 = new Oauth2();
    m_oauth2->setClientId(m_o2InternalSettings->value("client_id").toString());
    m_oauth2->setClientSecret(m_o2InternalSettings->value("client_secret").toString());
    m_oauth2->setGrantFlow(O2::GrantFlowResourceOwnerPasswordCredentials);
    m_oauth2->setTokenUrl(m_o2InternalSettings->value("token_url").toString());
    m_oauth2->setRefreshTokenUrl(m_o2InternalSettings->value("refresh_token_url").toString());

    QSettings *o2Settings = new QSettings("Sloubi", "oAuth2");
    O2SettingsStore *settingsStore = new O2SettingsStore(o2Settings, m_o2InternalSettings->value("encryption_key").toString());
    // Set the store before starting OAuth, i.e before calling link()
    m_oauth2->setStore(settingsStore);
}

void MainWindow::openNoteDialog()
{
    NoteDialog *dialog = new NoteDialog();
    dialog->show();

    QObject::connect(dialog, SIGNAL(saved(NoteDialog*)), this, SLOT(saveNoteFromDialog(NoteDialog*)));
}

void MainWindow::openEditNoteDialog(QListWidgetItem *item)
{
    NoteListWidgetItem *noteItem = static_cast<NoteListWidgetItem*>(item);

    NoteDialog *dialog = new NoteDialog(noteItem->note());
    dialog->setItemRow(m_listWidget->row(noteItem));
    dialog->show();

    QObject::connect(dialog, SIGNAL(saved(NoteDialog*)), this, SLOT(saveNoteFromDialog(NoteDialog*)));
}

void MainWindow::addNoteLabel(Note *note)
{
    // Création du label
    NoteLabel *label = new NoteLabel(note->title(), note->content(), note->updatedAt());

    // Création de l'item de la liste, ajout à la liste, assignation du label
    NoteListWidgetItem *item = new NoteListWidgetItem(m_listWidget);
    item->setSizeHint(label->minimumSizeHint());
    m_listWidget->addItem(item);
    m_listWidget->setItemWidget(item, label);

    // Enregistrement de la position de la note dans la liste
    m_sharedkeyRows[note->sharedKey()] = m_listWidget->row(item);

    // Rattachement de la note à l'item
    item->setNote(note);
}

void MainWindow::saveNoteFromDialog(NoteDialog *noteDialog)
{
    // Si c'est un ajout
    if (noteDialog->itemRow() == -1)
    {
        addNoteFromDialog(noteDialog);
    }

    // Modification
    else
    {
        editNoteFromDialog(noteDialog);
    }
}

void MainWindow::addNoteFromDialog(NoteDialog *noteDialog)
{
    // On ne crée pas la note si elle n'a pas encore de contenu
    if (noteDialog->title().isEmpty() && noteDialog->content().isEmpty())
        return;

    Note *note = new Note(noteDialog->title(), noteDialog->content());
    note->setUpdatedAt(SqlUtils::date(QDateTime::currentDateTime()));
    addNoteLabel(note);
    note->addToDb();

    // On rattache la NoteListWidgetItem à noteDialog
    noteDialog->setItemRow(m_sharedkeyRows[note->sharedKey()]);
}

void MainWindow::editNoteFromDialog(NoteDialog *noteDialog)
{
    NoteLabel *label = new NoteLabel(noteDialog->title(), noteDialog->content(), QDateTime::currentDateTime());

    QListWidgetItem *item = m_listWidget->item(noteDialog->itemRow());
    NoteListWidgetItem *noteItem = static_cast<NoteListWidgetItem*>(item);
    noteItem->setSizeHint(label->minimumSizeHint());
    m_listWidget->setItemWidget(item, label);

    noteItem->note()->setTitle(noteDialog->title());
    noteItem->note()->setContent(noteDialog->content());

    noteItem->note()->editInDb();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // On sort de la boucle infinie des raccourcis globaux
    *m_hotkeyLoop = false;

    // On laisse la fenêtre se fermer
    event->accept();
}

void MainWindow::close()
{
    // On sort de la boucle infinie des raccourcis globaux
    *m_hotkeyLoop = false;
    qApp->quit();
}

void MainWindow::deleteNote()
{
    QList<QListWidgetItem*> selected = m_listWidget->selectedItems();
    if ( ! selected.empty())
    {
        QListWidgetItem *item = selected.first();
        NoteListWidgetItem *noteItem = static_cast<NoteListWidgetItem*>(item);
        noteItem->note()->setDeleteInDb();

        m_listWidget->takeItem(m_listWidget->row(item));
    }
}

void MainWindow::about()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("À propos de YellowNote");

    QLabel *image = new QLabel(dialog);
    image->setPixmap(QPixmap(":/note/logo"));

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec("SELECT sqlite_version()");
    q.next();
    QString sqliteVersion = q.value(0).toString();

    QDate builtDate = QLocale(QLocale::English).toDate(QString(__DATE__).simplified(), "MMM d yyyy");

    QString aboutString;
    aboutString += "<b>YellowNote 0.1</b><br>";
    aboutString += "Par Sloubi, <a href='http://sloubi.eu'>sloubi.eu</a><br><br>";
    aboutString += "<font color='#5C5C5C'>Compilé le " + builtDate.toString("dd/MM/yyyy") + " à " + QString(__TIME__) + "<br>";
    aboutString += "Qt " + QString(QT_VERSION_STR) + "<br>";
    aboutString += "SQLite " + sqliteVersion + "</font>";

    QLabel *text = new QLabel(dialog);
    text->setText(aboutString);
    text->setOpenExternalLinks(true);

    QHBoxLayout *layout = new QHBoxLayout();
    layout->addWidget(image);
    layout->addWidget(text);

    QPushButton *close = new QPushButton("&Fermer", this);
    connect(close, SIGNAL(clicked()), dialog, SLOT(accept()));

    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addLayout(layout);
    mainLayout->addWidget(close, 0, Qt::AlignRight);

    dialog->setLayout(mainLayout);
    dialog->show();
}

void MainWindow::sync()
{
    if (m_oauth2->linked())
    {
        QNetworkAccessManager *manager = new QNetworkAccessManager();
        O2Requestor *requestor = new O2Requestor(manager, m_oauth2);

        QUrl url(m_o2InternalSettings->value("sync_url").toString());

        QUrlQuery params;
        params.addQueryItem("notes", Note::getJsonNotesToSync());

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        requestor->post(request, params.toString(QUrl::FullyEncoded).toUtf8());

        QObject::connect(requestor, SIGNAL(finished(int, QNetworkReply::NetworkError, QByteArray)),
                         this, SLOT(onSyncRequestFinished(int, QNetworkReply::NetworkError, QByteArray)));
    }
    else
    {
        ConnexionDialog dialog(m_oauth2, this);
        QObject::connect(&dialog, SIGNAL(connected()), this, SLOT(sync()));
        dialog.exec();
    }
}

void MainWindow::onSyncRequestFinished(int id, QNetworkReply::NetworkError error, QByteArray data)
{
    if (error == QNetworkReply::AuthenticationRequiredError)
    {
        QMessageBox::critical(this, "Erreur d'authentification", "Session expirée. Vous devez vous reconnecter.");
    }
    else if (error == QNetworkReply::ConnectionRefusedError)
    {
        QMessageBox::critical(this, "Erreur de connexion", "Impossible de contacter le serveur YellowNote. Vérifier votre connexion Internet ou réessayer plus tard.");
    }
    else if (error == QNetworkReply::NoError)
    {
        // Maintenant qu'on a prévenu le serveur que les notes allaient être supprimées,
        // Suppression réelles des notes dans la db local
        Note::deleteInDb();

        // Passage de toutes les notes en to_sync = 0 dans la db local
        Note::setToSyncOffInDb();

        QJsonDocument d = QJsonDocument::fromJson(data);
        if (d.isArray())
        {
            QJsonArray jsonArray = d.array();
            foreach (const QJsonValue &value, jsonArray)
            {
                QJsonObject obj = value.toObject();
                QString sharedKey = obj["shared_key"].toString();

                // La note a été supprimée sur le serveur, on la supprime aussi ici
                if (obj["to_delete"].toBool())
                {
                    Note::deleteInDb(sharedKey);
                }
                // Récupération des notes du serveur
                else
                {
                    // Modification d'une note déjà existante
                    if (Note::exists(sharedKey))
                    {
                        // Récupération de la position de la note dans la listWidget
                        int row = m_sharedkeyRows[sharedKey];

                        // Création du nouveau label
                        NoteLabel *label = new NoteLabel(
                            obj["title"].toString(),
                            obj["content"].toString(),
                            SqlUtils::date(obj["updated_at"].toString())
                        );

                        // Récupération de l'item
                        QListWidgetItem *item = m_listWidget->item(row);
                        NoteListWidgetItem *noteItem = static_cast<NoteListWidgetItem*>(item);
                        noteItem->setSizeHint(label->minimumSizeHint());
                        // Assignation du nouveau label
                        m_listWidget->setItemWidget(item, label);

                        // Modification de la note rattachée à l'item
                        noteItem->note()->setTitle(obj["title"].toString());
                        noteItem->note()->setContent(obj["content"].toString());
                        noteItem->note()->setUpdatedAt(obj["content"].toString());

                        // Modification de la note en base
                        noteItem->note()->editInDb();
                    }
                    // Nouvelle note
                    else
                    {
                        Note *note = new Note(
                            obj["title"].toString(),
                            obj["content"].toString(),
                            obj["shared_key"].toString()
                        );
                        note->setCreatedAt(obj["created_at"].toString());
                        note->setUpdatedAt(obj["updated_at"].toString());
                        addNoteLabel(note);
                        note->addToDb(false);
                    }
                }
            }
        }
        else
        {
            QMessageBox::critical(this, "Erreur de connexion", "Une erreur est survenue en contactant le serveur YellowNote. Veuillez réessayer plus tard.");
            return;
        }
    }
}
