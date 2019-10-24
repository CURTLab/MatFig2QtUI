#ifndef QCONVERTFIG_H
#define QCONVERTFIG_H

#include <QXmlStreamWriter>

#include "QMatIO.h"

class QConvertFig
{
public:
	QConvertFig(QString fileName);
	virtual ~QConvertFig();

	bool convert();

	QString outputFileName() const;

private:
	struct Widget;

	void writeAttribute(QXmlStreamWriter &xml, QString name, QVariant var) const;
	void writeAttributeEnum(QXmlStreamWriter &xml, QString name, QString var) const;
	void writeProperty(QXmlStreamWriter &xml, QString name, QVariant var) const;
	void writePropertyEnum(QXmlStreamWriter &xml, QString name, QString className, QString var) const;
	void writePropertySet(QXmlStreamWriter &xml, QString name, QString className, QStringList var) const;
	void writeWidget(QXmlStreamWriter &xml, Widget *widget) const;
	QRect position(const QMatStruct &properties, const QFont &font) const;
	QPixmap cdataToPixmap(const QMatVar &var) const;
	Widget *parseWidget(QMatStruct &var, size_t i, const QFont &font);

	QString m_fileName;
	QString m_outputFile;
	int m_height;

};

#endif // CONVERTFIG_H
