#include "paintdocument.h"
#include <KTextEditor/Document>
#include <QMouseEvent>


#include<QPainter>
#include "painter.h"

PaintDocument::PaintDocument(QWidget*&)
    : QWidget(), mWidth{frameSize().width()}, pageNumber{0}
    , mLayout{Layout(aspectRatio::sixteenToNine)}
{
    setMouseTracking(true);
    mSize = mLayout.mSize;
    mScale = 1.0 * mSize.width() / mWidth;
}

void PaintDocument::setPresentation(std::shared_ptr<Presentation> pres){
    mPresentation = pres;
    mActiveBoxId = QString();
}

void PaintDocument::paintEvent(QPaintEvent*)
{
    painter.begin(this);
    painter.setViewport(QRect(0, 0, mWidth, 1.0 * mWidth/mSize.width()*mSize.height()));
    painter.setWindow(QRect(QPoint(0, 0), mSize));
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(QRect(QPoint(0, 0), mSize), Qt::white);
    if(!mPresentation->empty()){
        auto paint = std::make_shared<Painter>(painter);
        paint->paintFrame(mPresentation->frameAt(pageNumber));
    }
    auto const box = mPresentation->getBox(mActiveBoxId);
    if(box != nullptr){
        box->drawBoundingBox(painter);
        box->drawScaleMarker(painter, diffToMouse);
    }
    auto font = painter.font();
    font.setPixelSize(50);
    painter.setFont(font);
    painter.drawText(QRect(0, mSize.height(), mSize.width(), 80), Qt::AlignCenter, mCurrentFrameId);
    painter.end();
}

QSize PaintDocument::sizeHint() const{
    return mLayout.mSize;
}

void PaintDocument::updateFrames(){
    setCurrentPage(mCurrentFrameId);
    update();
}

void PaintDocument::setCurrentPage(int page){
    if(page >= int(mPresentation->size()) || page < 0) {
        return;
    }
    pageNumber = page;
    mCurrentFrameId = mPresentation->frameAt(page)->id();
    mActiveBoxId = QString();
    emit selectionChanged(mPresentation->frameAt(pageNumber));
    update();
}

void PaintDocument::setCurrentPage(QString id){
    int counter = 0;
    for(auto const & frame: mPresentation->frames()) {
        if(frame->id() == id) {
            if(pageNumber != counter){
                mActiveBoxId = QString();
            }
            pageNumber = counter;
            emit pageNumberChanged(pageNumber);
            break;
        }
        counter++;
    }
}

void PaintDocument::resizeEvent(QResizeEvent*) {
    mWidth = frameSize().width();
    mScale = 1.0 * mSize.width() / mWidth;
}

void PaintDocument::determineBoxInFocus(QPoint mousePos){
    auto lastId = mActiveBoxId;
    mActiveBoxId = QString();
    for(auto box: mPresentation->frameAt(pageNumber)->getBoxes()) {
        if(box->geometry().contains(mousePos) && lastId != box->id()) {
            mActiveBoxId = box->id();
            break;
        }
    }
}

std::vector<std::shared_ptr<Box>> PaintDocument::determineBoxesUnderMouse(QPoint mousePos){
    mActiveBoxId = QString();
    std::vector<std::shared_ptr<Box>> boxesUnderMouse;
    for(auto &box: mPresentation->frameAt(pageNumber)->getBoxes()) {
        if(box->geometry().contains(mousePos, diffToMouse)) {
            boxesUnderMouse.push_back(box);
        }
    }
    return boxesUnderMouse;
}

void PaintDocument::mousePressEvent(QMouseEvent *event)
{
    if(mPresentation->empty()){
        return;
    }
    momentTrafo.reset();
    if (event->button() != Qt::LeftButton) {
        return;
    }
    lastPosition = event->pos() * mScale;
    cursorApperance(lastPosition);
    update();
    if(mActiveBoxId.isEmpty()) {
        return;
    }
}

void PaintDocument::mouseMoveEvent(QMouseEvent *event)
{
    if(mPresentation->empty()){
        return;
    }
    auto const newPosition = event->pos() * mScale;
    if(event->buttons() != Qt::LeftButton){
        cursorApperance(newPosition);
        return;
    }
    auto const mouseMovement = newPosition - lastPosition;
    if(mouseMovement.manhattanLength() < diffToMouse / 5){
        return;
    }
    if(!momentTrafo){
        cursorApperance(newPosition);
        if(mActiveBoxId.isEmpty()){
            return;
        }
        auto const activeBox = mPresentation->getBox(mActiveBoxId);
        auto const posMouseBox = activeBox->geometry().classifyPoint(lastPosition, diffToMouse);
        momentTrafo = BoxTransformation(activeBox, mTransform, posMouseBox, pageNumber, newPosition);
    }
    momentTrafo->doTransformation(newPosition, mPresentation);
    lastPosition = newPosition;
    update();
}

void PaintDocument::mouseReleaseEvent(QMouseEvent *event)
{
    if(mPresentation->empty()){
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if(mActiveBoxId.isEmpty() || !momentTrafo){
        determineBoxInFocus(event->pos() * mScale);
    }
    update();
}


void PaintDocument::cursorApperance(QPoint mousePosition){
    auto cursor = QCursor();
    if(mActiveBoxId.isEmpty()){
        cursor.setShape(Qt::ArrowCursor);
        setCursor(cursor);
        return;
    }
    cursor.setShape(Qt::ArrowCursor);
    auto const rect = mPresentation->getBox(mActiveBoxId)->geometry();
    auto const posMouseBox = rect.classifyPoint(mousePosition, diffToMouse);
    auto angle = rect.angle();
    switch(mTransform){
    case(TransformationType::translate):
        switch(posMouseBox){
        case pointPosition::topBorder:
            cursor.setShape(angleToCursor(90 + angle));
            break;
        case pointPosition::bottomBorder:
            cursor.setShape(angleToCursor(270 + angle));
            break;
        case pointPosition::leftBorder:
            cursor.setShape(angleToCursor(angle));
            break;
        case pointPosition::rightBorder:
            cursor.setShape(angleToCursor(180 + angle));
            break;
        case pointPosition::topLeftCorner:
            cursor.setShape(angleToCursor(45 + angle));
            break;
        case pointPosition::bottomRightCorner:
            cursor.setShape(angleToCursor(225 + angle));
            break;
        case pointPosition::topRightCorner:
            cursor.setShape(angleToCursor(135 + angle));
            break;
        case pointPosition::bottomLeftCorner:
            cursor.setShape(angleToCursor(315 + angle));
            break;
        case pointPosition::inBox:
            cursor.setShape(Qt::SizeAllCursor);
            break;
        case pointPosition::notInBox:
            cursor.setShape(Qt::ArrowCursor);
        }
        break;
    case TransformationType::rotate:
        switch (posMouseBox) {
        case pointPosition::topLeftCorner:
            cursor.setShape(Qt::CrossCursor);
            break;
        case pointPosition::bottomRightCorner:
            cursor.setShape(Qt::CrossCursor);
            break;
        case pointPosition::topRightCorner:
            cursor.setShape(Qt::CrossCursor);
            break;
        case pointPosition::bottomLeftCorner:
            cursor.setShape(Qt::CrossCursor);
            break;
        case pointPosition::inBox:
            cursor.setShape(Qt::SizeAllCursor);
            break;
        default:
            cursor.setShape(Qt::ArrowCursor);
            break;
        }
    }
    setCursor(cursor);
}

void PaintDocument::createPDF(QString filename){
    QPdfWriter pdfWriter(filename);
    auto const pdfLayout = QPageLayout(QPageSize(QSize(160, 90)), QPageLayout::Portrait, QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
    pdfWriter.setPageLayout(pdfLayout);

    QPainter painter(&pdfWriter);
    painter.setWindow(QRect(QPoint(0, 0), mSize));

    painter.begin(&pdfWriter);
    for(auto frame: mPresentation->frames()){
        auto paint = std::make_shared<Painter>(painter);
        paint->paintFrame(frame);
        if(frame != mPresentation->frames().back()){
            pdfWriter.newPage();
        }
    }
    painter.end();
}

void PaintDocument::layoutTitle(){
    if(mActiveBoxId.isEmpty()) {
        return;
    }
    mPresentation->setBox(mActiveBoxId, mLayout.mTitlePos, pageNumber);
    update();
}

void PaintDocument::layoutBody(){
    if(mActiveBoxId.isEmpty()) {
        return;
    }
    mPresentation->setBox(mActiveBoxId, mLayout.mBodyPos, pageNumber);
    update();
}

void PaintDocument::layoutFull(){
    if(mActiveBoxId.isEmpty()) {
        return;
    }
    mPresentation->setBox(mActiveBoxId, mLayout.mFullPos, pageNumber);
    update();
}

void PaintDocument::layoutLeft(){
    if(mActiveBoxId.isEmpty()) {
        return;
    }
    mPresentation->setBox(mActiveBoxId, mLayout.mLeftPos, pageNumber);
    update();
}

void PaintDocument::layoutRight(){
    if(mActiveBoxId.isEmpty()) {
        return;
    }
    mPresentation->setBox(mActiveBoxId, mLayout.mRightPos, pageNumber);
    update();
}

void PaintDocument::layoutPresTitle(){
    if(mActiveBoxId.isEmpty()) {
        return;
    }
    mPresentation->setBox(mActiveBoxId, mLayout.mPresTitlePos, pageNumber);
    update();
}

void PaintDocument::layoutSubtitle(){
    if(mActiveBoxId.isEmpty()) {
        return;
    }
    mPresentation->setBox(mActiveBoxId, mLayout.mSubtitlePos, pageNumber);
    update();
}
void PaintDocument::setTransformationType(TransformationType type){
    mTransform = type;
}

Qt::CursorShape PaintDocument::angleToCursor(qreal angle){
    angle = int(angle) % 180;
    if(angle >= 157.5 || angle < 22.5){
        return Qt::SizeHorCursor;
    }
    if(angle >= 22.5 && angle < 67.5){
        return Qt::SizeFDiagCursor;
    }
    if(angle >= 67.5 && angle < 112.5){
        return Qt::SizeVerCursor;
    }
    if(angle >= 110.5 && angle < 157.5){
        return Qt::SizeBDiagCursor;
    }
    else{
        return Qt::SizeAllCursor;
    }
}

int PaintDocument::getPageNumber() const{
    return pageNumber;
}
