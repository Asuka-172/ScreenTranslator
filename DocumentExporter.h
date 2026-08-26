#ifndef DOCUMENTEXPORTER_H
#define DOCUMENTEXPORTER_H

#include "Document.h"

class DocumentExporter
{
public:
    enum class Format { Txt, Docx, Pdf };

    static bool write(const Document& doc, const QString& filePath, Format fmt, QString* err = nullptr);
};

#endif
