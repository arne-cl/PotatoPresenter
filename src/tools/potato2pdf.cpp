
/*
    SPDX-FileCopyrightText: 2020-2021 Theresa Gier <theresa@fam-gier.de>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QFileInfo>

#include "version.h"
#include "presentation.h"
#include "configboxes.h"
#include "template.h"
#include "utils.h"
#include "pdfcreator.h"
#include "parser.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("potato2pdf");
    QGuiApplication::setApplicationVersion(PROJECT_VER);

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

    // Load template from preamble
    std::shared_ptr<Template> presentationTemplate = nullptr;
    auto const preamble = parserOutput.preamble();
    if (!preamble.templateName.isEmpty()) {
        QString templatePath = preamble.templateName;
        if (!QDir::isAbsolutePath(templatePath)) {
            templatePath = directory + "/" + templatePath;
        }
        
        // Load template file
        QFile templateFile(templatePath + ".potato");
        if (!templateFile.open(QIODevice::ReadOnly)) {
            qCritical().noquote() << "Error loading template:" << templatePath;
            return 1;
        }
        
        // Load template config
        ConfigBoxes templateConfig(templatePath + ".json");
        if (!templateConfig) {
            qCritical().noquote() << "Error loading template config:" << templatePath;
            return 1;
        }

        presentationTemplate = std::make_shared<Template>();
        presentationTemplate->setConfig(*templateConfig);
    }

    // Create presentation with template
    auto presentation = std::make_shared<Presentation>();
    try {
        presentation->setData({parserOutput.slideList(), presentationTemplate});
        presentation->setConfig(ConfigBoxes(jsonFileName(inputFile)));
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
