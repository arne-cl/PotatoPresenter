#include "pdfcreator.h"
#include "sliderenderer.h"

#include <QPdfWriter>

PDFCreator::PDFCreator()
{
}


void PDFCreator::createPdf(QString filename, std::shared_ptr<Presentation> presentation) const{
    QPdfWriter pdfWriter(filename);
    auto const pdfLayout = QPageLayout(QPageSize(QSize(160, 90)), QPageLayout::Portrait, QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
    pdfWriter.setPageLayout(pdfLayout);

    QPainter painter(&pdfWriter);
    painter.setWindow(QRect(QPoint(0, 0), presentation->dimensions()));

    painter.begin(&pdfWriter);
    auto paint = std::make_shared<SlideRenderer>(painter);
    for(auto &slide: presentation->slideList().vector){
        for( int i = 0; i < slide->numberPauses(); i++) {
            paint->paintSlide(slide, i);
            if(!(slide == presentation->slideList().vector.back() && i == slide->numberPauses() - 1)){
                pdfWriter.newPage();
            }
        }
    }
    painter.end();
}
