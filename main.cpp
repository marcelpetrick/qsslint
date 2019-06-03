/*
MIT License

Copyright (c) 2018 Klaralvdalens Datakonsult AB (KDAB)
Author: Sergio Martins <sergio.martins@kdab.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFileInfo>
#include <QtGui/private/qcssparser_p.h>
#include <QWidget>


static QtMessageHandler s_originalMessageHandler;
static bool s_warningsDetected = false;

static const char *const s_recommendationText =
R"(

NOTE: Avoid using Qt style sheets. Qt style sheets are pratically unmaintained and are being considered for deprecation or removal in Qt 6.
Often they aren't flexible enough to implement complex styles and you only realize that when it's too late to rewrite with QStyle.
Mixing style sheets with proxy styles would solve the above but it's currently not well supported (see https://codereview.qt-project.org/#/c/218791/ for proposed solution).
See also QTBUG-68671 for less drastic Qt 6 plans.
)";


void noWarningsMessageHandler(QtMsgType t, const QMessageLogContext &context, const QString &msg)
{
    // This is used in silent mode.
    if (t == QtWarningMsg)
        s_warningsDetected = true;

    s_originalMessageHandler(t, context, msg);
}

static bool lintStyleSheet(const QString &css, bool isFile, bool syntaxOnly, bool verboseErrorMessage = false)
{
    QCss::Parser parser(css, isFile);
    QCss::StyleSheet ss;

    s_originalMessageHandler = qInstallMessageHandler(noWarningsMessageHandler);
    s_warningsDetected = false;
    const bool success = parser.parse(&ss) && (!s_warningsDetected || syntaxOnly);
    qInstallMessageHandler(s_originalMessageHandler);

    if (success) {
        return true;
    } else {
        if (isFile)
        {
            qWarning().noquote() << QStringLiteral("Invalid stylesheet for %1").arg(css);
        }
        else
        {
            qWarning().noquote() << "Invalid stylesheet";
        }

        if(verboseErrorMessage)
        {
            auto const errorSymbol = parser.errorSymbol();
            //            qDebug() << errorSymbol.text; // prints full text of the given QSS, but this is not helpful, since newlines are replaced with \r\n
            //            auto split = errorSymbol.text.split("\r\n");
            //            qDebug() << split;
            qWarning() << "Lexem causing the error: " << errorSymbol.lexem();
            qWarning() << "\tstart:" << errorSymbol.start << "length:" << errorSymbol.len;

            auto const leftString = errorSymbol.text.left(errorSymbol.start);
            auto const midString = errorSymbol.text.mid(errorSymbol.start, errorSymbol.len);
            auto const rightString = errorSymbol.text.mid(errorSymbol.start + errorSymbol.len);

            qWarning() << "original length:" << errorSymbol.text.length();
            qWarning() << "new length:" << leftString.length() << midString.length() << rightString.length();
            qWarning() << "leftString:" << leftString;
            qWarning() << "midString:" << midString;
            qWarning() << "rightString:" << rightString.left(20);
        }

        return false;
    }
}

int main(int argv, char** argc)
{
    QApplication app(argv, argc);
    QCoreApplication::setApplicationName("qsslint");
    QCoreApplication::setApplicationVersion("1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("\nQt stylesheet syntax verifier %1").arg(s_recommendationText));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption syntaxOnlyOption(QStringList() << "s" << "syntax-only", QLatin1String("Only validate syntax, not semantics"));
    parser.addOption(syntaxOnlyOption);
    QCommandLineOption verboseErrorMessageOption(QStringList() << "e" << "verbose-error-message",
                                                 QLatin1String("Report errors with failing lexem, position and context."));
    parser.addOption(verboseErrorMessageOption);

    parser.addPositionalArgument(QLatin1String("files"), QLatin1String("list of qss files to verify"));
    parser.process(app);

    const auto files = parser.positionalArguments();

    if (files.isEmpty()) {
        parser.showHelp(-1);
    }

    const bool isSyntaxOnly = parser.isSet(syntaxOnlyOption);
    const bool verboseErrorMessage = parser.isSet(verboseErrorMessageOption);
    bool success{true};
    for (const QString &filename : files) {
        success &= lintStyleSheet(filename, /*isFile=*/ true, isSyntaxOnly, verboseErrorMessage);
    }

    return success ? 0 : 1;
}
