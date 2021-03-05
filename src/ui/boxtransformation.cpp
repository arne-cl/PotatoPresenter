#include "boxtransformation.h"
#include <vector>
#include <math.h>
#include <numbers>

BoxTransformation::BoxTransformation()
{

}

BoxTransformation::BoxTransformation(std::shared_ptr<Box> box, TransformationType trafo, pointPosition posMouseBox, int pageNumber, QPoint mousePos)
    : mBox{box}
    , mTrafo{trafo}
    , mPosMouseBox{posMouseBox}
    , mPageNumber{pageNumber}
    , mLastMousePosition{mousePos}
{
}

void BoxTransformation::doTransformation(QPoint mousePos, std::shared_ptr<Presentation> pres){
    BoxGeometry box;
    switch (mTrafo) {
        case TransformationType::translate:{
            box = makeScaleTransformation(mousePos);
            break;
        }
        case TransformationType::rotate:
            box = makeRotateTransformation(mousePos);
            break;
        }
    pres->setBoxGeometry(mBox->id(), box, mPageNumber);
}

QRect BoxTransformation::scale(QPoint mouse, QPointF v, BoxGeometry* boxrect) const{
    QRect rect = boxrect->rect();
    auto const transformV = boxrect->rotateTransform().map(v);
    auto const projection = QPointF::dotProduct(mouse, transformV);
    qWarning() << "pro" << projection * v.x() << mouse.x();
    auto const width = rect.width() - projection * v.x();
    auto const height = rect.height() - projection * v.y();
    rect.setSize(QSize(width, height));
    return rect;
}

BoxGeometry BoxTransformation::makeScaleTransformation(QPoint mousePos){
    auto mouseMovement = mousePos - mLastMousePosition;
    mLastMousePosition = mousePos;
    auto geometry = mBox->geometry();
    auto rect = geometry.rect();
    switch (mPosMouseBox) {
        case pointPosition::topLeftCorner:{
            auto const bottomRight = geometry.transform().map(rect.bottomRight());
            rect.moveBottomRight(bottomRight);
            auto const localMouse = geometry.transform(bottomRight).inverted().map(mousePos);
            rect.setTopLeft(localMouse);
            auto const center = geometry.transform(bottomRight).map(rect.center());
            rect.moveCenter(center);
            break;
        }
        case pointPosition::topRightCorner:{
            auto const bottomLeft = geometry.transform().map(rect.bottomLeft());
            rect.moveBottomLeft(bottomLeft);
            auto const localMouse = geometry.transform(bottomLeft).inverted().map(mousePos);
            rect.setTopRight(localMouse);
            auto const center = geometry.transform(bottomLeft).map(rect.center());
            rect.moveCenter(center);
            break;
        }
        case pointPosition::bottomLeftCorner:{
            auto const topRight = geometry.transform().map(rect.topRight());
            rect.moveTopRight(topRight);
            auto const localMouse = geometry.transform(topRight).inverted().map(mousePos);
            rect.setBottomLeft(localMouse);
            auto const center = geometry.transform(topRight).map(rect.center());
            rect.moveCenter(center);
            break;
        }
        case pointPosition::bottomRightCorner:{
            auto const topLeft = geometry.transform().map(rect.topLeft());
            rect.moveTopLeft(topLeft);
            auto const localMouse = geometry.transform(topLeft).inverted().map(mousePos);
            rect.setBottomRight(localMouse);
            auto const center = geometry.transform(topLeft).map(rect.center());
            rect.moveCenter(center);
            break;
        }
        case pointPosition::topBorder:{
            auto const bottomLeft = geometry.transform().map(rect.bottomLeft());
            rect.moveBottomLeft(bottomLeft);
            auto const localMouse = geometry.transform(bottomLeft).inverted().map(mousePos);
            rect.setTop(localMouse.y());
            auto const center = geometry.transform(bottomLeft).map(rect.center());
            rect.moveCenter(center);
            break;
        }
        case pointPosition::bottomBorder:{
            auto const topLeft = geometry.transform().map(rect.topLeft());
            rect.moveTopLeft(topLeft);
            auto const localMouse = geometry.transform(topLeft).inverted().map(mousePos);
            rect.setBottom(localMouse.y());
            auto const center = geometry.transform(topLeft).map(rect.center());
            rect.moveCenter(center);
            break;
        }
        case pointPosition::leftBorder:{
            auto const topRight = geometry.transform().map(rect.topRight());
            rect.moveTopRight(topRight);
            auto const localMouse = geometry.transform(topRight).inverted().map(mousePos);
            rect.setLeft(localMouse.x());
            auto const center = geometry.transform(topRight).map(rect.center());
            rect.moveCenter(center);
            break;
        }
        case pointPosition::rightBorder:{
            auto const topLeft = geometry.transform().map(rect.topLeft());
            rect.moveTopLeft(topLeft);
            auto const localMouse = geometry.transform(topLeft).inverted().map(mousePos);
            rect.setRight(localMouse.x());
            auto const center = geometry.transform(topLeft).map(rect.center());
            rect.moveCenter(center);
            break;
        }
        case pointPosition::inBox:{
            rect.translate(mouseMovement);
            geometry.setRect(rect);
            return geometry;
            break;
        }
        case pointPosition::notInBox:{
            return {};
        }
    }
    rect = rect.normalized();
    geometry.setRect(rect);
    return geometry;
}

BoxGeometry BoxTransformation::makeRotateTransformation(QPoint mousePos){
    auto boxrect = mBox->geometry();
    auto const center = boxrect.rect().center();
    auto const centerToMouse = center - mousePos;
    qInfo() << centerToMouse;
    auto const mouseAngle = std::atan2(double(centerToMouse.y()), centerToMouse.x());
    auto const angleCenterEdge = std::atan2(boxrect.rect().height(), boxrect.rect().width());
    qreal rectAngle;
    qInfo() << "mouseAngle: " << mouseAngle;
    switch (mPosMouseBox) {
    case pointPosition::topLeftCorner:
        rectAngle = mouseAngle - angleCenterEdge;
        break;
    case pointPosition::bottomLeftCorner:
        rectAngle = mouseAngle + angleCenterEdge;
        break;
    case pointPosition::bottomRightCorner:
        rectAngle = mouseAngle + std::numbers::pi - angleCenterEdge;
        break;
    case pointPosition::topRightCorner:
        rectAngle = mouseAngle - std::numbers::pi + angleCenterEdge;
        break;
    case pointPosition::inBox:{
        auto rect = boxrect.rect();
        rect.translate(mousePos - mLastMousePosition);
        mLastMousePosition = mousePos;
        boxrect.setRect(rect);
        return boxrect;
    }
    default:
        return {};
        break;
    }
    boxrect.setAngle(rectAngle / std::numbers::pi * 180);
    return boxrect;
}


