#ifndef PRESENTATION_H
#define PRESENTATION_H

#include <QObject>
#include <vector>
#include "frame.h"
#include "parser.h"
#include "configboxes.h"

enum PresentationOutput{
    handout,
    pause
};

using FrameList = std::vector<std::shared_ptr<Frame>>;
class Presentation : public QObject
{
    Q_OBJECT
public:
    Presentation();
    void loadInput(QString configFilename);
    FrameList frames() const;
    void updateFrames(QByteArray doc);

    bool empty() const;
    int size() const;

    std::shared_ptr<Frame> frameAt(int pageNumber) const;
    void setBox(QString boxId, BoxGeometry rect, int pageNumber);
    std::shared_ptr<Box> getBox(QString id) const;
    std::shared_ptr<Frame> getFrame(QString id) const;
    void saveConfig(QString file);
    void setPresentationOutput(PresentationOutput output);
signals:
    void presentationChanged();
    void frameChanged(int pageNumber);
private:
    FrameList mFrames;
    QString mInputDir;
    Parser mParser;
    ConfigBoxes mConfig;
    PresentationOutput mPresentationOut = PresentationOutput::handout;
};

#endif // PRESENTATION_H
