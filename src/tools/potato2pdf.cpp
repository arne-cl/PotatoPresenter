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

    // Create PDF writer
    QPdfWriter writer(outputFile);
    writer.setPageSize(QPageSize(QSize(1600, 900))); // Default presentation size
    writer.setResolution(96); // Standard screen DPI
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    
    // Create painter for PDF
    QPainter painter(&writer);

    // Scale to fill page while maintaining aspect ratio
    QSizeF pageSize = writer.pageLayout().fullRectPoints().size();
    double scale = std::min(
        pageSize.width() / 1600.0,
        pageSize.height() / 900.0
    );
    painter.scale(scale, scale);
    
    // Create slide renderer with vector hints since we're rendering to PDF
    SlideRenderer renderer(painter);
    renderer.setRenderHints(PresentationRenderHints::TargetIsVectorSurface);
    
    // Get slides to render
    auto const& slides = presentation.slideList();
    
    // Render each slide to a new PDF page
    for (int i = 0; i < slides.numberSlides(); ++i) {
        if (i > 0) {
            writer.newPage();
        }
        
        auto slide = slides.slideAt(i);
        if (!slide) continue;
        
        // Use SlideRenderer to paint the slide
        renderer.paintSlide(slide);
    }
    
    painter.end();
    
    std::cout << "Successfully created PDF: " << outputFile.toStdString() << "\n";

    return 0;
}
