/*
    SPDX-FileCopyrightText: 2025 Arne Neumann <potato.programming@arne.cl>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QPdfWriter>
#include <QPainter>
#include <iostream>

#include "../core/parser.h"
#include "../core/presentation.h"
#include "../core/sliderenderer.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("potato2pdf");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Convert .potato files to PDF");
    parser.addHelpOption();
    parser.addVersionOption();

    // Add input file argument
    parser.addPositionalArgument("input", "Input .potato file");
    
    // Add output file option
    QCommandLineOption outputOption(QStringList() << "o" << "output",
            "Output PDF file (default: same as input with .pdf extension)",
            "output-file");
    parser.addOption(outputOption);

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        std::cerr << "Error: Must provide input file\n";
        parser.showHelp(1);
    }

    // Get input and output files
    QString inputFile = args.first();
    QString outputFile = parser.value(outputOption);
    if (outputFile.isEmpty()) {
        outputFile = QFileInfo(inputFile).completeBaseName() + ".pdf";
    }

    // Read input file
    QFile file(inputFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Error: Cannot open input file: " << inputFile.toStdString() << "\n";
        return 1;
    }

    QString inputText = QString::fromUtf8(file.readAll());
    QString directory = QFileInfo(inputFile).absolutePath();

    // Parse the potato file
    ParserOutput output = generateSlides(inputText.toStdString(), directory);
    if (!output.successfull()) {
        std::cerr << "Error parsing file: " << output.parserError().message.toStdString() 
                  << " at line " << output.parserError().line << "\n";
        return 1;
    }

    // Create presentation
    Presentation presentation;
    presentation.setData(PresentationData(output.slideList()));

    // Create PDF writer with same settings as PDFCreator
    QPdfWriter writer(outputFile);
    writer.setPageSize(QPageSize(QSizeF(167.0625, 297), QPageSize::Millimeter));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setPageMargins(QMargins(0, 0, 0, 0));
    writer.setTitle(presentation.title());

    // Create painter and start painting
    QPainter painter(&writer);
    painter.begin(&writer);
    
    // Set window to match presentation dimensions
    painter.setWindow(QRect(QPoint(0, 0), presentation.dimensions()));
    
    // Create slide renderer with vector hints
    auto renderer = std::make_shared<SlideRenderer>(painter);
    renderer->setRenderHints(static_cast<PresentationRenderHints>(
        static_cast<int>(TargetIsVectorSurface) | 
        static_cast<int>(NoPreviewRendering)
    ));

    // Render each slide
    for(auto &slide: presentation.data().slideListDefaultApplied().vector) {
        for(int i = 0; i <= slide->numberPauses(); i++) {
            renderer->paintSlide(slide, i);
            if(!(slide == presentation.data().slideListDefaultApplied().vector.back() 
                 && i == slide->numberPauses())) {
                writer.newPage();
            }
        }
    }
    
    painter.end();
    
    std::cout << "Successfully created PDF: " << outputFile.toStdString() << "\n";

    return 0;
}
