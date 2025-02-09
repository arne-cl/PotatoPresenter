
/*
    SPDX-FileCopyrightText: 2020-2021 Theresa Gier <theresa@fam-gier.de>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QDebug>
#include <QFileInfo>

#include "presentation.h"
#include "pdfcreator.h"
#include "parser.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("potato2pdf");
    QCoreApplication::setApplicationVersion(PROJECT_VER);

    // Configure command line parser
    QCommandLineParser parser;
    parser.setApplicationDescription("Potato Presenter PDF Generator CLI");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("input", "Input .potato file");
    parser.addOption(QCommandLineOption("output", "Output PDF file", "file"));
    parser.addOption(QCommandLineOption("handout", "Generate handout version"));
    parser.process(app);

    // Validate input
    const QStringList args = parser.positionalArguments();
    if (args.size() != 1) {
        qCritical().noquote() << "Error: Missing input file";
        return 1;
    }

    const QString inputFile = args[0];
    if (!QFile::exists(inputFile)) {
        qCritical().noquote() << "Error: Input file not found:" << inputFile;
        return 1;
    }

    // Set output filename
    QString outputFile = parser.value("output");
    if (outputFile.isEmpty()) {
        outputFile = QFileInfo(inputFile).completeBaseName();
        outputFile += parser.isSet("handout") ? "_handout.pdf" : ".pdf";
    }

    // Read and parse input file
    QFile file(inputFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical().noquote() << "Error: Could not open file:" << inputFile;
        return 1;
    }

    const auto contents = file.readAll();
    const auto directory = QFileInfo(inputFile).absolutePath();
    const auto parserOutput = generateSlides(contents.toStdString(), directory);

    if (!parserOutput.successfull()) {
        const auto error = parserOutput.parserError();
        qCritical().noquote() << "Line" << error.line + 1 << ":" << error.message;
        return 1;
    }

    // Create presentation
    auto presentation = std::make_shared<Presentation>();
    try {
        presentation->setData({parserOutput.slideList(), nullptr});
    }  catch (const PorpertyConversionError& error) {
        qCritical().noquote() << "Line" << error.line + 1 << ":" << error.message;
        return 1;
    }

    // Generate PDF
    try {
        PDFCreator creator;
        if (parser.isSet("handout")) {
            creator.createPdfHandout(outputFile, presentation);
        } else {
            creator.createPdf(outputFile, presentation);
        }
    } catch (const std::exception& e) {
        qCritical().noquote() << "PDF generation failed:" << e.what();
        return 1;
    }

    qInfo().noquote() << "Successfully generated:" << outputFile;
    return 0;
}
