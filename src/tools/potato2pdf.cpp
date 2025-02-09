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
#include "../core/pdfcreator.h"
#include "../core/template.h"

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

    Template::Ptr presentationTemplate = nullptr;
    if (!output.preamble().templateName.isEmpty()) {
        QString templateName = output.preamble().templateName;
        if (!QDir::isAbsolutePath(templateName)) {
            templateName = QFileInfo(inputFile).absolutePath() + "/" + templateName;
        }
        QFile templateFile(templateName + ".potato");
        if (templateFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString templateContent = QString::fromUtf8(templateFile.readAll());
            ParserOutput templateOutput = generateSlides(templateContent.toStdString(), QFileInfo(templateName).absolutePath(), true);
            if (templateOutput.successfull()) {
                auto templatePresentation = std::make_shared<Template>();
                templatePresentation->setConfig(templateName + ".json");
                templatePresentation->setData(templateOutput.slideList());
                presentationTemplate = templatePresentation;
            }
        }
    }
    auto presentationPtr = std::make_shared<Presentation>();
    presentationPtr->setData({output.slideList(), presentationTemplate});

    // Use PDFCreator to generate the PDF
    PDFCreator creator;
    
    // Set up painter and dimensions matching the GUI implementation
    presentationPtr->setConfig({QFileInfo(inputFile).completeBaseName() + ".json"});
    creator.createPdf(outputFile, presentationPtr);
    
    std::cout << "Successfully created PDF: " << outputFile.toStdString() << "\n";

    return 0;
}
