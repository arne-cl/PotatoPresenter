/*
    SPDX-FileCopyrightText: 2020-2021 Theresa Gier <theresa@fam-gier.de>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <KParts/ReadOnlyPart>
#include <KTextEditor/View>
#include <KTextEditor/MarkInterface>
#include <KActionCollection>
#include <KTextEditor/ConfigInterface>

#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QDebug>
#include <QObject>
#include <QTableWidget>
#include <QFileDialog>
#include <QToolButton>
#include <QMessageBox>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>

#include <functional>
#include <algorithm>

#include "latexcachemanager.h"
#include "cachemanager.h"
#include "slidelistmodel.h"
#include "slidelistdelegate.h"
#include "templatelistdelegate.h"
#include "pdfcreator.h"
#include "utils.h"
#include "potatoformatvisitor.h"
#include "transformboxundo.h"
#include "version.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mLastAutosave(QDateTime::currentDateTime())
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/potato_logo.png"));
    ui->splitter->setSizes(QList<int>{10000, 10000});


//    setup Editor
    mEditor = KTextEditor::Editor::instance();
    mDoc = mEditor->createDocument(this);
    mViewTextDoc = mDoc->createView(this);
    ui->editor->addWidget(mViewTextDoc);


//    setup Presentation, Slide Widget
    mPresentation = std::make_shared<Presentation>();
    mSlideWidget = ui->slideWidget;
    mSlideWidget->setPresentation(mPresentation);
    connect(&mTemplateCache, &TemplateCache::templateChanged,
            this, [this](){
        mTemplateCache.resetTemplate();
        fileChanged();
    });


//    setup Item model
    mSlideModel = new SlideListModel(this);
    mSlideModel->setPresentation(mPresentation);
    ui->pagePreview->setModel(mSlideModel);
    SlideListDelegate *delegate = new SlideListDelegate(this);
    ui->pagePreview->setItemDelegate(delegate);
    ui->pagePreview->setViewMode(QListView::IconMode);
    QItemSelectionModel *selectionModel = ui->pagePreview->selectionModel();
    connect(selectionModel, &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current){
        mSlideWidget->setCurrentPage(current.row());
                });


//    setup Action
    connect(ui->actionQuit, &QAction::triggered,
            this, &MainWindow::close);
    connect(ui->actionNew, &QAction::triggered,
            this, [this](){
                if(!closeDocument()) return;
                setWindowTitle(windowTitle("Untitled"));
                ui->mainWidget->setCurrentIndex(1);});
    connect(ui->actionOpen, &QAction::triggered,
            this, &MainWindow::openFile);
    connect(ui->actionSave, &QAction::triggered,
            this, &MainWindow::save);
    connect(ui->actionSave_as, &QAction::triggered,
            this, &MainWindow::saveAs);
    connect(ui->actionCreatePDF, &QAction::triggered,
                     this, &MainWindow::exportPDF);
    connect(ui->actionExport_PDF_as, &QAction::triggered,
            this, &MainWindow::exportPDFAs);
    connect(ui->actionExport_PDF_Handout, &QAction::triggered,
            this, &MainWindow::exportPDFHandout);
    connect(ui->actionExport_PDF_Handout_as, &QAction::triggered,
            this, &MainWindow::exportPDFHandoutAs);
    connect(ui->actionReload_Resources, &QAction::triggered,
            this, &MainWindow::resetCacheManager);

    connect(ui->actionUndo, &QAction::triggered,
            mSlideWidget, &SlideWidget::undo);
    connect(ui->actionRedo, &QAction::triggered,
            mSlideWidget, &SlideWidget::redo);
    connect(ui->actionReset_Position, &QAction::triggered,
            this, [this]() {
                mSlideWidget->deleteBoxPosition();
            });
    connect(ui->actionReset_Angle, &QAction::triggered,
            this, [this]() {
        mSlideWidget->deleteBoxAngle();
    });

    auto transformGroup = new QActionGroup(this);
    transformGroup->addAction(ui->actionRotate);
    transformGroup->addAction(ui->actionTranslate);
    ui->actionTranslate->setChecked(true);
    connect(ui->actionRotate, &QAction::triggered,
           this, [this](){mSlideWidget->setTransformationType(TransformationType::rotate);});
    connect(ui->actionTranslate, &QAction::triggered,
            this, [this](){mSlideWidget->setTransformationType(TransformationType::translate);});

//    open Recent
    QSettings settings("Potato", "Potato Presenter");
    updateOpenRecent();
    connect(ui->openRecent_listWidget, &QListWidget::itemClicked,
            this, [this](QListWidgetItem *item){openProject(item->data(Qt::ToolTipRole).toString());});

//    setup CacheManager
    connect(&cacheManager(), &LatexCacheManager::conversionFinished,
            mSlideWidget, QOverload<>::of(&SlideWidget::update));

    CacheManager<QPixmap>::instance().setCallback([this](QString){mSlideWidget->update();});
    CacheManager<QSvgRenderer>::instance().setCallback([this](QString){mSlideWidget->update();});
    CacheManager<PixMapVector>::instance().setCallback([this](QString){mSlideWidget->update();});


//    setup bar with error messages, snapping and couple button
    mErrorOutput = new QLabel(this);
    mErrorOutput->setWordWrap(true);

    auto *barTop = new QHBoxLayout(this);
    ui->paint->insertLayout(0, barTop);

    mCoupleButton = new QToolButton(this);
    mCoupleButton->setCheckable(true);
    mCoupleButton->setChecked(true);
    mCoupleButton->setIcon(QIcon(":/icons/link.svg"));
    mCoupleButton->setToolTip("Couple the Cursor in the Editor and the selection in Slide view");

    mSnappingButton = new QToolButton(this);
    mSnappingButton->setCheckable(true);
    mSnappingButton->setChecked(true);
    mSnappingButton->setIcon(QIcon(":/icons/snap-nodes-cusp.svg"));
    mSnappingButton->setToolTip("Turn Snapping on/off during Box Geometry manipulation");

    barTop->addWidget(mCoupleButton);
    barTop->addWidget(mSnappingButton);
    barTop->addWidget(mErrorOutput);

    connect(mSnappingButton, &QToolButton::clicked,
            this, [this](){mSlideWidget->setSnapping(mSnappingButton->isChecked());});


//    coupling between document and slide widget selection
    connect(mSlideWidget, &SlideWidget::selectionChanged,
            this, [this](Slide::Ptr slide){
            if(!mCoupleButton->isChecked()) {return ;}
            auto [slideInLine, boxInLine] = mPresentation->findBoxForLine(mViewTextDoc->cursorPosition().line());
            if(slideInLine != slide) {
                mViewTextDoc->setCursorPosition(KTextEditor::Cursor(slide->line(), 0));
                mViewTextDoc->removeSelection();
            }});
    connect(mSlideWidget, &SlideWidget::boxSelectionChanged,
            this, [this](Box::Ptr box){
            if(!mCoupleButton->isChecked()) {return ;}
            auto [slideInLine, boxInLine] = mPresentation->findBoxForLine(mViewTextDoc->cursorPosition().line());
            if(boxInLine != box) {
                mViewTextDoc->setCursorPosition(KTextEditor::Cursor(box->line(), 0));
                mViewTextDoc->removeSelection();
            }});


//    setup left column of the new from template dialog
    connect(ui->toolButtonOpen, &QPushButton::clicked,
            ui->actionOpen, &QAction::trigger);


//    setup Item model templates in create new from template dialog
    auto const dirList = std::vector<QString>{
            ":/templates/templates/tutorial",
            ":/templates/templates/green_lines",
            ":/templates/templates/logo",
            ":/templates/templates/red",
            ":/templates/templates/red_line",
            ":/templates/templates/astro",
            ":/templates/templates/astro2"};
    auto const presentationList = generateTemplatePresentationList(dirList);
    mTemplateModel = new TemplateListModel(this);
    mTemplateModel->setPresentationList(presentationList);
    ui->templateList->setModel(mTemplateModel);
    TemplateListDelegate *delegateTemplate = new TemplateListDelegate(this);
    ui->templateList->setItemDelegate(delegateTemplate);
    connect(ui->templateList, &QListView::clicked,
            this, [this, dirList]{
                mTemplatePath = dirList[ui->templateList->currentIndex().row()];
                openCreatePresentationDialog();
            });
    connect(ui->emptyPresentationButton, &QPushButton::clicked,
            this, [this]{
                mTemplatePath = ":/templates/templates/empty";
                openCreatePresentationDialog();
            });


//    setup create project from template dialog
    connect(ui->back_button, &QPushButton::clicked,
            this, [this]{ui->mainWidget->setCurrentIndex(1);});
    connect(ui->create_project_button, &QPushButton::clicked,
            this, [this, dirList]{createProjectFromTemplate();});
    connect(ui->change_directory_button, &QPushButton::clicked,
            this, [this]{ui->label_folder->setText(openDirectory());});
    connect(ui->project_name_lineEdit, &QLineEdit::returnPressed,
            ui->create_project_button, &QPushButton::click);


//    setup document
    connect(mDoc, &KTextEditor::Document::textChanged,
                     this, &MainWindow::fileChanged);
    newDocument();

//    open create new from template dialog
    connect(ui->mainWidget, &QStackedWidget::currentChanged,
            this, [this](auto const index){
        if (index == 0) {
            setActionenEnabled(true);
        }
        else {
            setActionenEnabled(false);
        }
    });
    ui->mainWidget->setCurrentIndex(1);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if(!closeDocument()) {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::fileChanged() {
    auto iface = qobject_cast<KTextEditor::MarkInterface*>(mDoc);
    iface->clearMarks();
    auto const text = mDoc->text().toUtf8().toStdString();
    auto const parserOutput = generateSlides(text, fileDirectory());
    if(parserOutput.successfull()) {
        auto const slides = parserOutput.slideList();
        auto const preamble = parserOutput.preamble();
        auto templateName = preamble.templateName;
        Template::Ptr presentationTemplate = nullptr;
        if(!templateName.isEmpty()) {
            if (!templateName.startsWith("/home")) {
                templateName = fileDirectory() + "/" + templateName;
            }
            presentationTemplate = mTemplateCache.getTemplate(templateName);
            if (!presentationTemplate) {
                presentationTemplate = readTemplate(templateName);
                if(!presentationTemplate) {
                    mErrorOutput->setText("Line " + QString::number(parserOutput.preamble().line + 1) +
                                          ": Cannot load template" + " \u26A0");
                    iface->addMark(parserOutput.preamble().line, KTextEditor::MarkInterface::MarkTypes::Error);
                    return;
                }
                mTemplateCache.setTemplate(presentationTemplate, templateName);
            }
        }
        mPresentation->setData({slides, presentationTemplate});
        mErrorOutput->setText("Conversion succeeded \u2714");
    }
    else {
        auto const error = parserOutput.parserError();
        mErrorOutput->setText("Line " + QString::number(error.line + 1) + ": " + error.message + " \u26A0");
        iface->addMark(error.line, KTextEditor::MarkInterface::MarkTypes::Error);
        return;
    }

    mSlideWidget->updateSlideId();
    mSlideWidget->update();
    auto const index = mSlideModel->index(mSlideWidget->pageNumber());
    ui->pagePreview->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
    ui->pagePreview->scrollTo(index);
    autosave();
}

std::shared_ptr<Template> MainWindow::readTemplate(QString templateName) const {
    if(templateName.isEmpty()) {
        return {};
    }
    auto file = QFile(templateName + ".potato");
    if(!file.open(QIODevice::ReadOnly)){
        mErrorOutput->setText(tr("Cannot load template %1.").arg(file.fileName()));
        return {};
    }
    auto thisTemplate = std::make_shared<Template>();
    try {
        thisTemplate->setConfig(templateName + ".json");
    }  catch (ConfigError error) {
        mErrorOutput->setText(tr("Cannot load template %1.").arg(error.filename));
        return {};
    }
    auto const directoryPath = QFileInfo(templateName).absolutePath();
    auto const parserOutput = generateSlides(file.readAll().toStdString(), directoryPath, true);

    if(parserOutput.successfull()) {
        auto const slides = parserOutput.slideList();
        thisTemplate->setSlides(slides);
        return thisTemplate;
    }
    else {
        auto const error = parserOutput.parserError();
        mErrorOutput->setText("Cannot load template \u26A0");
        return {};
    }
}

void MainWindow::setupFileActionsFromKPart() {
    // HACK: Steal actions from the KPart
    deleteShortcutOfKDocAction("file_save");
    deleteShortcutOfKDocAction("file_save_as");
}

void MainWindow::openInputFile(QString filename) {
    auto url = QUrl::fromLocalFile(filename);
    if (!mDoc->openUrl(url)){
        qWarning() << "file not found";
    }
    mDoc->discardDataRecovery();
    mDoc->setHighlightingMode("LaTeX");
}

void MainWindow::askToRecoverAutosave() {
    // recover if file was not properly closed and autosave still exists
    if(QFile::exists(autosaveTextFile()) || QFile::exists(autosaveJsonFile())) {
        auto const ret = QMessageBox::information(this, tr("File was not probably closed."),
                        tr("File was not properly closed. Do you want to recover?"),
                        QMessageBox::Ok, QMessageBox::Cancel);
        switch(ret) {
        case QMessageBox::Cancel:
            break;
        case QMessageBox::Ok:
            recoverAutosave();
            break;
        default:
            break;
        }
        deleteAutosave();
    }
}

void MainWindow::recoverAutosave() {
    auto file = QFile(autosaveTextFile());
    if(file.open(QIODevice::ReadOnly)) {
        mDoc->setText(file.readAll());
    }
    file.close();
    auto const lastConfig = mPresentation->configuration();
    mPresentation->setConfig({autosaveJsonFile()});
    auto transform = new TransformBoxUndo(mPresentation, lastConfig, mPresentation->configuration());
    mSlideWidget->undoStack().push(transform);
}

void MainWindow::deleteAutosave() {
    QFile::remove(autosaveTextFile());
    QFile::remove(autosaveJsonFile());
}

void MainWindow::openFile() {
    if(!closeDocument()) {
        return;
    }
    auto const newFile = QFileDialog::getOpenFileName(this,
        tr("Open File"), guessSavingDirectory(), tr("Potato Files (*.potato)"));
    if(newFile.isEmpty()){
        return;
    }
    openProject(newFile);
}

void MainWindow::openProject(QString path) {
    // check if file exists
    if(!QFile(path).exists()) {
        QMessageBox::information(this, tr("File does not exist"), tr("File does not exist."),
                                         QMessageBox::Ok);
        return;
    }
    if(QFileInfo(path).suffix() != "potato") {
        QMessageBox::information(this, tr("Cannot open file."), tr("Cannot open file. Please choose a .potato file."),
                                 QMessageBox::Ok);
        return;
    }
    if(!QFile::exists(jsonFileName(path))){
        int ret = QMessageBox::information(this, tr("Failed to open File"), tr("Failed to find %1. Genereate a new empty Configuration File").arg(jsonFileName(path)),
                                 QMessageBox::Ok | QMessageBox::Cancel);
        switch (ret) {
        case QMessageBox::Cancel: {
            return;
        }
        case QMessageBox::Ok: {
            auto file = QFile(jsonFileName(path));
            file.open(QIODevice::WriteOnly);
            file.close();
            break;
        }
        }
    }

    ui->mainWidget->setCurrentIndex(0);
    newDocument();
    openInputFile(path);
    mPresentation->setConfig({jsonFileName()});
    mSlideWidget->setPresentation(mPresentation);
    mPdfFile = "";
    fileChanged();
    connect(mPresentation.get(), &Presentation::boxGeometryChanged,
            this, [this]{
        if(!mIsModified) {
            setWindowTitle(windowTitleNotSaved());
            mIsModified = true;
        }});
    connect(ui->actionClean_Configurations, &QAction::triggered,
            mPresentation.get(), &Presentation::deleteNotNeededConfigurations);
    addFileToOpenRecent(path);
    updateOpenRecent();
    addDirectoryToSettings(workingDirectory());
    mIsModified = false;
    setWindowTitle(windowTitle());
    askToRecoverAutosave();
}

void MainWindow::newDocument() {
    ui->mainWidget->setCurrentIndex(0);
    mDoc = mEditor->createDocument(this);
    delete mViewTextDoc;
    mViewTextDoc = mDoc->createView(this);
    ui->editor->addWidget(mViewTextDoc);
    connect(mDoc, &KTextEditor::Document::textChanged,
                     this, [this](){
        fileChanged();
        if(!mIsModified) {
        setWindowTitle(windowTitleNotSaved());
        mIsModified = true;
    }});
    setupFileActionsFromKPart();
    resetPresentation();
    mViewTextDoc->setFocus();
    mDoc->setHighlightingMode("LaTeX");
    mPdfFile = "";

    // couple cursor position and box selection
    mCursorTimer.setSingleShot(true);
    connect(mViewTextDoc, &KTextEditor::View::cursorPositionChanged,
            this, [this](KTextEditor::View *, const KTextEditor::Cursor &){mCursorTimer.start(10);});
    connect(mDoc, &KTextEditor::Document::textChanged,
            this,[this](){mCursorTimer.start(10);});
    connect(&mCursorTimer, &QTimer::timeout,
            this, &MainWindow::updateCursorPosition);
    connect(mPresentation.get(), &Presentation::slideChanged,
            this, [this](int){mSlideWidget->update();
            autosave();});
    setWindowTitle(windowTitle());
    mIsModified = false;
    mLastAutosave = QDateTime::currentDateTime();
    fileChanged();
    resetCacheManager();
}

bool MainWindow::save() {
    if(!mDoc->documentSave()) {
        return false;
    }
    QFile::remove(autosaveTextFile());
    saveJson();
    ui->statusbar->showMessage(tr("Saved File to  \"%1\".").arg(mDoc->url().toLocalFile()), 10000);
    setWindowTitle(windowTitle());
    mIsModified = false;
    return true;
}

bool MainWindow::saveAs(){
    if(!mDoc->documentSaveAs()) {
        return false;
    }
    QFile::remove(autosaveTextFile());
    saveJson();
    fileChanged();
    ui->statusbar->showMessage(tr("Saved File to  \"%1\".").arg(mDoc->url().toLocalFile()), 10000);
    setWindowTitle(windowTitle());
    mIsModified = false;
    return true;
}

QFileInfo MainWindow::fileInfo() const {
    auto const url = mDoc->url().toString(QUrl::PreferLocalFile);
    if (url.isEmpty()) {
        return QFileInfo("Untitled");
    }
    return QFileInfo(url);
}

QString MainWindow::absoluteFilePath() const {
    return fileInfo().absoluteFilePath();
}

QString MainWindow::completeBaseName() const {
    return fileInfo().completeBaseName();
}

QString MainWindow::pathWithBaseName() const {
    return fileInfo().dir().path() + "/" + completeBaseName();
}


QString MainWindow::jsonFileName() const {
    return pathWithBaseName() + ".json";
}

QString MainWindow::fileDirectory() const {
    return fileInfo().dir().absolutePath();
}

QString MainWindow::workingDirectory() const {
    auto dir = QDir(fileDirectory());
    dir.cdUp();
    return dir.absolutePath();
}

QString MainWindow::jsonFileName(QString textPath) const {
    auto const fileInfo = QFileInfo(textPath);
    return fileInfo.path() + "/" + fileInfo.completeBaseName() + ".json";
}

void MainWindow::saveJson() {
    mPresentation->configuration().saveConfig(jsonFileName());
    QFile::remove(autosaveJsonFile());
}

void MainWindow::resetPresentation() {
    mPresentation = std::make_shared<Presentation>();
    mSlideWidget->setPresentation(mPresentation);
    mSlideModel->setPresentation(mPresentation);
    connect(mPresentation.get(), &Presentation::rebuildNeeded,
            this, &MainWindow::fileChanged);
}

void MainWindow::exportPDF() {
    if(mPdfFile.isEmpty()) {
        exportPDFAs();
        return;
    }
    writePDF();
}

void MainWindow::exportPDFAs() {
    QFileDialog dialog;
    if(mPdfFile.isEmpty()) {
        mPdfFile = getPdfFilename();
    }
    dialog.selectFile(mPdfFile);
    mPdfFile = dialog.getSaveFileName(this, tr("Export PDF"),
                               mPdfFile,
                               tr("pdf (*.pdf)"));
    if(QFileInfo(mPdfFile).suffix() != "pdf") {
        mPdfFile += ".pdf";
    }
    writePDF();
}

void MainWindow::exportPDFHandout() {
    if(mPdfFileHandout.isEmpty()) {
        exportPDFHandoutAs();
        return;
    }
    writePDFHandout();
}

void MainWindow::exportPDFHandoutAs() {
    QFileDialog dialog;
    if(mPdfFileHandout.isEmpty()) {
        mPdfFileHandout = getPdfFilenameHandout();
    }
    dialog.selectFile(mPdfFileHandout);
    mPdfFileHandout = dialog.getSaveFileName(this, tr("Export PDF Handout"),
                               mPdfFileHandout,
                               tr("pdf (*.pdf)"));
    writePDFHandout();
}

void MainWindow::writePDF() const {
    PDFCreator creator;
    creator.createPdf(mPdfFile, mPresentation);
    ui->statusbar->showMessage(tr("Saved PDF to \"%1\".").arg(mPdfFile), 10000);
}

void MainWindow::writePDFHandout() const {
    PDFCreator creator;
    creator.createPdfHandout(mPdfFileHandout, mPresentation);
    ui->statusbar->showMessage(tr("Saved PDF to \"%1\".").arg(mPdfFileHandout), 10000);
}

QString MainWindow::getConfigFilename(QUrl inputUrl) {
    return completeBaseName() + ".json";
}

QString MainWindow::getPdfFilename() {
    return completeBaseName() + ".pdf";
}

QString MainWindow::getPdfFilenameHandout() {
    return completeBaseName() + "_handout.pdf";
}

QAction* MainWindow::deleteShortcutOfKDocAction(const char* name){
    QAction *action = mViewTextDoc->action(name);
    if( action )
       {
            action->setShortcut(QKeySequence());
       }
    return action;
}

void MainWindow::updateCursorPosition() {
    if(!mCoupleButton->isChecked()) {
        return ;
    }
    auto [slide, box] = mPresentation->findBoxForLine(mViewTextDoc->cursorPosition().line());
    if(!slide) {
        return;
    }
    mSlideWidget->setActiveBox(box ? box->id() : QString(), slide->id());
    auto const index = mSlideModel->index(mSlideWidget->pageNumber());
    ui->pagePreview->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
}

QString MainWindow::applicationName() const {
    return QString("Potato Presenter") + " " + PROJECT_VER;
}

QString MainWindow::windowTitle() const {
    return windowTitle(completeBaseName());
}

QString MainWindow::windowTitle(QString filename) const {
    return filename + " \u2014 " + applicationName();
}

QString MainWindow::windowTitleNotSaved() const {
    return completeBaseName() + " * \u2014 " + applicationName();
}

bool MainWindow::closeDocument() {
    if(!mIsModified) {
        return true;
    }
    int ret = QMessageBox::information(this, tr("Unsaved changes"), tr("The document %1 has been modified. "
                          "Do you want to save your changes or discard them?").arg(completeBaseName()),
                          QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    switch (ret) {
    case QMessageBox::Save:
        if(!save()) {
            return false;
        }
        mIsModified = false;
        break;
      case QMessageBox::Discard:
        mIsModified = false;
        break;
      case QMessageBox::Cancel:
        return false;
      default:
        return false;
    }
    QFile::remove(autosaveTextFile());
    QFile::remove(autosaveJsonFile());
    return true;
}

Presentation::Ptr MainWindow::generateTemplatePresentation(QString directory) const {
    QFile file(directory + "/demo.potato");
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    auto const val = file.readAll();

    auto presentation = std::make_shared<Presentation>();
    presentation->setConfig({directory + "/demo.json"});

    auto const parserOutput = generateSlides(val.toStdString(), directory);
    if(parserOutput.successfull()) {
        auto const slides = parserOutput.slideList();
        auto const preamble = parserOutput.preamble();
        auto templateName = preamble.templateName;
        if (!templateName.startsWith("/home")) {
            templateName = directory + "/" + templateName;
        }
        auto const presentationTemplate = readTemplate(templateName);
        presentation->setData({slides, presentationTemplate});
        return presentation;
    }
    else {
        return{};
    }
}

Presentation::List MainWindow::generateTemplatePresentationList(std::vector<QString> directories) const {
    Presentation::List list;
    for(auto const& directory : directories) {
        auto const presentation = generateTemplatePresentation(directory);
        if (!presentation) {
            continue;
        }
        list.push_back(presentation);
    }
    return list;
}

void MainWindow::insertTextInEditor(QString path) {
    QFile file(path + "/demo.potato");
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    auto const val = file.readAll();
    mDoc->setText(val);
}


QString MainWindow::openDirectory() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                 guessSavingDirectory(),
                                                 QFileDialog::ShowDirsOnly
                                                 | QFileDialog::DontResolveSymlinks);
    if(dir.isEmpty()){
       return guessSavingDirectory();
    }
    addDirectoryToSettings(dir);
    return dir;
}

void MainWindow::createProjectFromTemplate() {
    auto const projectname = ui->project_name_lineEdit->text();
    if(projectname.isEmpty()) {
        QMessageBox::information(this, tr("Please insert Project name."), tr("Please insert Project name."),
                                 QMessageBox::Ok);
        return;
    }

//    copy template and demo into project folder and rename it
    if(!copyDirectory(mTemplatePath, assembleProjectDirectory(projectname))) {
        QMessageBox::information(this, tr("Copy of template failed."), tr("Copy of template failed. Source directory: %1. Destination Directory: %2").arg(mTemplatePath, assembleProjectDirectory(projectname)),
                                 QMessageBox::Ok);
        return;
    }
    auto const projectName = ui->project_name_lineEdit->text();
    auto inputFile = QFile(assembleProjectDirectory(projectname) + "/demo.potato");
    auto jsonFile = QFile(assembleProjectDirectory(projectname) + "/demo.json");
    qInfo() << inputFile.fileName() << inputFile.exists() << projectName;
    if(projectname != "demo"
        && !(inputFile.rename(assembleProjectPathInputFile(projectname))
        && jsonFile.rename(assembleProjectPathJsonFile(projectname)))) {
        QMessageBox::information(this, tr("Rename failed."), tr("Rename failed."),
                                 QMessageBox::Ok);
        return;
    }
    openProject(assembleProjectPathInputFile(projectname));
}

QString MainWindow::assembleProjectPathInputFile(QString projectname) const {
    return assembleProjectDirectory(projectname) + "/" + projectname + ".potato";
}

QString MainWindow::assembleProjectPathJsonFile(QString projectname) const {
    return assembleProjectDirectory(projectname) + "/" + projectname + ".json";
}

QString MainWindow::assembleProjectDirectory(QString projectname) const {
    return ui->label_folder->text() + "/" + projectname;
}

QString MainWindow::guessSavingDirectory() const {
    QSettings settings;
    auto const dir = settings.value("directory").toString();
    if(dir.isEmpty()) {
        return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    return dir;
}

void MainWindow::addFileToOpenRecent(QString path) {
    auto const maxEntries = 8;
    QSettings settings;
    auto list = readOpenRecentArrayFromSettings(settings);
    auto const rm = std::ranges::remove(list, path);
    list.erase(rm.begin(), rm.end());
    list.insert(list.begin(), path);
    if (list.size() > maxEntries) {
        list.pop_back();
    }
    writeOpenRecentArrayToSettings(list, settings);
}

void MainWindow::updateOpenRecent() {
    QSettings settings;
    auto const openRecentList = readOpenRecentArrayFromSettings(settings);
    ui->menuOpen_Recent->clear();
    ui->openRecent_listWidget->clear();
    for (auto const& entry : openRecentList) {
//        add to toolbar
        QAction *openAct = new QAction(entry, this);
        connect(openAct, &QAction::triggered,
                this, [this, entry]{openProject(entry);});
        ui->menuOpen_Recent->addAction(openAct);

//        add to new from template dialog
        auto const filename = QFileInfo(entry).completeBaseName();
        qInfo() << "open recent filename" << filename;
        QListWidgetItem *newItem = new QListWidgetItem;
        newItem->setText(filename);
        newItem->setData(Qt::ToolTipRole, entry);
        ui->openRecent_listWidget->addItem(newItem);
    }
}

void MainWindow::addDirectoryToSettings(QString directory) const {
    QSettings settings;
    settings.setValue("directory", directory);
    settings.sync();
}

void MainWindow::openCreatePresentationDialog() {
    ui->mainWidget->setCurrentIndex(2);
    if(ui->label_folder->text().isEmpty()) {
        ui->label_folder->setText(guessSavingDirectory());
    }
    ui->project_name_lineEdit->clear();
    ui->project_name_lineEdit->setFocus();
}

void MainWindow::autosave() {
    // look up if the last autosave is more than 15s ago
    auto const currentTime = QDateTime::currentDateTime();
    if(mLastAutosave.addSecs(15) > currentTime) {
        return;
    }
    mLastAutosave = currentTime;

    // save txt file
    QFile file(autosaveTextFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream out(&file);
    out << mDoc->text();

    // save json file
    mPresentation->configuration().saveConfig(autosaveJsonFile());
}

QString MainWindow::autosaveTextFile(QString inputFile) const {
    return inputFile + ".autosave";
}

QString MainWindow::autosaveTextFile() const {
    return autosaveTextFile(absoluteFilePath());
}

QString MainWindow::autosaveJsonFile(QString jsonFile) const {
    return jsonFile + ".autosave";
}

QString MainWindow::autosaveJsonFile() const {
    return autosaveJsonFile(jsonFileName());
}

void MainWindow::setActionenEnabled(bool enabled) {
    ui->actionClean_Configurations->setEnabled(enabled);
    ui->actionCreatePDF->setEnabled(enabled);
    ui->actionExport_PDF_Handout->setEnabled(enabled);
    ui->actionExport_PDF_Handout_as->setEnabled(enabled);
    ui->actionExport_PDF_as->setEnabled(enabled);
    ui->actionRedo->setEnabled(enabled);
    ui->actionReload_Resources->setEnabled(enabled);
    ui->actionReset_Angle->setEnabled(enabled);
    ui->actionReset_Position->setEnabled(enabled);
    ui->actionRotate->setEnabled(enabled);
    ui->actionSave->setEnabled(enabled);
    ui->actionSave_as->setEnabled(enabled);
    ui->actionTranslate->setEnabled(enabled);
    ui->actionUndo->setEnabled(enabled);
}

void MainWindow::resetCacheManager() {
    mTemplateCache.resetTemplate();
    CacheManager<QPixmap>::instance().deleteAllResources();
    CacheManager<QSvgRenderer>::instance().deleteAllResources();
    CacheManager<PixMapVector>::instance().deleteAllResources();
    cacheManager().resetCache();
}
