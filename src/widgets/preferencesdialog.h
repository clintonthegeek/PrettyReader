#ifndef PRETTYREADER_PREFERENCESDIALOG_H
#define PRETTYREADER_PREFERENCESDIALOG_H

#include <KConfigDialog>

class PrettyReaderConfigDialog : public KConfigDialog
{
    Q_OBJECT

public:
    explicit PrettyReaderConfigDialog(QWidget *parent);
};

#endif // PRETTYREADER_PREFERENCESDIALOG_H
