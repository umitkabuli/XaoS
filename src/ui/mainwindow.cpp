#include "mainwindow.h"
#include "fractalwidget.h"
#include "customdialog.h"

#include "ui.h"

#include <QtWidgets>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QCoreApplication::applicationName());

    m_fractalWidget = new FractalWidget();
    setCentralWidget(m_fractalWidget);

    readSettings();
}

MainWindow::~MainWindow()
{
}

FractalWidget *MainWindow::fractalWidget()
{
    return m_fractalWidget;
}

void MainWindow::readSettings()
{
    QSettings settings;
    QPoint pos = settings.value("windowPosition", QPoint(200, 200)).toPoint();
    QSize size = settings.value("imageSize", QSize(640, 480)).toSize();
    m_fractalWidget->setSizeHint(size);
    move(pos);
}

void MainWindow::writeSettings()
{
    QSettings settings;
    settings.setValue("windowPosition", pos());
    settings.setValue("imageSize", size());
}

void MainWindow::closeEvent(QCloseEvent *)
{
    writeSettings();
    ui_quit();
}

QKeySequence::StandardKey MainWindow::keyForItem(const QString &name)
{
    if (name == "initstate") return QKeySequence::New;
    if (name == "loadpos") return QKeySequence::Open;
    if (name == "savepos") return QKeySequence::Save;
    if (name == "quit") return QKeySequence::Quit;
    if (name == "undo") return QKeySequence::Undo;
    if (name == "redo") return QKeySequence::Redo;
    if (name == "interrupt") return QKeySequence::Cancel;
    if (name == "recalculate") return QKeySequence::Refresh;
    if (name == "help") return QKeySequence::HelpContents;

    return QKeySequence::UnknownKey;
}

void MainWindow::buildMenu(struct uih_context *uih, const char *name)
{
    menuBar()->clear();

    const menuitem *item;
    for (int i = 0; (item = menu_item(name, i)) != NULL; i++) {
        if (item->type == MENU_SUBMENU) {
            QMenu *menu = menuBar()->addMenu(QString(item->name));
            buildMenu(uih, item->shortname, menu, false);
        }
    }
}

void MainWindow::buildMenu(struct uih_context *uih, const char *name, QMenu *parent, bool numbered)
{
    QActionGroup *group = 0;

    connect(parent, SIGNAL(aboutToShow()), SLOT(updateMenuCheckmarks()));

    const menuitem *item;
    for (int i = 0, n = 0; (item = menu_item(name, i)) != NULL; i++) {

        QString itemName(item->name);
        if (numbered) {
            char c;
            if (n < 9)
                c = n + '1';
            else if (n == 9)
                c = '0';
            else
                c = 'A' + n - 10;
            itemName = QString::asprintf("&%c ", c) + itemName;

            if (item->type != MENU_SEPARATOR)
                n++;
        }

        if (item->type == MENU_DIALOG || item->type == MENU_CUSTOMDIALOG)
            itemName += "...";

        if (item->type == MENU_SEPARATOR) {
            parent->addSeparator();
        } else if (item->type == MENU_SUBMENU) {
            QMenu *menu = parent->addMenu(itemName);
            buildMenu(uih, item->shortname, menu, numbered);
        } else {
            QAction *action = new QAction(itemName, parent);
            action->setShortcuts(keyForItem(item->shortname));
            action->setObjectName(item->shortname);
            if (item->flags & (MENUFLAG_RADIO | MENUFLAG_CHECKBOX)) {
                action->setCheckable(true);
                action->setChecked(menu_enabled(item, uih));
                if (item->flags & MENUFLAG_RADIO) {
                    if (!group)
                        group = new QActionGroup(parent);
                    action->setActionGroup(group);
                }
            }
            connect(action, SIGNAL(triggered()), this, SLOT(activateMenuItem()));
            parent->addAction(action);
        }
    }
}

void MainWindow::popupMenu(struct uih_context *uih, const char *name)
{
    QMenu *menu = new QMenu(this);
    buildMenu(uih, name, menu, true);
    menu->exec(QCursor::pos());
    delete menu;
}

void MainWindow::toggleMenu(struct uih_context *uih, const char *name)
{
    const menuitem *item = menu_findcommand(name);
    QAction *action = menuBar()->findChild<QAction *>(name);
    if (action)
        action->setChecked(menu_enabled(item, uih));
}

void MainWindow::activateMenuItem()
{
    QAction *action = qobject_cast<QAction *>(sender());
    const menuitem *item = menu_findcommand(action->objectName().toUtf8());
    ui_menuactivate(item, NULL);
}

void MainWindow::updateMenuCheckmarks()
{
    QMenu *menu = qobject_cast<QMenu *>(sender());
    foreach(QAction *action, menu->actions()) {
        if (action->isCheckable()) {
            const menuitem *item = menu_findcommand(action->objectName().toUtf8());
            action->setChecked(menu_enabled(item, globaluih));
        }
    }

}

void MainWindow::showDialog(struct uih_context *uih, const char *name)
{
    const menuitem *item = menu_findcommand(name);
    if (!item) return;

    const menudialog *dialog = menu_getdialog(uih, item);
    if (!dialog) return;

    int nitems;
    for (nitems = 0; dialog[nitems].question; nitems++);

    if (nitems == 1 && (dialog[0].type == DIALOG_IFILE || dialog[0].type == DIALOG_OFILE)) {
        QString filter = QString("*.%1").arg(QFileInfo(dialog[0].defstr).completeSuffix());
        QString directory;// = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);

        QString fileName;
        if (dialog[0].type == DIALOG_IFILE)
            fileName = QFileDialog::getOpenFileName(this, item->name, directory, filter);
        else if (dialog[0].type == DIALOG_OFILE)
            fileName = QFileDialog::getSaveFileName(this, item->name, directory, filter);


        if (!fileName.isNull()) {
            dialogparam *param = (dialogparam *)malloc(sizeof(dialogparam));
            param->dstring = strdup(fileName.toUtf8());
            ui_menuactivate(item, param);
        }
    } else {
        CustomDialog customDialog(uih, item, dialog, this);
        if (customDialog.exec() == QDialog::Accepted)
            ui_menuactivate(item, customDialog.parameters());
    }
}