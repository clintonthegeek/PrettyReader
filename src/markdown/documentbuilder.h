#ifndef PRETTYREADER_DOCUMENTBUILDER_H
#define PRETTYREADER_DOCUMENTBUILDER_H

#include <QObject>
#include <QStack>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextList>
#include <QTextTable>

#include <md4c.h>

class StyleManager;

class DocumentBuilder : public QObject
{
    Q_OBJECT

public:
    explicit DocumentBuilder(QTextDocument *document,
                             QObject *parent = nullptr);

    bool build(const QString &markdownText);
    void setBasePath(const QString &basePath);
    void setStyleManager(StyleManager *sm);

private:
    // MD4C static callbacks
    static int sEnterBlock(MD_BLOCKTYPE type, void *detail, void *userdata);
    static int sLeaveBlock(MD_BLOCKTYPE type, void *detail, void *userdata);
    static int sEnterSpan(MD_SPANTYPE type, void *detail, void *userdata);
    static int sLeaveSpan(MD_SPANTYPE type, void *detail, void *userdata);
    static int sText(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size,
                     void *userdata);

    // Instance handlers
    int enterBlock(MD_BLOCKTYPE type, void *detail);
    int leaveBlock(MD_BLOCKTYPE type, void *detail);
    int enterSpan(MD_SPANTYPE type, void *detail);
    int leaveSpan(MD_SPANTYPE type, void *detail);
    int onText(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size);

    // Helpers
    void ensureBlock();
    QString extractAttribute(const MD_ATTRIBUTE &attr);
    QString resolveEntity(const QString &entity);

    // Default format builders
    QTextBlockFormat headingBlockFormat(int level);
    QTextCharFormat headingCharFormat(int level);
    QTextBlockFormat bodyBlockFormat();
    QTextBlockFormat codeBlockBlockFormat();
    QTextCharFormat codeBlockCharFormat();
    QTextBlockFormat blockQuoteBlockFormat(int level);
    QTextCharFormat blockQuoteCharFormat();

    // Style helpers
    void applyParagraphStyle(const QString &styleName);
    void applyCharacterStyle(const QString &styleName);

    // Footnote handling
    void extractFootnotes(const QString &markdownText);
    void appendFootnotes();

    // State
    QTextDocument *m_document;
    QTextCursor m_cursor;
    QString m_basePath;
    StyleManager *m_styleManager = nullptr;

    // Tracking
    QStack<QTextCharFormat> m_charFormatStack;
    QStack<QTextList *> m_listStack;
    QTextTable *m_currentTable = nullptr;
    int m_tableRow = 0;
    int m_tableCol = 0;
    int m_blockQuoteLevel = 0;
    bool m_isFirstBlock = true;
    bool m_inCodeBlock = false;
    QTextFrame *m_codeFrame = nullptr;
    bool m_inTableHeader = false;
    bool m_collectingAltText = false;
    QString m_altText;
    QString m_codeLanguage;
    QString m_imageSrc;
    QString m_imageTitle;

    // Footnotes
    struct Footnote {
        QString label;
        QString text;
    };
    QList<Footnote> m_footnotes;
    int m_footnoteCounter = 0;
};

#endif // PRETTYREADER_DOCUMENTBUILDER_H
