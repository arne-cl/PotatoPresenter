/*
    SPDX-FileCopyrightText: 2020-2021 Theresa Gier <theresa@fam-gier.de>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef TEXTBOX_H
#define TEXTBOX_H

#include <box.h>

struct TextBoundings {
    std::vector<QRectF> lineBoundingRects;

    bool contains(QPoint point, int margin, BoxGeometry geometry) const {
        point = geometry.transform().inverted().map(point);
        bool inbox = false;
        for(auto const& lineRect: lineBoundingRects) {
            auto rect = lineRect.translated(geometry.left(), geometry.top());
            rect = rect.marginsAdded(QMargins(margin, margin, margin, margin));
            if(rect.contains(point)) {
                inbox = true;
            }
        }
        return inbox;
    };
};

class TextBox : public Box
{
public:
    TextBox(QString text);
    virtual std::shared_ptr<TextBox> clone() = 0;

    bool containsPoint(QPoint point, int margin) const override;

    void appendText(QString const& text);
    QString const& text() const;

protected:
    QString mText;
    TextBoundings mTextBoundings;
};

#endif // TEXTBOX_H
