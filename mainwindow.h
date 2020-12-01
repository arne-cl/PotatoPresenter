#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <KTextEditor/Document>
#include <QListWidget>
#include "frame.h"
#include "paintdocument.h"
#include "configboxes.h"
#include "layout.h"
#include "framelistmodel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void fileChanged(KTextEditor::Document *doc);

private slots:

private:
    Ui::MainWindow *ui;
    QGraphicsScene *scene;
    QGraphicsEllipseItem *ellipse;
    QGraphicsRectItem *rectangle;
    QGraphicsTextItem *text;
    KTextEditor::Document* doc;
    PaintDocument* mPaintDocument;
    ConfigBoxes mConfiguration;
    QListWidget *mListWidget;
    FrameListModel *mFrameModel;
};
#endif // MAINWINDOW_H
